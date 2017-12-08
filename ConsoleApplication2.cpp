
#include "stdafx.h"
#include <thread>
#include <iostream>
#include <cstddef>
#include <atomic>
#include <vector>
#include <atomic>
#include <cstdlib>
#include <vector>
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
				if ((old != -99) && old < 0) {
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
		Node * parent; // parent node of this node
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

	//recursively update parent's childCount.
	void increaseParent(Node* p) {
		if (p != NULL) {
			if (p->getChildCount() == 0) increaseParent(p->getParent()); // update grandparent
			p->childJoin();
		}
	}


	//StaticTreeBarrier await
	void addReader(int id) {

		node.at(id)->await(this); // start waiting in the tree.
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



// defines both writer node and reader/reader tree node
class RWnode {                    
public:
	bool iswriter = false;
	bool isroot = false;
	readerTree* rtree = NULL;
	int reader_id = 0;
	int writer_id = 0;
public:
	RWnode() {
	}
	// construct a tree
	void constructtree() {
		rtree = new readerTree(7,2);    //!!!!!!!!!!!!!!!!!!!!!!!!!!!! to be checked
	}
	//add reader to a tree
	void addreader() {
		rtree->addReader(reader_id);
	}
	// check if my tree is empty, every reader leaves //////!!!!!!!!!!!!!!!!!!!!!!! to be checked
	bool tree_empty() {
		if (rtree->isEmpty()) {
			return true;
		}
		else {
			return false;
		}
	}

};


// defines the Read/Write queue
class RWQueue {           

	class QueueNode {
	public:
		QueueNode() {
			nextitem = NULL;
			item = NULL;
		}
		QueueNode(RWnode* outitem) {

			item = outitem;
		}
		// get RWnode object
		RWnode* getItem() {
			return item;
		}
		// get the next queue node
		QueueNode* getNext() {
			return nextitem;
		}
		// set the next pointer
		void setNext(QueueNode* next) {
			nextitem.store(next,memory_order_relaxed);
		}
	public:
		atomic<QueueNode*> nextitem;
		RWnode * item;
	};

public:
	RWQueue() {
		first = new QueueNode();
		last = new QueueNode();
		size.store(0, memory_order_relaxed);
	}
	// enqueue a node
	void enqueue(RWnode* o) {
		QueueNode* newnode = new QueueNode(o);
		// empty queue
		if (first.load(memory_order_relaxed)->getItem() == NULL ) {
			first.load(memory_order_relaxed)->setNext(newnode);
			first.store(first.load(memory_order_relaxed)->getNext(), memory_order_relaxed);
			//last.load(memory_order_relaxed)->setNext(newnode);
			last.store(first, memory_order_relaxed);
			size.fetch_add(1, memory_order_relaxed);
		}
		// not empty queue
		else {
			last.load(memory_order_relaxed)->setNext(newnode);
			
			last.store(last.load(memory_order_relaxed)->getNext(), memory_order_relaxed);
			size.fetch_add(1, memory_order_relaxed);
		}
	}
	// dequeue a node
	RWnode * dequeue() {
		//only one item in the queue, need to change the last pointer
		if (first == last) {                           
			QueueNode* nodewithitem = first.load(memory_order_relaxed);
			RWnode * objecttoreturn = nodewithitem->getItem();
			size.fetch_sub(1, memory_order_release);
			QueueNode* temp = new QueueNode();
			first.store(temp, memory_order_relaxed);
			last.store(temp, memory_order_relaxed);
			free(nodewithitem);                         //free memory
			return objecttoreturn;
		}
		//multiple items, do not need to consider last pointer
		else {          
			QueueNode* nodewithitem = first.load(memory_order_relaxed);
			RWnode * objecttoreturn = nodewithitem->getItem();
			size.fetch_sub(1, memory_order_release);

			first.store(first.load(memory_order_relaxed)->getNext(), memory_order_relaxed);
			free(nodewithitem);                      //free memory
			return objecttoreturn;
		}
	}
	// get the first node in the queue
	RWnode* getfront() {                  
		if (first.load(memory_order_relaxed) != NULL) {
			QueueNode* nodewithitem = first.load(memory_order_relaxed);
			return nodewithitem->getItem();
		}
		else {
			cout << "should not occur: empty queue, can not get front";
		}
	}
	// get the last node in the queue
	RWnode* getlast() {
		if (last.load(memory_order_relaxed) != NULL) {
			QueueNode* nodewithitem = last.load(memory_order_relaxed);
			
			return nodewithitem->getItem();
		}
		cout << "should not occur: can not use get last beacuse queue is empty";
	}
	// check if a node is first
	bool isfront(RWnode* mynode) {           
		if (first.load(memory_order_relaxed)->getItem() == NULL){
			return false;
		}
		else
		{
			if (first.load(memory_order_relaxed)->getItem() == mynode) {
				return true;
			}
			else {
				return false;
			}
		}
	}
	// get size of the queue
	int getsize() {
		return size.load(memory_order_relaxed);
	}
	// check if the queue is empty
	bool isEmpty() {
		return size.load(memory_order_relaxed) == 0;
	}
protected:
	atomic<QueueNode*> first;
	atomic<QueueNode*> last;
	atomic<int> size;

};

// defines lock class
class RWlock {
protected:
	RWQueue * myqueue;
	
public:
	RWlock() {
		
		myqueue = new RWQueue();
	}

	
	void read_lock(RWnode* myreader) {

		if (myqueue->isEmpty()) {                  // is the first node
			myreader->isroot = true;
			myqueue->enqueue(myreader);
			myreader->constructtree();
			//myreader->rtree->setSense();
			myreader->addreader();
		}
		else {                                // there are nodes in the queue
			if (myqueue->getlast()->iswriter) {   // find out the last one in the queue is a writer
				myreader->isroot = true;
				myqueue->enqueue(myreader);
				myreader->constructtree();       // need a new tree				 
				myreader->addreader();
			}
			else {                                            // the last one is a reader tree. 					  
				myqueue->getlast()->addreader();
			}
		}	
	}
	void write_lock(RWnode* mywriter) {
		
		myqueue->enqueue(mywriter);
		while (!myqueue->isfront(mywriter)) {}
	}
	void read_unlock() {
		
		if (myqueue->getfront()->tree_empty()) { 
			myqueue->dequeue();
		}    
	}


	void write_unlock() {
		myqueue->dequeue();
	}

	void control() {
		while(true){
			if (myqueue->getfront()->isroot) {
				myqueue->getfront()->rtree->setSense();
			}
		}
	
	}
};




RWlock * mylock;

int var = 0;


void reader(void *arg)
{
	int* id = (int*)arg;
	RWnode* myreader = new RWnode();
	printf("\nReader %d constructed ", *id);
	myreader->reader_id = *id;
	mylock->read_lock(myreader);
	printf("\nReader is locked ");
	
	int newvar = var;
	cout << "read var:" << newvar << endl;
	mylock->read_unlock();
	printf("\nReader is unlocked ");

}

void writer(void *arg)
{
	int* id = (int*)arg;
	RWnode * mywriter = new RWnode();
	mywriter->iswriter = true;
	mywriter->writer_id = *id;
	mylock->write_lock(mywriter);
	printf("\nWriter is locked ");
	var += 1;
	cout << "after write var:" << var << endl;
	mylock->write_unlock();
	printf("\nWriter is unlocked ");

}
void manager(void *arg) {
	mylock->control();
}
int main()
{

	int i = 1;
	int y = 1;
	mylock = new RWlock();
	thread Manager(manager, NULL);
	Manager.join();
	std::thread R1(reader, &i);
	R1.join();
	i++;
	//std::thread R2(reader, &i);
	//R2.join();
	std::thread W1(writer, &y);
	W1.join();
	y++;
	std::thread W2(writer, &y);
	W2.join();
	std::thread R3(reader, &i);
	R3.join();
	

	return 0;
}

