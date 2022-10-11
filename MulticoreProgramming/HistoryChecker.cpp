#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <array>
#include <mutex>

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
	void disable() { on = false; };
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

class NB_SET {
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

NB_SET my_set;

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