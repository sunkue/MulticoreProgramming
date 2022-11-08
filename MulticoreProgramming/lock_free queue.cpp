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

static const auto NUM_TEST = 1000'0000;
static const auto MAX_THREAD = 16;

struct Node {
	Node(int v) :value{ v } {}
	int value{};
	Node* next{};
};

class LF_QUEUE {
	Node* volatile head{},
		* volatile tail{};
public:
	LF_QUEUE() {
		head = tail = new Node{ -1 };
	}

	void enq(int x) {
		auto node = new Node(x);
		while (true) {
			auto last = tail;
			auto next = last->next;
			if (last != tail)continue;
			if (nullptr == next) {
				if (CAS::CAS(last->next, {nullptr}, node)) {
					CAS::CAS(tail, last, node);
					return;
				}
			}
			else CAS::CAS(tail, last, next);
		}
	}

	int deq() {
		while (true) {
			auto first = head;
			auto last = tail;
			auto next = first->next;
			if (first != head) continue;
			if (nullptr == next) return -1;
			if (first == last) {
				CAS::CAS(tail, last, next);
				continue;
			}
			auto ret = next->value;
			if (!CAS::CAS(head, first, next)) continue;
			delete first;
			return ret;
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
		auto p = head;
		while (p) {
			auto tmp = p;
			p = p->next;
			delete tmp;
		}
		head = tail = new Node{ -1 };
	}
};

LF_QUEUE q;

void Benchmark(int threadNum) {
	for (int i = 0; i < NUM_TEST / threadNum; i++) {
		if ((rand() % 2) || i < 32 / threadNum) {
			q.enq(i);
		}
		else {
			q.deq();
		}
	}
}

int main()
{
	auto DoJob = []() {
		for (int threadNum = 1; threadNum <= MAX_THREAD; threadNum *= 2) {
			q.reset();
			timer::reset();
			vector<thread> threadPool; threadPool.reserve(threadNum);
			for (int i = 0; i < threadNum; i++) threadPool.push_back(move(thread(Benchmark, threadNum)));
			for (auto& t : threadPool) t.join();
			timer::elapsed("[" + to_string(threadNum) + " Threads] [", "]\t");
			q.show();
		}
	};

	cout << "=========== LF 무잠금 (가끔 크래시) ===========" << endl;
	DoJob();
}


