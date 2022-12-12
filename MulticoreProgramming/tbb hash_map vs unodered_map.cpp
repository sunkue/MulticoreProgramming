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
	tbb::concurrent_hash_map<string, int> table;

	tbb::parallel_for(0, NUM_TEST, [&table](int i) {
		decltype(table)::accessor a;
		table.insert(a, Data[i]);
		a->second++;
		});
}

void Benchmark3() {
	tbb::concurrent_unordered_map<string, int> table;

	tbb::parallel_for(0, NUM_TEST, [&table](int i) {
		table[Data[i]]++;
		});
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

	// hash map 은 삭제가 쓰레드 세이프하다.

	DoJob(Benchmark);  //19
	DoJob(Benchmark3); //10
	DoJob(Benchmark2); //6
}



