#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <array>
#include <mutex>
#include "CAS.hpp"

using namespace std;
using namespace chrono;

constexpr int operationNum = 400'0000;
constexpr int checkerSize = 1000;

class null_mutex {
public:
	void lock() {}
	void unlock() {}
};

class NODE : public mutex {
public:
	int v;
	bool on = true;
	NODE* next;
	NODE() : v(-1), next(nullptr) {}
	NODE(int x) : v(x), next(nullptr) {}
	NODE(int value, NODE* next) :v{ value }, next{ next } {}
	void disable() { on = false; };

	pair<NODE*, bool> getNextAndOn() {
		return make_pair(next, on);
	}
};

class SET {
	NODE head, tail;
	mutex ll;
public:
	SET()
	{
		head.v = 0x80000000;
		tail.v = 0x7FFFFFFF;
		head.next = &tail;
		tail.next = nullptr;
	}
	bool ADD(int x)
	{
		NODE* prev = &head;
		ll.lock();
		NODE* curr = prev->next;
		while (curr->v < x) {
			prev = curr;
			curr = curr->next;
		}
		if (curr->v != x) {
			NODE* node = new NODE{ x };
			node->next = curr;
			prev->next = node;
			ll.unlock();
			return true;
		}
		else
		{
			ll.unlock();
			return false;
		}
	}

	bool REMOVE(int x)
	{
		NODE* prev = &head;
		ll.lock();
		NODE* curr = prev->next;
		while (curr->v < x) {
			prev = curr;
			curr = curr->next;
		}
		if (curr->v != x) {
			ll.unlock();
			return false;
		}
		else {
			prev->next = curr->next;
			delete curr;
			ll.unlock();
			return true;
		}
	}

	bool CONTAINS(int x)
	{
		NODE* prev = &head;
		ll.lock();
		NODE* curr = prev->next;
		while (curr->v < x) {
			prev = curr;
			curr = curr->next;
		}
		bool res = (curr->v == x);
		ll.unlock();
		return res;
	}
	void print20()
	{
		NODE* p = head.next;
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->v << ", ";
			p = p->next;
		}
		cout << endl;
	}

	void clear()
	{
		NODE* p = head.next;
		while (p != &tail) {
			NODE* t = p;
			p = p->next;
			delete t;
		}
		head.next = &tail;
	}
};

class F_SET {
	NODE head, tail;
public:
	F_SET()
	{
		head.v = 0x80000000;
		tail.v = 0x7FFFFFFF;
		head.next = &tail;
		tail.next = nullptr;
	}
	bool ADD(int x)
	{
		head.lock();
		NODE* prev = &head;
		NODE* curr = prev->next;
		curr->lock();
		while (curr->v < x) {
			prev->unlock();
			prev = curr;
			curr = curr->next;
			curr->lock();
		}
		if (curr->v != x) {
			NODE* node = new NODE{ x };
			node->next = curr;
			prev->next = node;
			curr->unlock();
			prev->unlock();
			return true;
		}
		else
		{
			curr->unlock();
			prev->unlock();
			return false;
		}
	}

	bool REMOVE(int x)
	{
		head.lock();
		NODE* prev = &head;
		NODE* curr = prev->next;
		curr->lock();
		while (curr->v < x) {
			prev->unlock();
			prev = curr;
			curr = curr->next;
			curr->lock();
		}
		if (curr->v != x) {
			curr->unlock();
			prev->unlock();
			return false;
		}
		else {
			prev->next = curr->next;
			curr->unlock();
			prev->unlock();
			delete curr;
			return true;
		}
	}

	bool CONTAINS(int x)
	{
		head.lock();
		NODE* prev = &head;
		NODE* curr = prev->next;
		curr->lock();
		while (curr->v < x) {
			prev->unlock();
			prev = curr;
			curr = curr->next;
			curr->lock();
		}
		bool res = (curr->v == x);
		curr->unlock();
		prev->unlock();
		return res;
	}
	void print20()
	{
		NODE* p = head.next;
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->v << ", ";
			p = p->next;
		}
		cout << endl;
	}

	void clear()
	{
		NODE* p = head.next;
		while (p != &tail) {
			NODE* t = p;
			p = p->next;
			delete t;
		}
		head.next = &tail;
	}
};

class O_SET {
	NODE head, tail;
public:
	O_SET()
	{
		head.v = 0x80000000;
		tail.v = 0x7FFFFFFF;
		head.next = &tail;
		tail.next = nullptr;
	}

