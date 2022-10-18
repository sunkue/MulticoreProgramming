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

class Node;
class LF_PTR {
	unsigned long long next;
public:
	LF_PTR() : next(0) {}
	LF_PTR(bool marking, Node* ptr)
	{
		next = reinterpret_cast<unsigned long long>(ptr);
		if (true == marking) next = next | 1;
	}
	Node* get_ptr()
	{
		return reinterpret_cast<Node*>(next & 0xFFFFFFFFFFFFFFFE);
	}
	bool get_removed()
	{
		return (next & 1) == 1;
	}
	Node* get_ptr_mark(bool* removed)
	{
		unsigned long long cur_next = next;
		*removed = (cur_next & 1) == 1;
		return reinterpret_cast<Node*>(cur_next & 0xFFFFFFFFFFFFFFFE);
	}
	bool CAS(Node* o_ptr, Node* n_ptr, bool o_mark, bool n_mark)
	{
		unsigned long long o_next = reinterpret_cast<unsigned long long>(o_ptr);
		if (true == o_mark) o_next++;
		unsigned long long n_next = reinterpret_cast<unsigned long long>(n_ptr);
		if (true == n_mark) n_next++;
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_uint64_t*>(&next), &o_next, n_next);
	}
	bool attempt_mark(Node* ptr, bool mark) {
		unsigned long long o_next = reinterpret_cast<unsigned long long>(ptr);
		unsigned long long n_next = o_next;
		if (true == mark) n_next++;
		return atomic_compare_exchange_strong(
			reinterpret_cast<atomic_uint64_t*>(&next), &o_next, n_next);
	}
};

class Node {
public:
	int value;
	LF_PTR next;
	Node() : value(-1), next(false, nullptr) {}
	Node(int x) : value(x), next(false, nullptr) {}
	Node(int x, Node* ptr) : value(x), next(false, ptr) {}
};

class SET {
	Node* head{}, * tail{};
public:
	SET() {
		head = new Node(std::numeric_limits<int>::min());
		tail = new Node(std::numeric_limits<int>::max());
		head->next = LF_PTR{ false, tail };
	}

	// prev , curr
	pair<Node*, Node*> find(int x) {
		while (true) {
		Retry:
			Node* prev, * curr, * next{};
			prev = head;
			curr = prev->next.get_ptr();
			while (true) {
				bool removed;
				next = curr->next.get_ptr_mark(&removed);
				while (removed) { // 삭제된 노드 curr.
					if (prev->next.CAS(curr, next, false, false)) {
						// delete curr;
					}
					else goto Retry;
					curr = next;
					next = curr->next.get_ptr_mark(&removed);
				}

				if (x <= curr->value)
					return make_pair(prev, curr);

				prev = curr;
				curr = next;
			}
		}
	}

	bool add(int x) {
		while (true) {
			auto target = find(x);
			auto prev = target.first;
			auto curr = target.second;
			if (curr->value == x)
				return false;
			auto node = new Node(x, curr);
			if (prev->next.CAS(curr, node, false, false))
				return true;
			delete node;
		}
	}

	bool remove(int x) {
		while (true) {
			Node* prev, * curr, * next;
			auto target = find(x);
			prev = target.first;
			curr = target.second;

			if (curr->value != x)
				return false;

			next = curr->next.get_ptr();

			if (!curr->next.attempt_mark(next, true))
				continue;

			prev->next.CAS(curr, next, false, false);
			return true;
		}
	}

	bool contains(int x) {
		auto curr = head;
		bool mark{};
		while (curr->value < x) {
			curr = curr->next.get_ptr_mark(&mark);
		}
		return curr->value == x && !mark;
	}

	void show() {
		auto node = head->next.get_ptr();
		for (int i = 0; i < 20; i++) {
			if (node == tail)break;
			cout << node->value << " ";
			node = node->next.get_ptr();
		}
		cout << endl;
	}

	void reset() {
		auto curr = head->next.get_ptr();
		while (curr != tail) {
			auto node = curr;
			curr = curr->next.get_ptr();
			delete node;
		}
		head->next = LF_PTR{ false, tail };
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
	CAS::TaggedPointer<int> ff;
	int x;
	ff.attemptTag(&x, 123);


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

	cout << "=========== Lock Free 2 ===========" << endl;
	DoJob();


}


