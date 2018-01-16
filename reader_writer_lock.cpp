
#include <pthread.h>
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

#define NUMBER_READERS  32
#define NUMBER_WRITERS  2
#define RADIX  4



class readerTree {
public:

	/*inner class of node*/
	class Node {
	public:
		Node(Node* myParent, int count) {
			this->children = count;
			this->childCount.store(0, memory_order_relaxed);
			this->parent = myParent;
			this->threadSense = 0;
		}
		~Node() {
			delete this->parent;

		}


		void await(readerTree* t) {
			this->threadSense = 1;
			if (this->parent != NULL)this->parent->childJoin();
			while (t->sense.load(memory_order_acquire) != true) {}; //spin until sense change
			if (this->parent != NULL) this->parent->childDone();
			this->threadSense = 2;
		}


		void rootAwait() {
			this->threadSense = 1;
			if (this->parent != NULL)this->parent->childJoin();
		}

		void rootDepart() {

			if (this->parent != NULL) this->parent->childDone();
			this->threadSense = 2;
		}





		//update the no of current children
		void childDone() {
			while (true) {
				int old = this->childCount.load(memory_order_acquire);
				//decrement the counter 
				if (old == 0) return;
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
		int getThreadSense() {
			return this->threadSense;
		}

		//return the latest no of current children.
		int getChildCount() {
			return this->childCount.load(memory_order_acquire);
		}
	private:
		Node * parent; // parent node of this node
		int children; // Max. of children
		atomic<int> childCount;  //the number of child that currently waiting in the tree
		int threadSense; // indicate the reader 0:empty, 1:exist , 2:leave
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
	}


	//recursive tree constructor
	void build(Node* p, int depth) {
		if (depth == 0) {
			node.at(nodes.load(memory_order_relaxed)) = new Node(p, 0);
			nodes++;
		}
		else {
			Node* myNode = new Node(p, radix);
			node.at(nodes.load(memory_order_relaxed)) = myNode;
			nodes++;
			for (int i = 0; i < radix; i++) {
				build(myNode, depth - 1);
			}
		}

	}

	//check if all tree nodes gone
	bool isEmpty() {
		if (node.at(0)->getChildCount() == 0 && node.at(0)->getThreadSense() != 1) return true;
		else return false;
	}


	//StaticTreeBarrier await
	void addReader(int id) {
		node.at(id)->await(this); // start waiting in the tree.
	}

	void addRoot(int id) {
		node.at(id)->rootAwait(); // start waiting in the tree.
	}
	void removeRoot(int id) {
		node.at(id)->rootDepart();
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
		rtree = new readerTree(NUMBER_READERS, RADIX);   
	}
	//add reader to a tree
	void addreader(bool isroot) {
		if (!isroot) {
			rtree->addReader(reader_id);
		}
		else {
			rtree->addRoot(reader_id);
		}
	}

	void removeRoot() {
		if (isroot) {
			rtree->removeRoot(reader_id);
		}
	}
	
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
			nextitem.store(NULL, memory_order_relaxed);
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
			return nextitem.load(memory_order_acquire);
		}
		// set the next pointer
		void setNext(QueueNode* next) {
			//nextitem.store(next, memory_order_release);
			bool succ = false;
			QueueNode* old = nextitem.load(memory_order_relaxed);
			while (!succ) {
				if (nextitem.compare_exchange_weak(old, next, memory_order_release, memory_order_relaxed)) {
					succ = true;
				}
				else {
					old = nextitem.load(memory_order_acquire);
				}
			}
		}
	public:
		atomic<QueueNode*> nextitem;
		RWnode * item;
	};

public:
	RWQueue() {
		first.store(new QueueNode(), memory_order_relaxed);
		last.store(first.load(memory_order_relaxed), memory_order_relaxed);
		size.store(0, memory_order_relaxed);
	}
	// enqueue a node
	void enqueue(RWnode* o) {
		QueueNode* newnode = new QueueNode(o);
		bool succ1 = false;
		bool succ2 = false;
		// empty queue
		while (!succ1 || !succ2) {
			if (first.load(memory_order_relaxed)->getItem() == NULL && last.load(memory_order_relaxed)->getItem() == NULL) {
				//first.load(memory_order_relaxed)->setNext(newnode);
				QueueNode* old = first.load(memory_order_acquire);
				if (last.compare_exchange_weak(old, newnode, memory_order_release, memory_order_relaxed)) {
					succ2 = true;

					while (!first.compare_exchange_weak(old, newnode, memory_order_release, memory_order_relaxed)) {
						old = newnode->getNext();


					}
					size.fetch_add(1, memory_order_release);
					succ1 = true;
				}


			}
			// not empty queue
			else {
				QueueNode* old = last.load(memory_order_acquire);
				if (old->getItem() != NULL) {
					old->setNext(newnode);
					//last.store(last.load(memory_order_relaxed)->getNext(), memory_order_relaxed);
					if (last.compare_exchange_weak(old, old->getNext(), memory_order_release, memory_order_relaxed)) {

						size.fetch_add(1, memory_order_release);
						succ1 = true;
						succ2 = true;
					}
				}
			}
		}
	}
	// dequeue a node
	RWnode * dequeue() {
		bool succ1 = false;
		bool succ2 = false;
		while (!succ1 || !succ2) {
			//only one item in the queue, need to change the last pointer
			if (first.load(memory_order_relaxed) == last.load(memory_order_relaxed)) {
				QueueNode* nodewithitem = first.load(memory_order_relaxed); //the snapshot of first
				RWnode * objecttoreturn = nodewithitem->getItem();
				QueueNode* old = last.load(memory_order_acquire);
				QueueNode* first_old = first.load(memory_order_acquire);

				QueueNode* newnode = new QueueNode();

				succ2 = first.compare_exchange_weak(first_old, newnode, memory_order_release, memory_order_relaxed);
				succ1 = last.compare_exchange_weak(old, newnode, memory_order_release, memory_order_relaxed);
				if (succ1&&succ2) {
					size.fetch_sub(1, memory_order_release);
					return objecttoreturn;
				}
			}
			//multiple items, do not need to consider last pointer
			else {
				QueueNode* nodewithitem = first.load(memory_order_relaxed);
				RWnode * objecttoreturn = nodewithitem->getItem();
				QueueNode* first_old = first.load(memory_order_acquire);
				if (first_old->getNext() != NULL) {
					if (first.compare_exchange_weak(first_old, first_old->getNext(), memory_order_release, memory_order_relaxed)) {
						size.fetch_sub(1, memory_order_release);
						succ2 = true;
						succ1 = true;
						return objecttoreturn;
					}
				}
			}
		}
	}
	// get the first node in the queue
	RWnode* getfront() {
		if (first.load(memory_order_relaxed) != NULL) {
			QueueNode* nodewithitem = first.load(memory_order_acquire);
			return nodewithitem->getItem();
		}
		else {
			cout << "should not occur: empty queue, can not get front";
		}
	}
	// get the last node in the queue
	RWnode* getlast() {
		if (last.load(memory_order_relaxed) != NULL) {
			QueueNode* nodewithitem = last.load(memory_order_acquire);

			return nodewithitem->getItem();
		}
		cout << "should not occur: can not use get last beacuse queue is empty";
	}
	// check if a node is first
	bool isfront(RWnode* mynode) {
		if (first.load(memory_order_acquire)->getItem() == NULL) {
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
		return size.load(memory_order_acquire);
	}
	// check if the queue is empty
	bool isEmpty() {
		return size.load(memory_order_acquire) == 0;
	}
protected:
	atomic<QueueNode*> first;
	atomic<QueueNode*> last;
	atomic<int> size;

};

// defines lock class
class RWlock {
public:
	RWQueue * myqueue;
	bool end = false;
public:
	RWlock() {

		myqueue = new RWQueue();
	}


