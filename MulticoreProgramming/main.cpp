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
			auto taget = currs[0];
			bool failed = false;
			for (int lvl = currs[0]->topLvl; 1 <= lvl; lvl--) {
				if (!prevs[lvl]->next[lvl].CAS(currs[lvl], currs[lvl], false, true)) {
					failed = true;
					break;
				}
			}

			if (failed)continue;

			if (prevs[0]->next[0].CAS(currs[0], currs[0], false, true))
				continue;

			find(x, prevs, currs);
			return true;
		}
	}

	bool contains(int x) {
		return find(x, prevs, currs);
	}

	bool find(int x, Node* volatile prevs[], Node* volatile currs[]) {
		Node* prev, * curr, * next;
		bool tag{ false };
	retry:
		while (true) {
			prev = head;
			for (int cl = MAX_LEVEL; 0 <= cl; cl--) {
				curr = prev->next[cl].getPtr();
				while (true) {
					std::tie(next, tag) = curr->next[cl].getPtrTag();
					while (tag) {
						if (!prev->next[cl].CAS(curr, next, false, false)) 
							goto retry;
						curr = prev->next[cl].getPtr();
					std:tie(next, tag) = curr->next[cl].getPtrTag();
					}
					if (curr->value < x) { prev = curr; curr = next; }
					else break;
				}
				prevs[cl] = prev;
				currs[cl] = curr;
			}
		}
		return curr->value == x;
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

	cout << "=========== Lazy Skip List Set ===========" << endl;
	DoJob();
}



