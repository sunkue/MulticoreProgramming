/*
*	2016182024 À±¼±±Ô
*	CAS lock algorithm
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

namespace timer {
	using clk = std::chrono::high_resolution_clock;
	using namespace std::chrono;
	using namespace std::literals::string_view_literals;
	clk::time_point static thread_local StopWatch;
	inline void reset() { StopWatch = clk::now(); }
	inline void elapsed(const std::string_view header = ""sv, const std::string_view footer = "\n"sv) {
		std::cout << header << duration_cast<milliseconds>(clk::now() - StopWatch) << footer;
	}
}

using namespace std;

template <class T> concept AtomicableType = atomic<T>::is_always_lock_free;

template <AtomicableType T> bool CAS(atomic<T>* addr, T expected, T newValue) {
	return atomic_compare_exchange_strong(addr, &expected, newValue);
}

template <AtomicableType T> bool CAS(volatile T* addr, T expected, T newValue) {
	return atomic_compare_exchange_strong(
		reinterpret_cast<volatile atomic<T> *>(addr),
		&expected, newValue);
}

class CASLock
{
	volatile bool Locked = false;
public:
	void lock() {
		while (!CAS(&Locked, false, true));
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


