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
	bool onRemove{ false };
	bool fullyLinked{ false };
	void lock() { m.lock(); }
	void unlock() { m.unlock(); }
	bool try_lock() { return m.try_lock(); }
private:
	recursive_mutex m;
};

class SKIP_LIST {
	Node* volatile head{};
	Node* volatile tail{};

	inline static thread_local Node* volatile prevs[MAX_LEVEL + 1];
	inline static thread_local Node* volatile currs[MAX_LEVEL + 1];

public:
	SKIP_LIST() { reset(); }

	bool add(int x) {
		while (true) {
			auto foundLvl = find(x, prevs, currs);
			if (foundLvl != -1) {
				auto node = currs[foundLvl];
				if (!node->onRemove) {
					while (!node->fullyLinked)
						;;;
					return false;
				}
				continue;
			}
			int topLocked = -1;
			int topLvl = 0;
			for (int i = 0; i < MAX_LEVEL; i++) {
				if (rand() % 2) break;
				topLvl++;
			}
			bool valid = true;
			for (int lvl = 0; valid && (lvl <= topLvl); lvl++) {
				auto prev = prevs[lvl];
				auto curr = currs[lvl];
				prev->lock();
				topLocked = lvl;
				valid = !prev->onRemove && !curr->onRemove && prev->next[lvl] == curr;
			}
			if (!valid) continue;
			auto node = new Node(x, topLvl);
			for (int lvl = 0; lvl <= topLvl; lvl++)
				node->next[lvl] = currs[lvl];
			for (int lvl = 0; lvl <= topLvl; lvl++)
				prevs[lvl]->next[lvl] = node;
			node->fullyLinked = true;
			for (int lvl = 0; lvl <= topLocked; lvl++)
				prevs[lvl]->unlock();
			return true;
		}
	}

	bool remove(int x) {
		// ¹Ì¿Ï¼º..
		Node* victim{};
		bool isOnRemove{};
		int topLvl = -1;
		while (true) {
			int foundLvl = find(x, prevs, currs);
			if (foundLvl != -1)victim = currs[foundLvl];
			if (isOnRemove || (victim && victim->topLvl == foundLvl && !victim->onRemove)) {
				if (!isOnRemove) {
					topLvl = victim->topLvl;
					victim->lock();
					if (victim->onRemove) {
						victim->unlock();
						return false;
					}
					victim->onRemove = true;
					isOnRemove = true;
				}
				int topLocked = -1;
				bool valid = true;
				for (int lvl = 0; valid && lvl <= topLvl; lvl++) {
					auto prev = prevs[lvl];
					prev->lock();
					topLocked = lvl;
					valid = !prev->onRemove && prev->next[lvl] == victim;
				}
				if (!valid)continue;
				for (int lvl = topLvl; 0 <= lvl; lvl--) {
					prevs[lvl]->next[lvl] = victim->next[lvl];
				}
				victim->unlock();
				for (int lvl = 0; lvl <= topLocked; lvl++)
					prevs[lvl]->unlock();
				return true;
			}
			return false;
		}
	}

	bool contains(int x) {
		int foundLvl = find(x, prevs, currs);
		return foundLvl != -1 && currs[foundLvl]->fullyLinked && !currs[foundLvl]->onRemove;
	}

	int find(int x, Node* volatile prevs[], Node* volatile currs[]) {
		int foundLvl = -1;
		auto prev = head;
		for (int cl = MAX_LEVEL; 0 <= cl; cl--) {
			if (cl != MAX_LEVEL) prevs[cl] = prevs[cl + 1];
			auto curr = prev->next[cl];
			while (curr->value < x) {
				prev = curr;
				curr = prev->next[cl];
			}
			if (foundLvl == -1 && curr->value == x)
				foundLvl = cl;
			prevs[cl] = prev;
			currs[cl] = curr;
		}
		return foundLvl;
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

	cout << "=========== Lazy Skip List Set ===========" << endl;
	DoJob();
}


