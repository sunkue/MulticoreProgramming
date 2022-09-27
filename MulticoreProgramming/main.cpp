/*
*	2016182024 À±¼±±Ô
*	
*	c++ 20, x64, Release
*/

#include <iostream>
#include <thread>
#include <vector>
#include <array>
#include <algorithm>
#include <mutex>
#include <ranges>
#include <functional>
#include <iostream>
#include <chrono>

#include "CAS.hpp"
#include "Timer.hpp"

using namespace std;

class CASLock
{
	volatile bool Locked = false;
public:
	void lock() {
		while (!CAS::CAS(&Locked, false, true));
	}
	void unlock() {
		Locked = false;
	}
};

namespace MutableLock {
	function<void()> lock;
	function<void()> unlock;
}

mutex m;
CASLock casLock;

int volatile sum = 0;

void Func(int threadNum) {
	for (int i = 0; i < 500'0000 / threadNum; i++) {
		MutableLock::lock();
		sum += 2;
		MutableLock::unlock();
	}
}

int main() {
	auto DoJob = []() {
		for (int threadNum = 1; threadNum <= 8; threadNum *= 2) {
			sum = 0; timer::reset();
			vector<thread> threadPool; threadPool.reserve(threadNum);
			for (int i = 0; i < threadNum; i++) threadPool.push_back(move(thread(Func, threadNum)));
			for (auto& t : threadPool) t.join();
			timer::elapsed("[" + to_string(threadNum) + " Threads] [", "]\t");
			cout << "SUM::[" << sum << "]" << endl;
		}
	};

	cout << "\t<< None Lock >>" << endl;
	MutableLock::lock = []() {};
	MutableLock::unlock = []() {};
	DoJob();

	cout << "\t<< std::mutex Lock >>" << endl;
	MutableLock::lock = []() { m.lock(); };
	MutableLock::unlock = []() { m.unlock(); };
	DoJob();

	cout << "\t<< CAS Lock >>" << endl;
	MutableLock::lock = []() { casLock.lock(); };
	MutableLock::unlock = []() { casLock.unlock(); };
	DoJob();
}


