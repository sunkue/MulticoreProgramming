#include <iostream>
#include <thread>
#include <vector>
#include <set>
#include <array>
#include <list>
#include <algorithm>
#include <mutex>
#include <ranges>
#include <random>
#include "Timer.hpp"
#include "CAS.hpp"

using namespace std;

static const auto NUM_TEST = 100'0000;
static const auto MAX_THREAD = 16;


struct Node {
	Node(int v) :value{ v } {}
	int value{};
	Node* volatile next{};
};

class STACK {
	Node* volatile top{};
	mutex m;
public:
	STACK() { reset(); }

	void push(int x) {
		auto node = new Node(x);
		lock_guard lck{ m };
		node->next = top;
		top = node;
	}

	int pop() {
		lock_guard lck{ m };
		if (nullptr == top) return -2;
		int value = top->value;
		auto node = top;
		top = top->next;
		delete node;
		return value;
	}

	void show() {
		auto node = top;
		for (int i = 0; i < 20; i++) {
			if (node == nullptr)break;
			cout << node->value << " ";
			node = node->next;
		}
		cout << endl;
	}

	void reset() {
		auto node = top;
		while (node) {
			auto tmp = node;
			node = node->next;
			delete tmp;
		}
		top = nullptr;
	}
};

STACK st;

void Benchmark(int threadNum) {
	try {
		for (int i = 1; i < NUM_TEST / threadNum; i++) {
			if ((rand() % 2) || i < 1000 / threadNum) {
				st.push(i);
			}
			else {
				st.pop();
			}
		}
	}
	catch (std::exception& e) {
		std::cout << e.what() << std::endl;
	}
}

int main()
{
	auto DoJob = []() {
		for (int threadNum = 1; threadNum <= MAX_THREAD; threadNum *= 2) {
			st.reset();
			timer::reset();
			vector<thread> threadPool; threadPool.reserve(threadNum);
			for (int i = 0; i < threadNum; i++) threadPool.push_back(move(thread(Benchmark, threadNum)));
			for (auto& t : threadPool) t.join();
			timer::elapsed("[" + to_string(threadNum) + " Threads] [", "]\t");
			st.show();
		}
	};

	cout << "=========== 성긴동기화 Stack ===========" << endl;
	DoJob();
}


