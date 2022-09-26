/*
*	2016182024 À±¼±±Ô
*	bakery algorithm
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

size_t inline static thread_local tid;

class BakeryVector
{
	vector<bool> flags;
	vector<size_t> labels;
public:
	void init(int threadNum) {
		flags.assign(threadNum, false);
		labels.assign(threadNum, 0);
	}
	void lock() {
		flags[tid] = true;
		labels[tid] = *ranges::max_element(labels) + 1;

		for (size_t k = -1; ranges::any_of(labels,
			[&](const auto labelK) {
				k++;
				auto flagK = flags[k];
				auto label = labels[tid];
				// if k == tid then labelCheck always false
				auto sameLabelSmallK = (labelK == label) && (k < tid);
				auto labelCheck = (labelK < label) || sameLabelSmallK;
				return flagK && labelCheck;
			}); k = -1);
	}
	void unlock() {
		flags[tid] = false;
	}
};

class BakeryArray
{
	array<bool, 8> flags;
	array<size_t, 8> labels;
	size_t threadNum = 0;
public:
	void init(int threadNum) {
		this->threadNum = threadNum;
		flags.fill(false);
		labels.fill(0);
	}
	void lock() {
		flags[tid] = true;
		labels[tid] = *ranges::max_element(labels) + 1;

		for (size_t k = -1; ranges::any_of(views::take(labels, threadNum),
			[&](const auto labelK) {
				k++;
				auto flagK = flags[k];
				auto label = labels[tid];
				// if k == tid then labelCheck always false
				auto sameLabelSmallK = (labelK == label) && (k < tid);
				auto labelCheck = (labelK < label) || sameLabelSmallK;
				return flagK && labelCheck;
			}); k = -1);
	}
	void unlock() {
		flags[tid] = false;
	}
};

namespace MutableLock {
	function<void()> lock;
	function<void()> unlock;
}

mutex m;
BakeryVector bakeryVector;
BakeryArray bakeryArray;

int volatile sum = 0;

void Func(int threadNum) {
	for (int i = 0; i < 500'0000 / threadNum; i++) {
		MutableLock::lock();
		sum += 2;
		MutableLock::unlock();
	}
}

void SetTidAndExecFunc(int threadNum, int threadId) {
	tid = threadId;
	Func(threadNum);
}

int main() {
	auto DoJob = []() {
		for (int threadNum = 1; threadNum <= 8; threadNum *= 2) {
			sum = 0; timer::reset();
			bakeryVector.init(threadNum);
			bakeryArray.init(threadNum);
			vector<thread> threadPool; threadPool.reserve(threadNum);
			for (int i = 0; i < threadNum; i++) threadPool.push_back(move(thread(SetTidAndExecFunc, threadNum, i)));
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

	cout << "\t<< Bakery(vector) Lock >>" << endl;
	MutableLock::lock = []() { bakeryVector.lock(); };
	MutableLock::unlock = []() { bakeryVector.unlock(); };
	DoJob();

	cout << "\t<< Bakery(array) Lock >>" << endl;
	MutableLock::lock = []() { bakeryArray.lock(); };
	MutableLock::unlock = []() { bakeryArray.unlock(); };
	DoJob();
}


