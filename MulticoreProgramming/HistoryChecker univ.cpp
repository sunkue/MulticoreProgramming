#include <iostream>
#include <stdlib.h>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <set>
#include <array>

using namespace std;
using namespace chrono;

static const auto NUM_TEST = 40000;
static const auto KEY_RANGE = 1000;
static const auto MAX_THREAD = 64;
constexpr int checkerSize = 1000;

static int THREAD_NUM;
thread_local int thread_id;

class Consensus {
	size_t result{ numeric_limits<size_t>::max() };
	bool CAS(size_t old_value, size_t new_value) {
		return atomic_compare_exchange_strong(reinterpret_cast<atomic_size_t*>(&result), &old_value, new_value);
	}
public:
	size_t decide(size_t value) {
		if (CAS(numeric_limits<size_t>::max(), value)) return value;
		else return result;
	}
};

typedef bool Response;

enum Method { M_ADD, M_REMOVE, M_CONTAINS };

struct Invocation {
	Method method;
	int	input;
};

class SeqObject {
	set <int> seq_set;
public:
	Response Apply(const Invocation& invoc) {
		Response res;
		switch (invoc.method) {
		case M_ADD:
			if (seq_set.count(invoc.input) != 0) res = false;
			else {
				seq_set.insert(invoc.input);
				res = true;
			}
			break;
		case M_REMOVE:
			if (seq_set.count(invoc.input) == 0) res = false;
			else {
				seq_set.erase(invoc.input);
				res = true;
			}
			break;
		case M_CONTAINS:
			res = (seq_set.count(invoc.input) != 0);
			break;
		default: throw;
		}
		return res;
	}

	void Print20() {
		cout << "First 20 item : ";
		int count = 20;
		for (auto n : seq_set) {
			if (count-- == 0) break;
			cout << n << ", ";
		}
	}

	void clear() {
		seq_set.clear();
	}
};

class NODE {
public:
	Invocation invoc{};
	Consensus decideNext;
	NODE* next{};
	volatile int seq{};
	NODE() = default;
	NODE(const Invocation& input_invoc) :invoc{ input_invoc } {}
};


class LFUniversal {
	friend class WFUniversal;
protected:
	NODE* head[MAX_THREAD];
	NODE* tail;
public:
	NODE* GetMaxNODE() {
		NODE* max_node = head[0];
		for (int i = 1; i < MAX_THREAD; i++) {
			if (max_node->seq < head[i]->seq) max_node = head[i];
		}
		return max_node;
	}

	void clear() {
		while (tail != nullptr) {
			NODE* temp = tail;
			tail = tail->next;
			delete temp;
			temp = nullptr;
		}
		tail = new NODE;
		tail->seq = 1;
		for (int i = 0; i < MAX_THREAD; ++i)
			head[i] = tail;
	}

	LFUniversal() {
		tail = new NODE;
		tail->seq = 1;
		for (int i = 0; i < MAX_THREAD; ++i)
			head[i] = tail;
	}
	~LFUniversal() {
		clear();
	}

	Response Apply(const Invocation& invoc) {
		NODE* prefer = new NODE{ invoc };
		while (prefer->seq == 0) {
			NODE* before = GetMaxNODE();
			NODE* after = reinterpret_cast<NODE*>(before->decideNext.decide(reinterpret_cast<size_t>(prefer)));
			before->next = after;
			after->seq = before->seq + 1;
			head[thread_id] = after;
		}

		SeqObject my_object;

		NODE* curr = tail->next;
		while (curr != prefer) {
			my_object.Apply(curr->invoc);
			curr = curr->next;
		}
		return my_object.Apply(curr->invoc);
	}

	void Print20() {
		NODE* before = GetMaxNODE();

		SeqObject my_object;

		NODE* curr = tail->next;
		while (true) {
			my_object.Apply(curr->invoc);
			if (curr == before) break;
			curr = curr->next;

		}
		my_object.Print20();
	}
};

class WFUniversal : public LFUniversal {
	NODE* announce[MAX_THREAD];
public:
	void clear() {
		LFUniversal::clear();
		for (int i = 0; i < MAX_THREAD; ++i) {
			announce[i] = tail;
		}
	}

	WFUniversal() :LFUniversal{} {
		for (int i = 0; i < MAX_THREAD; ++i) {
			announce[i] = tail;
		}
	}
	~WFUniversal() {
		clear();
	}