	bool ADD(int x)
	{
		while (true) {
			NODE* prev = &head;
			NODE* curr = prev->next;
			while (curr->v < x) {
				prev = curr;
				curr = curr->next;
			}
			scoped_lock lck{ *prev , *curr };
			if (!validate(prev, curr)) continue;
			if (curr->v != x) {
				NODE* node = new NODE{ x };
				node->next = curr;
				prev->next = node;
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	bool REMOVE(int x)
	{
		while (true) {
			NODE* prev = &head;
			NODE* curr = prev->next;
			while (curr->v < x) {
				prev = curr;
				curr = curr->next;
			}
			scoped_lock lck{ *prev , *curr };
			if (!validate(prev, curr)) continue;
			if (curr->v != x) {
				return false;
			}
			else {
				prev->next = curr->next;
				curr->disable();
				return true;
			}
		}
	}

	bool CONTAINS(int x)
	{
		while (true) {
			NODE* prev = &head;
			NODE* curr = prev->next;
			while (curr->v < x) {
				prev = curr;
				curr = curr->next;
			}
			scoped_lock lck{ *prev , *curr };
			if (!validate(prev, curr)) continue;
			return curr->v == x;
		}
	}

	bool validate(NODE* prev, NODE* curr) {
		NODE* node = &head;
		while (node->v <= prev->v) {
			if (node == prev) return prev->next == curr;
			node = node->next;
		}
		return false;
	}

	void print20()
	{
		NODE* p = head.next;
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->v << ", ";
			p = p->next;
		}
		cout << endl;
	}

	void clear()
	{
		NODE* p = head.next;
		while (p != &tail) {
			NODE* t = p;
			p = p->next;
			delete t;
		}
		head.next = &tail;
	}
};

class L_SET {
	NODE head, tail;
public:
	L_SET()
	{
		head.v = 0x80000000;
		tail.v = 0x7FFFFFFF;
		head.next = &tail;
		tail.next = nullptr;
	}

	bool ADD(int x)
	{
		while (true) {
			NODE* prev = &head;
			NODE* curr = prev->next;
			while (curr->v < x) {
				prev = curr;
				curr = curr->next;
			}
			scoped_lock lck{ *prev , *curr };
			if (!validate(prev, curr)) continue;
			if (curr->v != x) {
				NODE* node = new NODE{ x };
				node->next = curr;
				prev->next = node;
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	bool REMOVE(int x)
	{
		while (true) {
			NODE* prev = &head;
			NODE* curr = prev->next;
			while (curr->v < x) {
				prev = curr;
				curr = curr->next;
			}
			scoped_lock lck{ *prev , *curr };
			if (!validate(prev, curr)) continue;
			if (curr->v != x) {
				return false;
			}
			else {
				curr->disable(); // 순서에 따라 
								 // valid => invalid 오판 :: 허용가능
								 // invalid => valid 오판 :: 심각한 버그
				prev->next = curr->next;
				return true;
			}
		}
	}

	bool CONTAINS(int x)
	{
		NODE* curr = &head;
		while (curr->v < x) curr = curr->next;
		return curr->v == x && curr->on;
	}

	bool validate(NODE* prev, NODE* curr)
	{
		auto alive = prev->on && curr->on;		// 둘 다 리스트에 존재한다.
		auto noIntercept = prev->next == curr;	// 중간에 다른 노드가 끼어들지 않았다.
		return alive && noIntercept;
	}

	void print20()
	{
		NODE* p = head.next;
		for (int i = 0; i < 20; ++i) {
			if (p == &tail) break;
			cout << p->v << ", ";
			p = p->next;
		}
		cout << endl;
	}

	void clear()
	{
		NODE* p = head.next;
		while (p != &tail) {
			NODE* t = p;
			p = p->next;
			delete t;
		}
		head.next = &tail;
	}
};

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

class LF_SET1 {
	Node* head{}, * tail{};
public:
	LF_SET1() {
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

	bool ADD(int x) {
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

	bool REMOVE(int x) {
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

	bool CONTAINS(int x) {
		auto curr = head;
		bool mark;
		while (curr->value < x) {
			curr = curr->next.get_ptr_mark(&mark);
		}
		return curr->value == x && !mark;
	}

	void print20() {
		auto node = head->next.get_ptr();
		for (int i = 0; i < 20; i++) {
			if (node == tail)break;
			cout << node->value << " ";
			node = node->next.get_ptr();
		}
		cout << endl;
	}

	void clear() {
		auto curr = head->next.get_ptr();
		while (curr != tail) {
			auto node = curr;
			curr = curr->next.get_ptr();
			delete node;
		}
		head->next = LF_PTR{ false, tail };
	}
};

class LF_SET2 {
	NODE head, tail;
public:
	LF_SET2()
	{
		head.v = 0x80000000;
		tail.v = 0x7FFFFFFF;
		head.next = &tail;
		tail.next = nullptr;
	}

	pair<NODE*, NODE*> find(int x) {
		while (true) {
		Retry:
			auto prev = &head;
			auto curr = prev->next;
			while (true) {
				bool on;
				auto currInfo = curr->getNextAndOn();
				auto next = currInfo.first;
				on = currInfo.second;
				while (!on) { // 삭제된 노드 curr.
					if (CAS::CAS(prev->next, curr, next,
						prev->on, true, true)) {
						// delete curr;
					}
					else goto Retry;
					curr = next;
					currInfo = curr->getNextAndOn();
					next = currInfo.first;
					on = currInfo.second;
				}

				if (x <= curr->v)
					return make_pair(prev, curr);

				prev = curr;
				curr = next;
			}
		}
	}

	bool ADD(int x)
	{
		while (true) {
			auto target = find(x);
			auto prev = target.first;
			auto curr = target.second;
			if (curr->v == x)
				return false;
			auto node = new NODE(x, curr);
			if (CAS::CAS(prev->next, curr, node
				, prev->on, true, true))
				return true;
			delete node;
		}
	}

	bool REMOVE(int x)
	{
		while (true) {
			auto target = find(x);
			auto prev = target.first;
			auto curr = target.second;

			if (curr->v != x)
				return false;

			auto next = curr->next;

			if (!CAS::CAS(curr->next, next, next
				, curr->on, true, false))
				continue;

			CAS::CAS(prev->next, curr, next
				, prev->on, true, true);
			return true;
		}
	}

	bool CONTAINS(int x)
	{
		auto curr = &head;
		auto on = curr->on;
		while (curr->v < x) {
			auto target = curr->getNextAndOn();
			curr = target.first;
			on = target.second;
		}
		return curr->v == x && on;
	}

	void print20()
	{
		auto node = head.next;
		for (int i = 0; i < 20; i++) {
			if (node == &tail)break;
			cout << node->v << ", ";
			node = node->next;
		}
		cout << endl;
	}

	void clear()
	{
		auto curr = head.next;
		while (curr != &tail) {
			auto node = curr;
			curr = curr->next;
			delete node;
		}
		head.next = &tail;
	}
};

LF_SET1 my_set;

class HISTORY {
public:
	int op;
	int i_value;
	bool o_value;
	HISTORY(int o, int i, bool re) : op(o), i_value(i), o_value(re) {}
};

void worker(vector<HISTORY>* history, int num_threads)
{
	for (int i = 0; i < operationNum / num_threads; ++i) {
		int op = rand() % 3;
		switch (op) {
		case 0: {
			int v = rand() % 1000;
			my_set.ADD(v);
			break;
		}
		case 1: {
			int v = rand() % 1000;
			my_set.REMOVE(v);
			break;
		}
		case 2: {
			int v = rand() % 1000;
			my_set.CONTAINS(v);
			break;
		}
		}
	}
}

void worker_check(vector<HISTORY>* history, int num_threads)
{
	for (int i = 0; i < operationNum / num_threads; ++i) {
		int op = rand() % 3;
		switch (op) {
		case 0: {
			int v = rand() % 1000;
			history->emplace_back(0, v, my_set.ADD(v));
			break;
		}
		case 1: {
			int v = rand() % 1000;
			history->emplace_back(1, v, my_set.REMOVE(v));
			break;
		}
		case 2: {
			int v = rand() % 1000;
			history->emplace_back(2, v, my_set.CONTAINS(v));
			break;
		}
		}
	}
}

void check_history(array <vector <HISTORY>, 16>& history, int num_threads)
{
	array <int, checkerSize> survive = {};
	cout << "Checking Consistency : ";
	if (history[0].size() == 0) {
		cout << "No history.\n";
		return;
	}
	for (int i = 0; i < num_threads; ++i) {
		for (auto& op : history[i]) {
			if (false == op.o_value) continue;
			if (op.op == 3) continue;
			if (op.op == 0) survive[op.i_value]++;
			if (op.op == 1) survive[op.i_value]--;
		}
	}
	for (int i = 0; i < survive.size(); ++i) {
		int val = survive[i];
		if (val < 0) {
			cout << "The value " << i << " removed while it is not in the set.\n";
			exit(-1);
		}
		else if (val > 1) {
			cout << "The value " << i << " is added while the set already have it.\n";
			exit(-1);
		}
		else if (val == 0) {
			if (my_set.CONTAINS(i)) {
				cout << "The value " << i << " should not exists.\n";
				exit(-1);
			}
		}
		else if (val == 1) {
			if (false == my_set.CONTAINS(i)) {
				cout << "The value " << i << " shoud exists.\n";
				exit(-1);
			}
		}
	}
	cout << " OK\n";
}

int main()
{
	for (int num_threads = 1; num_threads <= 16; num_threads *= 2) {
		vector <thread> threads;
		array<vector <HISTORY>, 16> history;
		my_set.clear();
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker_check, &history[i], num_threads);
		for (auto& th : threads)
			th.join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = duration_cast<milliseconds>(exec_t).count();
		my_set.print20();
		cout << num_threads << "Threads.  Exec Time : " << exec_ms << endl;
		check_history(history, num_threads);
	}

	for (int num_threads = 1; num_threads <= 16; num_threads *= 2) {
		vector <thread> threads;
		array<vector <HISTORY>, 16> history;
		my_set.clear();
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker, &history[i], num_threads);
		for (auto& th : threads)
			th.join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = duration_cast<milliseconds>(exec_t).count();
		my_set.print20();
		cout << num_threads << "Threads.  Exec Time : " << exec_ms << endl;
		check_history(history, num_threads);
	}
}