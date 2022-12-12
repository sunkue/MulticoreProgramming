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
#include <execution>
#include "Timer.hpp"
#include "CAS.hpp"

#include <tbb/tbb.h>

#include <concurrent_unordered_map.h>

using namespace std;
//#define UseStdParallelFor

constexpr auto NUM_TEST = 500'0000;
constexpr auto MAX_THREAD = 1;
int numOfThread = 1;
string Data[NUM_TEST];

void Benchmark() {
	unordered_map<string, int> table;
	for (auto& s : Data)
		table[s]++;

}

void Benchmark2() {
	tbb::concurrent_unordered_map<string, int> table;
#ifdef UseStdParallelFor
	std::vector<int> v(NUM_TEST); iota(v.begin(), v.end(), 0);
	std::for_each(std::execution::par, v.begin(), v.end(), [&table](auto i) {
		table[Data[i]]++;
		});
#else
	tbb::parallel_for(0, NUM_TEST, [&table](int i) {
		table[Data[i]]++;
		});
#endif
}

void Benchmark3() {
	concurrency::concurrent_unordered_map<string, int> table;

#ifdef UseStdParallelFor
	std::vector<int> v(NUM_TEST); iota(v.begin(), v.end(), 0);
	std::for_each(std::execution::par, v.begin(), v.end(), [&table](auto i) {
		table[Data[i]]++;
		});
#else
	tbb::parallel_for(0, NUM_TEST, [&table](int i) {
		table[Data[i]]++;
		});
#endif
}


int main()
{
	auto DoJob = [](auto& f) {
		for (int threadNum = 1; threadNum <= MAX_THREAD; threadNum *= 2) {
			timer::reset();
			numOfThread = threadNum;
			vector<thread> threadPool; threadPool.reserve(threadNum);
			for (int i = 0; i < threadNum; i++) threadPool.push_back(move(thread(f)));
			for (auto& t : threadPool) t.join();
			timer::elapsed("[" + to_string(threadNum) + " Threads] [", "]\t");
		}
	};

	for (int i = 0; i < NUM_TEST; i++)Data[i] = "##2" + to_string(i);

	// concurrency is legacy of tbb
	DoJob(Benchmark3);//16
	DoJob(Benchmark2);//10
	DoJob(Benchmark); //19
}



