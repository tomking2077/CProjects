#ifndef BoundedBuffer_h
#define BoundedBuffer_h

#include <iostream>
#include <queue>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
using namespace std;

class BoundedBuffer
{
private:
  	int cap;
  	queue<vector<char>> q;

	/* mutexto protect the queue from simultaneous producer accesses
	or simultaneous consumer accesses */
	mutex mtx;
	
	/* condition that tells the consumers that some data is there */
	condition_variable data_available;
	/* condition that tells the producers that there is some slot available */
	condition_variable slot_available;

public:
	BoundedBuffer(int _cap):cap(_cap){

	}
	~BoundedBuffer(){

	}

	/* NOT NEEDED
	
	void push(vector<char> data){
		unique_lock<mutex> lock(mtx);

		slot_available.wait(lock, [this]{ return q.size() < cap;} );
		q.push(data);
		data_available.notify_one();

		lock.unlock();
	}
	*/


	void push(char* data, int len){

		unique_lock<mutex> lock(mtx);
		//printf("Locked\n");

		/* 
		while(q.size() >= cap){
			//printf("Push Blocked | Size: %d Lock Released, max capacity\n", q.size());
			slot_available.wait(lock);
		}
		*/
		slot_available.wait(lock, [this]{ return q.size() < cap;} );

		vector<char> v(data, data+len);
		q.push(v);

		data_available.notify_one();
		lock.unlock();
		//printf("Unlocked\n");
	}

	vector<char> pop(){
		vector<char> temp;

		unique_lock<mutex> lock(mtx);
		//printf("Locked\n");

		/* 
		while(q.size() == 0){
			//printf("Pop Blocked | Size: %d Lock Released, nothing to pop\n", q.size());
			data_available.wait(lock);
		}
		*/
		data_available.wait(lock, [this]{ return q.size() > 0;} );

		temp = q.front();
		q.pop();
		slot_available.notify_one();

		lock.unlock();
		//printf("Unlocked\n");
		return temp;  
	}
};

#endif /* BoundedBuffer_ */
