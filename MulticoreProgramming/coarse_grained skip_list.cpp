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

constexpr auto NUM_TEST = 400'0000;
constexpr auto TEST_RANGE = 1000;
constexpr auto MAX_THREAD = 16;
constexpr auto MAX_LEVEL = 3;
int numOfThread = 1;


struct Node {
	Node(int v, int top) :value{ v }, topLvl{ top }{}
	int value{};
	Node* volatile next[MAX_LEVEL + 1]{ nullptr };
	int topLvl{};
	bool removed{ false };
};

class SKIP_LIST {
	Node* volatile head{};
	Node* volatile tail{};

	inline static thread_local Node* prevs[MAX_LEVEL + 1];
	inline static thread_local Node* currs[MAX_LEVEL + 1];

	mutex m;
public:
	SKIP_LIST() { reset(); }

	bool add(int x) {
		std::lock_guard lck{ m };
		find(x, prevs, currs);
		if (currs[0]->value == x)return false;
		int lvl = 0;
		for (int i = 0; i < MAX_LEVEL; i++) {
			if (rand() % 2) break;
			lvl++;
		}
		auto node = new Node(x, lvl);
		for (int i = lvl; 0 <= i; i--) {
			//	node->next[i] = prevs[i]->next[i];
			node->next[i] = currs[i];
			prevs[i]->next[i] = node;
		}
		return true;
	}

	bool remove(int x) {
		std::lock_guard lck{ m };
		find(x, prevs, currs);
		if (currs[0]->value != x) return false;
		//		if (!contains(x))return;
		auto target = currs[0];
		for (int i = target->topLvl; 0 <= i; --i) {
			prevs[i]->next[i] = target->next[i];
		}
		target->removed = true;
		//		delete target;
		return true;
	}

	bool contains(int x) {
		std::lock_guard lck{ m };
		find(x, prevs, currs);
		return currs[0]->value == x && !currs[0]->removed;
	}

	void find(int x, Node* prevs[], Node* currs[]) {
		prevs[MAX_LEVEL] = head;
		for (int cl = MAX_LEVEL; 0 <= cl; cl--) {
			if (cl != MAX_LEVEL) prevs[cl] = prevs[cl + 1];
			currs[cl] = prevs[cl]->next[cl];
			while (currs[cl]->value < x) {
				prevs[cl] = currs[cl];
				currs[cl] = currs[cl]->next[cl];
			}
		}
	}

	void show() {
		auto node = head->next[0];
		for (int i = 0; i < 20; i++) {
			if (node == nullptr)break;
			cout << node->value << " ";
			node = node->next[0];
		}
		cout << endl;
	}

	void reset() {
		auto node = head;
		while (node) {
			auto tmp = node;
			node = node->next[0];
			delete tmp;
		}
		head = new Node(std::numeric_limits<int>::min(), MAX_LEVEL);
		tail = new Node(std::numeric_limits<int>::max(), MAX_LEVEL);
		for (int i = 0; i < MAX_LEVEL + 1; i++)
			head->next[i] = tail;
	}
};

SKIP_LIST sl;

void Benchmark() {
	try {
		int key;
		for (int i = 0; i < NUM_TEST / numOfThread; i++) {
			switch (rand() % 3) {
			case 0: key = rand() % TEST_RANGE;
				sl.add(key);
				break;
			case 1: key = rand() % TEST_RANGE;
				sl.remove(key);
				break;
			case 2: key = rand() % TEST_RANGE;
				sl.contains(key);
				break;
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
			sl.reset();
			timer::reset();
			numOfThread = threadNum;
			vector<thread> threadPool; threadPool.reserve(threadNum);
			for (int i = 0; i < threadNum; i++) threadPool.push_back(move(thread(Benchmark)));
			for (auto& t : threadPool) t.join();
			timer::elapsed("[" + to_string(threadNum) + " Threads] [", "]\t");
			sl.show();
		}
	};

	cout << "=========== CoarseGrained Skip List ===========" << endl;
	DoJob();
}


