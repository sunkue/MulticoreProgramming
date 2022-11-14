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
static const auto MAX_THREAD = 64;

class StampPtr : public CAS::TaggedPointerSuper<class Node> {
public:
	StampPtr(Ptr ptr = nullptr, Tag tag = 0)
		:CAS::TaggedPointerSuper<class Node>{ ptr , tag } {};

	void set(Node* n) {
		taggedPtr = reinterpret_cast<SuperLine>(n) << 32 | 1;
	}
};

struct Node {
	Node(int v) :value{ v } {}
	int value{};
	StampPtr next{};
};

class STAMP_QUEUE {
	StampPtr head{}, tail{};
public:
	STAMP_QUEUE() { reset(); }

	void enq(int x) {
		auto node = new Node(x);
		while (true) {
			auto last = tail;
			auto next = last.getPtr()->next;
			if (last != tail)continue;
			if (nullptr == next.getPtr()) {
				if (last.getPtr()->next.CAS({ nullptr }, node,
					next.getTag(), next.getTag() + 1)) {
					tail.CAS(last.getPtr(), node,
						last.getTag(), last.getTag() + 1);
					return;
				}
			}
			else tail.CAS(last.getPtr(), next.getPtr(),
				last.getTag(), last.getTag() + 1);
		}
	}

	int deq() {
		while (true) {
			auto first = head;
			auto last = tail;
			auto next = first.getPtr()->next;
			if (first != head) continue;
			if (nullptr == next.getPtr()) return -1;
			if (first == last) {
				tail.CAS(last.getPtr(), next.getPtr(),
					last.getTag(), last.getTag() + 1);
				continue;
			}
			auto ret = next.getPtr()->value;
			if (!head.CAS(first.getPtr(), next.getPtr(),
				first.getTag(), first.getTag() + 1)) continue;
			delete first.getPtr();
			return ret;
		}
	}

	void show() {
		auto node = head.getPtr()->next;
		for (int i = 0; i < 20; i++) {
			if (node == tail)break;
			cout << node.getPtr()->value << " ";
			node = node.getPtr()->next;
		}
		cout << endl;
	}

	void reset() {
		auto p = head;
		while (p.getPtr()) {
			auto tmp = p.getPtr();
			p = tmp->next;
			delete tmp;
		}
		auto g = new Node{ -1 };
		head.set(g);
		tail.set(g);
	}
};

STAMP_QUEUE q;

void Benchmark(int threadNum) {
	try {
		for (int i = 0; i < NUM_TEST / threadNum; i++) {
			if ((rand() % 2) || i < 32 / threadNum) {
				q.enq(i);

			}
			else {
				q.deq();
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
			q.reset();
			timer::reset();
			vector<thread> threadPool; threadPool.reserve(threadNum);
			for (int i = 0; i < threadNum; i++) threadPool.push_back(move(thread(Benchmark, threadNum)));
			for (auto& t : threadPool) t.join();
			timer::elapsed("[" + to_string(threadNum) + " Threads] [", "]\t");
			q.show();
		}
	};

	cout << "=========== STAMP ===========" << endl;
	DoJob();
}


