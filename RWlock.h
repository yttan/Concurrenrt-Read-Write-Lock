#include <cstddef>
#include <atomic>
//#include <thread>
#include <iostream>
#include <atomic>
#include <vector>

using namespace std;

class ReaderTree {
	//   inner class Node
	class Node {
	private:
		Node* parent; // parent node of this node
					  //atomic<int> childCount; // total count of all children
		bool threadSense; // identify if the thread has left or not
						  //ReaderTree& tree;// a reference of current tree barrier

	public:
		Node(Node* myParent) {
			this->parent = myParent;
			this->threadSense = false;

		}
		~Node() {
			if (threadSense) {
				//free(parent);
			}
		}
		// wait for safe to leave
		void await() {
			if (parent != NULL) {
				while (parent->threadSense != true) {};
			}
		}

		void setThreadSense() {
			this->threadSense = true;
		}

		bool getThreadSense() {
			return threadSense;
		}

	};
	atomic<bool> sense; // tell the reader in this tree can start to leave.
private:
	atomic<int> size;  // current size of the tree
	vector<Node*> node; // queue for reader node
						//int radix;  // Each node's number of children
	atomic<int> nodes;   // total readers in the tree
						 //bool threadSense;
	Node* root;

public:
	//constructor
	ReaderTree() {
		size.store(0, memory_order_relaxed);
		this->nodes = 0;
		//int depth = 0;
		// 1 layer tree structure
		//while (size > 1){
		//	depth++;
		//	this->size = size;
		//	this->radix = size-1;
		//}
		this->node.push_back(new Node(NULL));
		//build(root, depth);
		this->sense.store(false, memory_order_relaxed);
		//this->threadSense = !this->sense.load(memory_order_relaxed);
	}


	//recursive tree constructor for reader
	/*void build(Node* parent, int depth){
	if(depth == 0){
	Node* tmp = new Node(parent, 0, this->threadSense, this);
	this->node.push(tmp);
	nodes++;
	}
	else {
	Node* myNode = new Node(parent, this->radix, this->threadSense, this);
	for (int i =0; i < this->radix; i++){
	build(myNode, depth -1);
	}
	}
	}*/

	~ReaderTree() {
		//	for (int i = 0; i < nodes; i++) {
		//	node[i]->~Node();
		//free(node[i]);
		//}
	}

	//add a node, return its id.
	int addReader() {
		Node* tmp = new Node(node[0]);
		this->node.push_back(tmp);
		this->nodes.fetch_add(1, memory_order_relaxed);
		this->size.fetch_add(1, memory_order_relaxed);
		return this->nodes.load(memory_order_relaxed);
	}

	//Reader await.  after finishing await, it leaves the ReaderTree
	void await(int id) {
		this->node[id]->await();
		this->node[id]->setThreadSense();
		this->size.fetch_sub(1, memory_order_acquire);
	}


	bool isEmpty() {
		if (this->size.load(memory_order_relaxed) == 0) return true;
		else return false;
	}

	void setSense() {
		//	sense.store(true, memory_order_release);
		this->node[0]->setThreadSense();
	}


};



class RWnode {                    // this class defines both writer node and reader tree node
public:
	bool iswriter = false;
	ReaderTree* rtree = NULL;
	int reader_id = 0;
	int writer_id = 0;
public:
	RWnode() {
	}
	void constructtree() {
		rtree = new ReaderTree();
	}
	void addreader() {
		rtree->addReader();
	}
	bool tree_empty() {
		if (rtree->isEmpty()) {
			return true;
		}
		else {
			return false;
		}
	}

	bool operator==(const RWnode& rhs) const
	{
		return (iswriter == rhs.iswriter) && (rtree == rhs.rtree) && (reader_id == rhs.reader_id) && (writer_id == rhs.writer_id);
	}

};



class RWQueue {           // this class defines the R/W queue

	class QueueNode {
	public:
		QueueNode() {
			nextitem = NULL;
			item = NULL;
		}
		QueueNode(RWnode* outitem) {

			item = outitem;
		}
		RWnode* getItem() {
			return item;
		}

