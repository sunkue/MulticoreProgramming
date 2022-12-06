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
	CAS::TaggedPointer<Node> next[MAX_LEVEL + 1]{};
	int value{};
	int topLvl{};
};

class SKIP_LIST {
	Node* volatile head{};
	Node* volatile tail{};

	inline static thread_local Node* volatile prevs[MAX_LEVEL + 1];
	inline static thread_local Node* volatile currs[MAX_LEVEL + 1];

public:
	SKIP_LIST() { reset(); }

	bool add(int x) {
		int topLvl = 0;
		for (int i = 0; i < MAX_LEVEL; i++) {
			if (rand() % 2) break;
			topLvl++;
		}

		while (true) {
			if (find(x, prevs, currs)) return false;
			auto node = new Node(x, topLvl);
			for (int lvl = 0; lvl <= topLvl; lvl++)
				node->next[lvl] = CAS::TaggedPointer<Node>(currs[lvl], false);

			if (!prevs[0]->next[0].CAS(currs[0], node, false, false))
				continue;

			for (int lvl = 1; lvl <= topLvl; lvl++) {
				while (!prevs[lvl]->next[lvl].CAS(currs[lvl], node, false, false)) {
					find(x, prevs, currs);
				}
			}

			return true;
		}
	}

	bool remove(int x) {
		while (true) {
			if (!find(x, prevs, currs)) return false;
			auto target = currs[0];
			for (int lvl = target->topLvl; 1 <= lvl; lvl--) {
				auto [next, removed] = target->next[lvl].getPtrTag();
				while (!removed) {
					target->next[lvl].attemptTag(next, true);
					std::tie(next, removed) = target->next[lvl].getPtrTag();
				}
			}

			auto [next, removed] = target->next[0].getPtrTag();

			while (true) {
				bool it = target->next[0].CAS(next, next, false, true);
				std::tie(next, removed) = currs[0]->next[0].getPtrTag();
				if (it) {
					find(x, prevs, currs);
					return true;
				}
				else if (removed)return false;
			}
		}
	}

	bool remove1(int x) {
		// maybe not safe
		while (true) {
			if (!find(x, prevs, currs)) return false;
			auto target = currs[0];
			bool failed = false;
			for (int lvl = target->topLvl; 1 <= lvl; lvl--) {
				auto [next, removed] = target->next[lvl].getPtrTag();
				if (!target->next[lvl].attemptTag(next, true)) {
					failed = true;
					break;
				}
			}

			if (failed) continue;

			auto [next, removed] = target->next[0].getPtrTag();
			if (!target->next[0].CAS(next, next, false, true))
				continue;

			find(x, prevs, currs);
			return true;
		}
	}

	bool contains(int x) {
		return find(x, prevs, currs);
	}

	bool find(int x, Node* volatile prevs[], Node* volatile currs[]) {
	retry:
		prevs[MAX_LEVEL] = head;
		for (int lvl = MAX_LEVEL; 0 <= lvl; lvl--) {
			currs[lvl] = prevs[lvl]->next[lvl].getPtr();
			while (true) {
				auto [next, removed] = currs[lvl]->next[lvl].getPtrTag();
				while (removed) {
					if (!prevs[lvl]->next[lvl].CAS(currs[lvl], next, false, false))
						goto retry;
					currs[lvl] = next;
					std::tie(next, removed) = currs[lvl]->next[lvl].getPtrTag();
				}
				if (currs[lvl]->value < x) {
					prevs[lvl] = currs[lvl];
					currs[lvl] = next;
				}
				else {
					if (lvl == 0) return currs[0]->value == x;
					prevs[lvl - 1] = prevs[lvl];
					break;
				}
			}
		}
	}

	void show() {
		auto node = head->next[0];
		for (int i = 0; i < 20; i++) {
			if (node == nullptr)break;
			cout << node.getPtr()->value << " ";
			node = node.getPtr()->next[0];
		}
		cout << endl;
	}

	void reset() {
		auto node = head;
		while (node) {
			auto tmp = node;
			node = node->next[0].getPtr();
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

	cout << "=========== Lock Free Skip List Set ===========" << endl;
	DoJob();
}



