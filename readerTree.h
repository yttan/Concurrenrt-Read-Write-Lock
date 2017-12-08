// readerwriterlock.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include <atomic>
#include <thread>
#include <cstdlib>
//#include <model-assert.h>
#include <vector>
#include <iostream>
#include <stdio.h>
#include <math.h>
using namespace std;


class readerTree {
public:

	/*inner class of node*/
	class Node {
	public:
		Node(Node* myParent, int count, bool tSense) {
			this->children = count;
			this->childCount.store(0, memory_order_relaxed);
			this->parent = myParent;
			this->threadSense = tSense;
		}

		void await(readerTree* t) {
			bool mySense = threadSense;
			if (this->parent != NULL)this->parent->childJoin();
			while (t->sense.load(memory_order_acquire) != mySense) {}; //spin until sense change
			if (this->parent != NULL) this->parent->childDone();
			this->threadSense = !mySense;
		}



		//update the no of current children
		void childDone() {
			while (true) {
				int old = this->childCount.load(memory_order_acquire);
				//decrement the counter 
				if (old >= 1) {
					if (childCount.compare_exchange_weak(old, old - 1, memory_order_release, memory_order_relaxed)) {
						if (old == 1 && this->parent != NULL) {
							this->parent->childDone();
						}
						return;
					}
				}
			}
		}


		//update the no of current children
		void childJoin() {
			bool succ = false;
			this->undoArr = -1;
			while (!succ) {
				int old = this->childCount.load(memory_order_acquire); //snapshot of childcount
																	   //if it is the first child node, update to 1
				if (old == 0) {
					if (this->childCount.compare_exchange_weak(old, -99, memory_order_release, memory_order_relaxed)) {
						old = -99;//new snapshot of childcount
						this->childCount.compare_exchange_weak(old, 1, memory_order_release, memory_order_relaxed);
						succ = true;
					}
				}
				if (old >= 1) {
					if (this->childCount.compare_exchange_weak(old, old + 1, memory_order_release, memory_order_relaxed)) {
						succ = true;
						
					}
				}
				if (old == -99) {
					if (this->parent != NULL) {
						this->parent->childJoin();
						if (!childCount.compare_exchange_weak(old, 1, memory_order_release, memory_order_relaxed)) {
							undoArr = undoArr + 1;

						}
						//else succ = true;
					}
					else {
						childCount.compare_exchange_weak(old, 1, memory_order_release, memory_order_relaxed);
					}
				}
				if ((old != -99) && old < 0 ) {
					cout << "old: " << old << "error occur in childJoin() " << this << endl;
				}
			}

			//Undo the unnecessary update
			if (this->parent != NULL) {
				while (undoArr > 0) {
					this->parent->childDone();
					undoArr -= 1;
				}
			}


		}

		//retrun parent
		Node* getParent() {
			return this->parent;
		}

		//return threadSense
		bool getThreadSense() {
			return this->threadSense;
		}

		//return the latest no of current children.
		int getChildCount() {
			return this->childCount.load(memory_order_relaxed);
		}
	private:
		Node* parent; // parent node of this node
		int children; // Max. of children
		atomic<int> childCount;  //the number of child that currently waiting in the tree
		bool threadSense; // indicate the reader leave or not
						  //bool succ;   //indicate the update to parent is succ or not.
		int undoArr; // steps needed to undo
	};

	/*readerTree */
	//constructor
	readerTree(int size, int myRadix) {
		this->radix = myRadix;
		this->nodes.store(0, memory_order_relaxed);
		int depth = 0;
		while (size > 1) {
			depth++; // squence 
			size = size / this->radix;
		}
		this->node.resize((int)((pow(this->radix, depth + 1) - 1) / (this->radix - 1)));
		build(NULL, depth);
		this->sense.store(false, memory_order_relaxed);
		this->threadSense = !sense.load(memory_order_relaxed);
	}


	//recursive tree constructor
	void build(Node* p, int depth) {
		if (depth == 0) {
			node.at(nodes.load(memory_order_relaxed)) = new Node(p, 0, threadSense);
			nodes++;
		}
		else {
			Node* myNode = new Node(p, radix, threadSense);
			node.at(nodes.load(memory_order_relaxed)) = myNode;
			nodes++;
			for (int i = 0; i < radix; i++) {
				build(myNode, depth - 1);
			}
		}

	}

	//check if all tree nodes gone
	bool isEmpty() {
		if (node.at(0)->getChildCount() == 0 && !node.at(0)->getThreadSense()) return true;
		else return false;
	}



	//StaticTreeBarrier await
	void addReader(int id) {
		// start waiting in the tree.
		node.at(id)->await(this); 
	}

	//tell all children they can leave at anytime.
	void setSense() {
		sense.store(true, memory_order_release);
	}


protected:
	int radix;	// Each node's number of children.
	vector<Node*> node;	// vector for storaging all thread.
	atomic<unsigned int> nodes; //total nodes
	atomic<bool> sense; //Global sense
	bool threadSense = true; //initial status of each node
};


