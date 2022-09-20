#include <thread>
#include <iostream>
#include <atomic>
#include <functional>
#include <mutex>
#include <chrono>

using namespace std;
mutex l;

// ------------- 타이머 -------------------
namespace timer
{
	using clk = std::chrono::high_resolution_clock;
	using namespace std::chrono;
	using namespace std::literals::string_view_literals;

	clk::time_point thread_local static StopWatch;
	inline void start() {
		StopWatch = clk::now();
	}

	inline void end(const std::string_view mess = ""sv) {
		auto t = clk::now() - StopWatch;
		std::cout << mess << duration_cast<milliseconds>(t) << '\n';
	}
}
// ---------------------------------------
atomic_int victim = 0;
bool volatile flag[2] = { false, false };

void Lock(int myID)
{
	int other = 1 - myID;
	flag[myID] = true;
	
	atomic_thread_fence(memory_order_seq_cst);
	victim = myID;
	atomic_thread_fence(memory_order_relaxed);

	while (flag[other] && victim == myID) {}
}
void Unlock(int myID)
{
	flag[myID] = false;
}

int sum = 0;
void f(int tid)
{
	for (int i = 0; i < 250000000; i++)
	{
		//lock_guard lck(l);
		Lock(tid);
		sum += 2;
		Unlock(tid);
	}
}

// ---------------------------------------
auto const SIZE = 50000000;
int volatile x, y;
int trace_x[SIZE], trace_y[SIZE];
void ThreadFunc0()
{
	for (int i = 0; i < SIZE; i++) {
		x = i;
		atomic_thread_fence(memory_order_seq_cst);
		trace_y[i] = y;
	}
}
void ThreadFunc1()
{
	for (int i = 0; i < SIZE; i++) {
		y = i;
		atomic_thread_fence(memory_order_seq_cst);
		trace_x[i] = x;
	}
}

// ---------------------------------------

volatile bool done = false;
volatile int* bound;
int error;
void ThreadFunc11()
{
	for (int j = 0; j <= 25000000; ++j) *bound = -(1 + *bound);
	done = true;
}
void ThreadFunc22()
{
	while (!done) {
		int v = *bound;
		if ((v != 0) && (v != -1)) {
			cout << hex << v << endl;
			error++;
		}
	}
}
// ---------------------------------------


int main()
{
	int ARR[32];
	intmax_t addr = reinterpret_cast<intmax_t>(&ARR[16]);
	addr = (addr / 64) * 64;
	addr -= 1;
	bound = reinterpret_cast<int*>(addr);
	*bound = 0;
	timer::start();
	thread tt11(ThreadFunc11);
	thread tt22(ThreadFunc22);
	tt11.join();
	tt22.join();
	timer::end("[ ");
	cout << error << endl;

	return 0;
	timer::start();
	thread tt1(ThreadFunc0);
	thread tt2(ThreadFunc1);
	tt1.join();
	tt2.join();
	timer::end("[ ");

	int count = 0;
	for (int i = 0; i < SIZE; ++i)
		if (trace_x[i] == trace_x[i + 1])
			if (trace_y[trace_x[i]] == trace_y[trace_x[i] + 1]) {
				if (trace_y[trace_x[i]] != i) continue;
				count++;
			}
	cout << "Total Memory Inconsistency: " << count << endl;

	return 0;

	timer::start();
	thread t1(f, 0);
	thread t2(f, 1);
	t1.join();
	t2.join();
	cout << sum << endl;
	timer::end("[ ");
	return 0;

	for (int threadNum = 1; threadNum <= 8; threadNum *= 2) {
		vector<thread> threadPool;
		timer::start();
		threadPool.push_back(move(thread(f, threadNum)));
		for (auto& t : threadPool) t.join();
		timer::end("[ " + to_string(threadNum) + " Threads]\t[ ");
	}
}