	Response Apply(const Invocation& invoc) {
		const auto i = thread_id;
		announce[i] = new NODE{ invoc };
		head[i] = GetMaxNODE();
		while (announce[i]->seq == 0) {
			NODE* before = head[i];
			NODE* help = announce[((before->seq + 1) % THREAD_NUM)];
			NODE* prefer = help->seq == 0 ? help : announce[i];
			NODE* after = reinterpret_cast<NODE*>(before->decideNext.decide(reinterpret_cast<size_t>(prefer)));
			before->next = after;
			after->seq = before->seq + 1;
			head[i] = after;
		}

		static thread_local SeqObject my_object;
		my_object.clear();
		NODE* curr = tail->next;
		while (curr != announce[i]) {
			my_object.Apply(curr->invoc);
			curr = curr->next;
		}
		head[i] = announce[i];
		return my_object.Apply(curr->invoc);
	}
};


class MutexUniversal {
	SeqObject seq_object;
	mutex m_lock;
public:

	void clear() {
		seq_object.clear();
	}

	Response Apply(const Invocation& invoc) {
		m_lock.lock();
		Response res = seq_object.Apply(invoc);
		m_lock.unlock();
		return res;
	}

	void Print20() {
		seq_object.Print20();
	}
};

//SeqObject my_set;
//MutexUniversal my_set;
//LFUniversal my_set;
WFUniversal my_set;

void Benchmark(int num_thread, int tid) {
	thread_id = tid;

	Invocation invoc;

	for (int i = 0; i < NUM_TEST / num_thread; ++i) {
		switch (rand() % 3) {
		case 0: invoc.method = M_ADD; break;
		case 1: invoc.method = M_REMOVE; break;
		case 2: invoc.method = M_CONTAINS; break;
		}
		invoc.input = rand() % KEY_RANGE;
		my_set.Apply(invoc);
	}
}

class HISTORY {
public:
	int op;
	int i_value;
	bool o_value;
	HISTORY(int o, int i, bool re) : op(o), i_value(i), o_value(re) {}
};

void worker(vector<HISTORY>* history, int num_thread, int tid)
{
	thread_id = tid;
	Invocation invoc;

	for (int i = 0; i < NUM_TEST / num_thread; ++i) {
		switch (rand() % 3) {
		case 0: invoc.method = M_ADD; break;
		case 1: invoc.method = M_REMOVE; break;
		case 2: invoc.method = M_CONTAINS; break;
		}
		invoc.input = rand() % KEY_RANGE;
		my_set.Apply(invoc);
	}
}
void worker_check(vector<HISTORY>* history, int num_thread, int tid)
{
	thread_id = tid;
	Invocation invoc;

	for (int i = 0; i < NUM_TEST / num_thread; ++i) {
		auto op = rand() % 3;
		switch (op) {
		case 0: invoc.method = M_ADD; break;
		case 1: invoc.method = M_REMOVE; break;
		case 2: invoc.method = M_CONTAINS; break;
		}
		invoc.input = rand() % KEY_RANGE;
		history->emplace_back(op, invoc.input, my_set.Apply(invoc));
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
		Invocation invoc; 
		invoc.method = M_CONTAINS;
		invoc.input = i;
		if (val < 0) {
			cout << "The value " << i << " removed while it is not in the set.\n";
			exit(-1);
		}
		else if (val > 1) {
			cout << "The value " << i << " is added while the set already have it.\n";
			exit(-1);
		}
		else if (val == 0) {
			if (my_set.Apply(invoc)) {
				cout << "The value " << i << " should not exists.\n";
				exit(-1);
			}
		}
		else if (val == 1) {
			if (false == my_set.Apply(invoc)) {
				cout << "The value " << i << " shoud exists.\n";
				exit(-1);
			}
		}
	}
	cout << " OK\n";
}

int main()
{
	cout << "================== WF =====================" << endl;
	for (int num_threads = 1; num_threads <= 16; num_threads *= 2) {
		THREAD_NUM = num_threads;
		vector <thread> threads;
		array<vector <HISTORY>, 16> history;
		my_set.clear();
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker_check, &history[i], num_threads, i);
		for (auto& th : threads)
			th.join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = duration_cast<milliseconds>(exec_t).count();
		cout << num_threads << "Threads.  Exec Time : " << exec_ms <<"::";
		my_set.Print20();
		check_history(history, num_threads);
	}

	for (int num_threads = 1; num_threads <= 16; num_threads *= 2) {
		THREAD_NUM = num_threads;
		vector <thread> threads;
		array<vector <HISTORY>, 16> history;
		my_set.clear();
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker, &history[i], num_threads, i);
		for (auto& th : threads)
			th.join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = duration_cast<milliseconds>(exec_t).count();
		cout << num_threads << "Threads.  Exec Time : " << exec_ms << "::";
		my_set.Print20();
		check_history(history, num_threads);
	}

	int i;
	cin >> i;
}