#include <thread>
#include <iostream>
#include <atomic>
#include <functional>
#include <mutex>
#include <chrono>
#include "Timer.hpp"

using namespace std;
mutex l;

int victim = 0;
bool volatile flag[2] = { false, false };

void Lock(int myID)
{
	int other = 1 - myID;
	flag[myID] = true;
	victim = myID;
	atomic_thread_fence(memory_order_seq_cst);
	while (flag[other] && victim == myID);
}
void Unlock(int myID)
{
	flag[myID] = false;
}

int sum = 0;
void f(int tid)
{
	for (int i = 0; i < 12500000; i++)
	{
		//	lock_guard lck(l);
		Lock(tid);
		sum += 2;
		Unlock(tid);
	}
}

int main()
{
	timer::reset();
	thread t1(f, 0);
	thread t2(f, 1);
	t1.join();
	t2.join();
	cout << sum << " ";
	timer::elapsed("[ ", "]\n");
}



