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

constexpr auto NUM_TEST = 4000'0000;
constexpr auto MAX_THREAD = 16;

constexpr int TIMELIMIT = 10000;

constexpr int EMPTY = 0;
constexpr int WAITTING = 1;
constexpr int BUSY = 2;

constexpr int POP = -1;
constexpr int TIMEOUT = -2;

int numOfThread = 1;

class Exchanger {
	atomic_int value{};
public:
	int exchange(int x, bool& busy) {
		for (int i = 0; i < TIMELIMIT; i++) {
			int c_value = value;
			int state = value >> 30;
			switch (state) {
			case EMPTY: {
				int n_value = (WAITTING << 30) | x;
				if (CAS::CAS(value, c_value, n_value)) {
					bool success = false;
					for (int i = 0; i < TIMELIMIT; i++) {
						if (BUSY == (value >> 30)) {
							success = true;
							break;
						}
					}
					if (!success)
						if (CAS::CAS(value, n_value, 0)) return TIMEOUT;
					int ret = value & 0x3FFFFFFFF;
					value = 0;
					return ret;
				}
			} break;
			case WAITTING: {
				int n_value = (BUSY << 30) | x;
				if (CAS::CAS(value, c_value, n_value)) {
					return c_value & 0x3FFFFFFFF;
				}
			}break;
			case BUSY: { busy = true; }break;
			}
		}
		return TIMEOUT;
	}
};

class EliminationArray {
	int range{ 1 };
	std::array<Exchanger, MAX_THREAD> exchangers;
public:
	int visit(int value) {
		int slot = rand() % range;
		bool busy = false;
		int ret = exchangers[slot].exchange(value, busy);
		int o_range = range;
		if ((TIMEOUT == ret) && (o_range > 1))
			CAS::CAS(range, o_range, range - 1);
		if ((busy) && (o_range < numOfThread / 2))
			CAS::CAS(range, o_range, range + 1);
		return ret;
	}
};

struct Node {
	Node(int v) :value{ v } {}
	int value{};
	Node* next{};
};

class STACK {
	Node* volatile top{};
	EliminationArray el;
public:
	STACK() { reset(); }

	void push(int x) {
		auto node = new Node(x);
		while (true) {
			node->next = top;
			if (CAS::CAS(top, node->next, node))return;
			if (POP == el.visit(x)) return;
		}
	}

	int pop() {
		while (true) {
			auto node = top;
			if (nullptr == node) return -2;
			int value = node->value;
			if (CAS::CAS(top, node, node->next)) {
				//delete node;
				return value;
			}
		}
	}

	void show() {
		auto node = top;
		for (int i = 0; i < 20; i++) {
			if (node == nullptr)break;
			cout << node->value << " ";
			node = node->next;
		}
		cout << endl;
	}

	void reset() {
		auto node = top;
		while (node) {
			auto tmp = node;
			node = node->next;
			delete tmp;
		}
		top = nullptr;
	}
};

STACK st;

void Benchmark() {
	try {
		for (int i = 1; i < NUM_TEST / numOfThread; i++) {
			if ((rand() % 2) || i < 1000 / numOfThread) {
				st.push(i);
			}
			else {
				st.pop();
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
			st.reset();
			timer::reset();
			numOfThread = threadNum;
			vector<thread> threadPool; threadPool.reserve(threadNum);
			for (int i = 0; i < threadNum; i++) threadPool.push_back(move(thread(Benchmark)));
			for (auto& t : threadPool) t.join();
			timer::elapsed("[" + to_string(threadNum) + " Threads] [", "]\t");
			st.show();
		}
	};

	cout << "===========  Elimination Lock Free Stack ===========" << endl;
	DoJob();
}


