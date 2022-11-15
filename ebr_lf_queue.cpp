#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>

using namespace std;
using namespace chrono;

constexpr int MAX_THREAD = 16;

thread_local int thread_id;
int num_threads;

class EBRNODE {
public:
	int key;
	long long epoch;
	EBRNODE* volatile next;

	EBRNODE() { next = nullptr; }

	EBRNODE(int x) {
		key = x;
		next = nullptr;
	}
	~EBRNODE() {	}
};

class alignas(64) EBRMANAGER {
	EBRNODE* head, *tail;
	EBRMANAGER* ebrs;
	int counter;
public:
	long long local_epoch;
	EBRMANAGER() { counter = 0; head = tail = nullptr; local_epoch = 0; }
	~EBRMANAGER() {
		if (nullptr == tail) return;
		while (tail != head) {
			EBRNODE* t = head;
			head = head->next;
			delete t;
		}
		delete head;
	}

	void init_ebrs(EBRMANAGER p[])
	{
		ebrs = p;
	}

	void retire_node(EBRNODE* p)
	{
		p->epoch = local_epoch;
		if (nullptr != tail)
			tail->next = p;
		tail = p;
		if (nullptr == head) head = tail;
	}

	long long get_last_epoch()
	{
		long long oldest_epoch = INT_MAX;
		for (int i = 0; i < num_threads; ++i) {
			if (oldest_epoch > ebrs[i].local_epoch)
				oldest_epoch = ebrs[i].local_epoch;
		}
		return oldest_epoch;
	}

	EBRNODE* get_node(int key)
	{
		if (tail != head)
			if (head->epoch < get_last_epoch()) {
				EBRNODE* e = head;
				head = e->next;
				e->key = key;
				e->next = nullptr;
				e->epoch = local_epoch;
				return e;
			}
		return new EBRNODE{ key };
	}

	void update_epoch(long long *new_e)
	{
		counter++;
		if (counter < 1000) return;
		counter = 0;
		local_epoch = (*new_e)++;
	}
};

class EBR_LFQUEUE {
	alignas(64) EBRNODE* volatile head;
	alignas(64) EBRNODE* volatile tail;
	long long g_epoch;
	EBRMANAGER ebr[MAX_THREAD];
public:
	EBR_LFQUEUE()
	{
		head = tail = new EBRNODE();
		ebr[0].init_ebrs(ebr);
		g_epoch = 0;
	}
	~EBR_LFQUEUE()
	{
		clear();
		delete head;
	}

	void thread_init()
	{
		for (auto &e : ebr)
			e.init_ebrs(ebr);
	}

	void clear()
	{
		thread_init();
		while (head != tail) {
			EBRNODE* to_delete = head;
			head = head->next;
			delete to_delete;
		}
	}

	bool CAS(EBRNODE* volatile* next, EBRNODE* o_node, EBRNODE* n_node)
	{
		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic_int*>(next),
			reinterpret_cast<int*>(&o_node),
			reinterpret_cast<int>(n_node)
		);
	}

	void Enq(int x)
	{
		EBRNODE* e = ebr[thread_id].get_node(x);
		while (true) {
			EBRNODE* last = tail;
			EBRNODE* next = last->next;
			if (last != tail) continue;
			if (nullptr == next) {
				if (true == CAS(&last->next, nullptr, e)) {
					CAS(&tail, last, e);
					ebr[thread_id].update_epoch(&g_epoch);	
					return;
				}
			}
			else
				CAS(&tail, last, next);
		}
	}

	int Deq()
	{
		while (true) {
			EBRNODE* first = head;
			EBRNODE* last = tail;
			EBRNODE* next = first->next;
			if (first != head) continue;
			if (nullptr == next) {
				ebr[thread_id].update_epoch(&g_epoch);
				return -1; // EMPTY
			}
			if (first == last) {
				CAS(&tail, last, next);
				continue;
			}
			int value = next->key;
			if (false == CAS(&head, first, next)) continue;
			ebr[thread_id].retire_node(first);
			ebr[thread_id].update_epoch(&g_epoch);
			return value;
		}
	}

	void display20()
	{
		EBRNODE* ptr = head->next;
		for (int i = 0; i < 20; ++i) {
			if (nullptr == ptr) break;
			cout << ptr->key << ", ";
			ptr = ptr->next;
		}
		cout << endl;
	}
};

EBR_LFQUEUE my_queue;

constexpr int NUM_TEST = 10000000;

void benchmark(int t_id)
{
	int loop = NUM_TEST / num_threads;
	thread_id = t_id;
	for (int i = 0; i < loop; ++i) {
		if ((rand() % 2 == 0) || (i < 2 / num_threads))
			my_queue.Enq(i);
		else
			my_queue.Deq();
	}
}

int main()
{
	for (int num = 1; num <= MAX_THREAD; num = num * 2) {
		num_threads = num;
		vector <thread> threads;
		my_queue.clear();
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num; ++i)
			threads.emplace_back(benchmark, i);
		for (auto& th : threads) th.join();
		auto end_t = high_resolution_clock::now();
		auto du = end_t - start_t;

		cout << num << " Threads,  ";
		cout << "Exec time " <<
			duration_cast<milliseconds>(du).count() << "ms  ";
		my_queue.display20();
	}
}