	void read_lock(RWnode* myreader) {
		bool succ = false;
		while (!succ) {
			if (myqueue->isEmpty()) {                  // is the first node
													   //if (myqueue->getlast!=NULL) {                  // is the first node
				myreader->isroot = true;

				myreader->constructtree();
				myqueue->enqueue(myreader);
				myreader->addreader(true);
				while (!myqueue->isfront(myreader)) {}
				myreader->rtree->setSense();
				myreader->removeRoot();
				succ = true;
			}
			//not Empty
			else {
				RWnode* now = myqueue->getlast();// there are nodes in the queue
				if (now) {
					if (now->iswriter) {   // find out the last one in the queue is a writer
						myreader->isroot = true;
						myreader->constructtree(); // need a new tree	

						myqueue->enqueue(myreader);
						myreader->addreader(true);
						while (!myqueue->isfront(myreader)) {}
						myreader->rtree->setSense();
						myreader->removeRoot();
						succ = true;

					}
					if (now->isroot) {         // the last one is a reader tree. 					  
						now->addreader(false);
						succ = true;
					}
				}
			}
		}
	}
	void write_lock(RWnode* mywriter) {

		myqueue->enqueue(mywriter);
		while (!myqueue->isfront(mywriter)) {}
	}
	void read_unlock() {

		if (myqueue->getfront() != NULL){
                  if(myqueue->getfront()->rtree != NULL) {
			if (myqueue->getfront()->tree_empty()) {
				myqueue->dequeue();
			}
		}
            }
	}


