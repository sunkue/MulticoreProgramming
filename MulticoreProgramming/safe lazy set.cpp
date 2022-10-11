/*
*	2016182024 윤선규
*
*	c++ 20, x64, Release
*/

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
#include "CAS.hpp"
#include "Timer.hpp"

using namespace std;

constexpr int MAX_THREAD = 16;
constexpr int BECNMARK_LOOP_COUNT = 400'0000;

struct Node : public mutex {
	Node(int v) :value{ v } {}
	void disable() { on = false; };
	int value{};
	bool on{ true };
	atomic<shared_ptr<Node>> next{};
};

class SET {
	atomic<shared_ptr<Node>> head{}, tail{};
public:
	SET() {
		head = make_shared<Node>(std::numeric_limits<int>::min());
		tail = make_shared<Node>(std::numeric_limits<int>::max());
		head.load()->next.store(tail);
	}

	bool add(int x) {
		while (true) {
			auto prev = head.load();
			auto curr = prev->next.load();

			while (curr->value < x) {
				prev = curr;
				curr = curr->next;
			}

			scoped_lock lck{ *prev , *curr };

			if (!validate(prev, curr)) continue;

			if (curr->value != x) {
				auto node = make_shared<Node>(x);
				node->next.store(curr);
				prev->next.store(node);
				return true;
			}
			else {
				return false;
			}
		}
	}

	bool remove(int x) {
		while (true) {
			auto prev = head.load();
			auto curr = prev->next.load();

			while (curr->value < x) {
				prev = curr;
				curr = curr->next;
			}

			scoped_lock lck{ *prev , *curr };

			if (!validate(prev, curr)) continue;

			if (curr->value == x) {
				curr->disable(); // 순서에 따라 
								 // valid => invalid 오판 :: 허용가능
								 // invalid => valid 오판 :: 심각한 버그
				prev->next.store(curr->next);
				return true;
			}
			else {
				return false;
			}
		}
	}

	bool contains(int x) {
		auto curr = head.load();
		while (curr->value < x) curr = curr->next;
		auto contains = curr->value == x;
		return contains;
	}

	void show() {
		auto node = head.load()->next.load();
		for (int i = 0; i < 20; i++) {
			if (node == tail.load())break;
			cout << node->value << " ";
			node = node->next;
		}
		cout << endl;
	}

	void reset() {
		head.load()->next.store(tail);
	}

	bool validate(const atomic<shared_ptr<Node>> prev, const atomic<shared_ptr<Node>> curr) {
		auto alive = prev.load()->on && curr.load()->on;
		auto noIntercept = prev.load()->next.load() == curr.load();
		return alive && noIntercept;
	}
};

SET s;

void Benchmark(int threadNum) {
	for (int i = 0; i < BECNMARK_LOOP_COUNT / threadNum; i++) {
		switch (rand() % 3)
		{
		case 0: { // ADD
			s.add(rand() % 1000);
		}break;
		case 1: { // REMOVE
			s.remove(rand() % 1000);
		}break;
		case 2: { // CONTAINS
			s.contains(rand() % 1000);
		}break;
		}
	}
}

int main()
{
	auto DoJob = []() {
		for (int threadNum = 1; threadNum <= MAX_THREAD; threadNum *= 2) {
			s.reset();
			timer::reset();
			vector<thread> threadPool; threadPool.reserve(threadNum);
			for (int i = 0; i < threadNum; i++)
				threadPool.push_back(move(thread(Benchmark, threadNum)));
			for (auto& t : threadPool) t.join();
			timer::elapsed("[" + to_string(threadNum) + " Threads] [", "]\t");
			s.show();
		}
	};

	cout << "=========== safe lazy 메모리관리 게으른동기화 ===========" << endl;

	DoJob();
}