		QueueNode* getNext() {
			return nextitem;
		}

		void setNext(QueueNode* next) {
			nextitem = next;

			//nextitem.store(next,memory_order_acq_rel);
		}
	public:
		QueueNode* nextitem;
		RWnode* item;
	};

public:
	RWQueue() {
		first = new QueueNode();
		last = new QueueNode();
		size.store(0, memory_order_relaxed);
	}

	void enqueue(RWnode* o) {
		QueueNode* newnode = new QueueNode(o);
		//QueueNode* temp = (QueueNode*)last;
		last.load(memory_order_relaxed)->setNext(newnode);
		//last=last->getNext();
		last.store(last.load(memory_order_acquire)->getNext(), memory_order_release);
		size.fetch_add(1, memory_order_acquire);
	}

	RWnode* dequeue() {
		QueueNode* nodewithitem = first.load(memory_order_acquire)->getNext();
		//QueueNode* temp = first;
		//QueueNode* nodewithitem = temp->getNext();
		RWnode * objecttoreturn = nodewithitem->getItem();
		size.fetch_sub(1, memory_order_release);
		//if (first->getItem().iswriter){
		//  free(first->getItem());                        // free the memory
		//}
		//else{
		//  first->getItem().rtree.~ReaderTree();           // free the memory
		//  free(first->getItem())；
		//}

		//first = nodewithitem;
		first.store(nodewithitem, memory_order_release);
		return objecttoreturn;
	}

	RWnode* getfront() {                  // get the first item
		QueueNode* nodewithitem = first.load(memory_order_relaxed)->getNext();
		return nodewithitem->getItem();
	}
	RWnode* getlast() {
		QueueNode* nodewithitem = last.load(memory_order_relaxed)->getNext();
		return nodewithitem->getItem();
	}
	bool isfront(RWnode* mynode) {
		return first.load(memory_order_relaxed)->getItem() == mynode;
	}

	int getsize() {
		return size.load(memory_order_relaxed);
	}

	bool isEmpty() {
		return size.load(memory_order_relaxed) == 0;
	}
protected:
	atomic<QueueNode*> first;
	atomic<QueueNode*> last;
	atomic<int> size;

};


class RWlock {
protected:
	RWQueue* myqueue;
	atomic<bool> flag; // if a tree is constructed successfully, then raise the flag
public:
	RWlock() {
		flag.store(false, memory_order_relaxed);
		myqueue = new RWQueue();
	}
	void read_lock(RWnode* myreader) {

		if (myqueue->isEmpty()) {                  // is the first node
			myreader->constructtree();
			flag.store(true, memory_order_release);  // raise flag after construction
			myreader->addreader();
			myqueue->enqueue(myreader);
		}
		else {                                // there are nodes in the queue
			if (myqueue->getlast()->iswriter) {   // find out the last one in the queue is a writer
				myreader->constructtree();       // need a new tree
				flag.store(true, memory_order_relaxed); // raise flag after construction
				myreader->addreader();
				myqueue->enqueue(myreader);
			}
			else {                                            // the last one is a reader tree. Note: the construction may not be done yet, need a flag
				while (!flag.load(memory_order_acquire)) {} // if another reader has not done the construction, then wait for the tree to be constructed
				myqueue->getlast()->addreader();
			}
		}
		while (!myqueue->isfront(myreader)) {}     // is not front, can not do the job, spin
	}
	void write_lock(RWnode* mywriter) {
		flag.store(false, memory_order_release);   // a new writer comes,so reset the flag to construct a new tree
		myqueue->enqueue(mywriter);
		while (!myqueue->isfront(mywriter)) {}
	}
	void read_unlock(RWnode* myreader) {
		if (myqueue->getfront()->tree_empty()) {
			myqueue->dequeue();
		}  // thread not done, spin
	}
	void write_unlock(RWnode* mywriter) {
		myqueue->dequeue();
	}
};