	void write_unlock() {
		myqueue->dequeue();
	}

	
};




RWlock * mylock;

int var = 0;


void* reader(void* id)
{
	int tid = (long)id;
	RWnode* myreader = new RWnode();
	//printf("\nReader %d constructed ", id);
	myreader->reader_id = tid;
	mylock->read_lock(myreader);
	//printf("\nReader  %d is locked ", id);

	int newvar = var;
	printf("\nReader %d read var: %d ", id, var);
	//cout << "read var:" << newvar << endl;
	mylock->read_unlock();
	//printf("\nReader %d is unlocked ", id);
	return 0;
}

void* writer(void* id)
{
	int tid = (long)id;
	RWnode * mywriter = new RWnode();
	mywriter->iswriter = true;
	mywriter->writer_id = tid;
	mylock->write_lock(mywriter);
	//printf("\nWriter is locked ");
	//	sleep(10);
	var += 1;
	printf("\nWriter %d write var: %d ", id, var);
	mylock->write_unlock();
	//printf("\nWriter is unlocked ");
	return 0;
}
//void manager() {
//	mylock->control();
//}
//



int main()
{


	mylock = new RWlock();

	pthread_t  Reader[NUMBER_READERS];
	pthread_t  Writer[NUMBER_WRITERS];
	int rc;
	//thread create
	for (int i = 0; i < NUMBER_READERS; i++) {
		rc = pthread_create(&Reader[i], NULL, reader, (void*)i);
		if (rc) {
			printf("ERROR:unable to create thread ");
			cout << rc << endl;
			exit(-1);
		}

	}
	for (int i = 0; i < NUMBER_WRITERS; i++) {
		rc = pthread_create(&Writer[i], NULL, writer, (void*)i);
		if (rc) {
			printf("ERROR:unable to create thread ");
			cout << rc << endl;
			exit(-1);
		}

	}

	//thread join
	for (int i = 0; i < NUMBER_READERS; i++) {
		rc = pthread_join(Reader[i], NULL);
		if (rc) {
			printf("ERROR:unable to join thread ");
			cout << rc << endl;
			exit(-1);
		}

	}

	for (int i = 0; i < NUMBER_WRITERS; i++) {
		rc = pthread_join(Writer[i], NULL);
		if (rc) {
			printf("ERROR:unable to join thread ");
			cout << rc << endl;
			exit(-1);
		}

	}


	return 0;
}
