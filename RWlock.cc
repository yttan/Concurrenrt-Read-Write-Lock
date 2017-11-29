#include <stdio.h>
#include <threads.h>
#include <iostream>
#include "RWlock.h"
#include "librace.h"

RWlock * mylock;

int var = 0;


void reader(void *arg)
{
	int* id = (int*)arg;
	RWnode* myreader = new RWnode();
	printf("\nReader %d constructed ", &id);
	mylock->read_lock(myreader);
	printf("\nReader is locked ");
	myreader->reader_id = *id;
	int newvar = var;
	cout << "read var:" << newvar << endl;
	mylock->read_unlock(myreader);
	printf("\nReader is unlocked ");

}

void writer(void *arg)
{
	int* id = (int*)arg;
	RWnode* mywriter = new RWnode();
	mylock->write_lock(mywriter);
	printf("\nWriter is locked ");
	mywriter->writer_id = *id;
	mywriter->iswriter = true;
	var = 1;
	mylock->write_unlock(mywriter);
	printf("\nWriter is unlocked ");

}
int main()
{

	int i = 1;
	int y = 1;
	mylock = new RWlock();
	std::thread R1(reader, &i);
	i++;
	std::thread R2(reader, &i);
	std::thread W1(writer, &y);


	R1.join();
	R2.join();
	W1.join();
	return 0;
}

