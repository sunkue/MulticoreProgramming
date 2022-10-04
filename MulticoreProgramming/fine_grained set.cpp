/*
*	2016182024 À±¼±±Ô
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
	int value{};
	Node* next{};
};

class SET {
	Node* head{}, * tail{};
public:
	SET() {
		head = new Node(std::numeric_limits<int>::min());
		tail = new Node(std::numeric_limits<int>::max());
		head->next = tail;
	}

	bool add(int x) {
		auto node = new Node(x);
		head->lock();
		auto prev = head;
		auto curr = prev->next;
		curr->lock();
		while (curr->value < node->value) {
			prev->unlock();
			prev = curr;
			curr = curr->next;
			curr->lock();
		}
		if (curr->value != x) {
			prev->next = node;
			node->next = curr;
			curr->unlock();
			prev->unlock();
			return true;
		}
		else {
			curr->unlock();
			prev->unlock();
			delete node;
			return false;
		}
	}

	bool remove(int x) {
		head->lock();
		auto prev = head;
		auto curr = prev->next;
		curr->lock();
		while (curr->value < x) {
			prev->unlock();
			prev = prev->next;
			curr = curr->next;
			curr->lock();
		}
		if (curr->value == x) {
			prev->next = curr->next;
			curr->unlock();
			delete curr;
			prev->unlock();
			return true;
		}
		else {
			curr->unlock();
			prev->unlock();
			return false;
		}
	}

	bool contains(int x) {
		head->lock();
		auto prev = head;
		auto curr = prev->next;
		curr->lock();
		while (curr->value < x) {
			prev->unlock();
			prev = prev->next;
			curr = curr->next;
			curr->lock();
		}
		if (curr->value == x) {
			curr->unlock();
			prev->unlock();
			return true;
		}
		else {
			curr->unlock();
			prev->unlock();
			return false;
		}
	}

	void show() {
		auto node = head->next;
		for (int i = 0; i < 20; i++) {
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

	DoJob();
}


