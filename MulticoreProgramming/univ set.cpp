#include <iostream>
#include <stdlib.h>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <set>

using namespace std;
using namespace chrono;

static const auto NUM_TEST = 4000;
static const auto KEY_RANGE = 1000;
static const auto MAX_THREAD = 64;
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
		cout << endl;
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


int main()
{
	vector <thread*> worker_threads;
	for (int num_thread = 1; num_thread <= 16; num_thread *= 2)
	{
		THREAD_NUM = num_thread;
		my_set.clear();

		auto start = high_resolution_clock::now();
		for (int i = 0; i < num_thread; ++i)
			worker_threads.push_back(new thread{ Benchmark, num_thread, i + 1 });
		for (auto pth : worker_threads) pth->join();
		auto du = high_resolution_clock::now() - start;

		for (auto pth : worker_threads) delete pth;
		worker_threads.clear();

		cout << num_thread << " Threads   ";
		cout << duration_cast<milliseconds>(du).count() << "ms \n";
		my_set.Print20();
	}
}