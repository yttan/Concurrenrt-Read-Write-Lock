# Concurrenrt-Read-Write-Lock

Author: Yuting Tan, Lai Man Tang  
Date: 12/09/2017

This project is aimed to implement a concurrent reader writer lock using c++11 atomic features.  
This lock has 1 FIFO queue and multiple SNZI trees.    
We implemented 4 clases. They are: readerTree, RWnode, RWlock and RWQueue.  

RWqueue is a queue. All readers and writers go into the queue. For a reader, if it sees the queue is empty or the last item in the queue is a writer, it consturcts a reader tree for the following readers to get into the tree. If the reader sees another reader being the last item in the queue, it joins the reader tree the previous reader constructed. It has a inner class called QueueNode. A QueueNode has a pointer pointing to the next QueueNode and a pointer pointing to the RWnode it stores.   

readerTree is a Tree Structure that stored the incoming reader threads. We used SNZI data structure to ensure the tree is scalable. Also, the tree can resize itself, which means although we set the initial size and radix to static numbers, no matter how many readers come in, we can always resize the tree for them.  

RWnode defines both the readers, writers and reader trees instances.   

RWlock is the lock calss. We can call read_lock(), write_lock(), read_unlock(), write_unlock() functions.  

We used pthread to implement multithreading and run it on a server with C++ 11 environment. We have 32 readers and 2 writers.  

To run it, using this command:    
``
$scl enable devtoolset-7 bash
$gcc reader-writer-locker.cpp -lpthread -lstdc++ -lm
``
