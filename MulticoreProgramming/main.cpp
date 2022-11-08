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

using namespace std;

struct Node : public mutex {
	Node(int v) :value{ v } {}
	int value{};
	Node* next{};
};

class QUEUE {
	Node* head{}, * tail{};
	mutex m;
public:
	QUEUE() {
		head = new Node(std::numeric_limits<int>::min());
		tail = new Node(std::numeric_limits<int>::max());
		head->next = tail;
	}

	bool add(int x) {
		auto node = new Node(x);
		auto prev = head;
		lock_guard lck{ m };
		auto curr = prev->next;
		while (curr->value < node->value) {
			prev = curr;
			curr = curr->next;
		}
		if (curr->value != x) {
			prev->next = node;
			node->next = curr;
			return true;
		}
		else {
			delete node;
			return false;
		}
	}

	bool remove(int x) {
		auto prev = head;
		lock_guard lck{ m };
		auto curr = prev->next;
		while (curr->value < x) {
			prev = prev->next;
			curr = curr->next;
		}
		if (curr->value == x) {
			prev->next = curr->next;
			delete curr;
			return true;
		}
		else {
			return false;
		}
	}

	bool contains(int x) {
		auto prev = head;
		lock_guard lck{ m };
		while (prev->value < x) {
			prev = prev->next;
		}
		if (prev->value == x) {
			return true;
		}
		else {
			return false;
		}
	}

	void show() {
		auto node = head->next;
		for (int i = 0; i < 30; i++) {
			if (node == tail)break;
			cout << node->value << " ";
			node = node->next;
		}
		cout << endl;
	}

	void reset() {
		auto curr = head->next;
		while (curr != tail) {
			auto node = curr;
			curr = curr->next;
			delete node;
		}
		head->next = tail;
	}
};

QUEUE q;

void Benchmark(int threadNum) {
	for (int i = 0; i < 400'0000 / threadNum; i++) {
		switch (rand() % 3) // zeroTo3(engine)
		{
		case 0: { // ADD
			q.add(rand() % 1000);
		}break;
		case 1: { // REMOVE
			q.remove(rand() % 1000);
		}break;
		case 2: { // CONTAINS
			q.contains(rand() % 1000);
		}break;
		}
	}
}

int main()
{
	auto DoJob = []() {
		for (int threadNum = 1; threadNum <= 16; threadNum *= 2) {
			q.reset();
			timer::reset();
			vector<thread> threadPool; threadPool.reserve(threadNum);
			for (int i = 0; i < threadNum; i++) threadPool.push_back(move(thread(Benchmark, threadNum)));
			for (auto& t : threadPool) t.join();
			timer::elapsed("[" + to_string(threadNum) + " Threads] [", "]\t");
			q.show();
		}
	};

	cout << "=========== coarse_grained 성긴동기화 ===========" << endl;
	DoJob();
}


