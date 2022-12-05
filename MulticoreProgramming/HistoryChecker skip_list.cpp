#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <array>
#include <mutex>
#include "CAS.hpp"

using namespace std;
using namespace chrono;

constexpr int operationNum = 400'0000;
constexpr int range = 1000;
constexpr int checkerSize = range;

namespace C_SL {
	constexpr auto MAX_LEVEL = 3;
	struct Node {
		Node(int v, int top) :value{ v }, topLvl{ top }{}
		int value{};
		Node* volatile next[MAX_LEVEL + 1]{ nullptr };
		int topLvl{};
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
			delete target;
			return true;
		}

		bool contains(int x) {
			std::lock_guard lck{ m };
			find(x, prevs, currs);
			return currs[0]->value == x;
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
}

namespace L_SL {
	constexpr auto MAX_LEVEL = 3;

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
					if (node->onRemove)continue;
					while (!node->fullyLinked)
						;;;
					return false;
				}
				int topLocked = -1;
				int topLvl = 0;
				for (int i = 0; i < MAX_LEVEL; i++) {
					if (rand() % 2) break;
					topLvl++;
				}
				bool valid = true;
				std::array<std::optional<std::lock_guard<Node>>, MAX_LEVEL + 1> lcks;
				for (int lvl = 0; valid && (lvl <= topLvl); lvl++) {
					auto prev = prevs[lvl];
					auto curr = currs[lvl];
					lcks[lvl].emplace(*prev);
					topLocked = lvl;
					valid = !prev->onRemove && !curr->onRemove && prev->next[lvl] == curr;
				}
				if (!valid)  continue;
				auto node = new Node(x, topLvl);
				for (int lvl = 0; lvl <= topLvl; lvl++)
					node->next[lvl] = currs[lvl];
				for (int lvl = 0; lvl <= topLvl; lvl++)
					prevs[lvl]->next[lvl] = node;
				node->fullyLinked = true;
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
					std::optional<std::lock_guard<Node>> victimLck;
					if (!isOnRemove) {
						topLvl = victim->topLvl;
						victimLck.emplace(*victim);
						if (victim->onRemove) return false;
						victim->onRemove = true;
						isOnRemove = true;
					}
					int topLocked = -1;
					bool valid = true;
					std::array<std::optional<std::lock_guard<Node>>, MAX_LEVEL + 1> lcks;
					for (int lvl = 0; valid && lvl <= topLvl; lvl++) {
						auto prev = prevs[lvl];
						lcks[lvl].emplace(*prev);
						topLocked = lvl;
						valid = !prev->onRemove && prev->next[lvl] == victim;
					}
					if (!valid) continue;
					for (int lvl = topLvl; 0 <= lvl; lvl--)
						prevs[lvl]->next[lvl] = victim->next[lvl];
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
}

namespace LF_SL {
	constexpr auto MAX_LEVEL = 3;
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
}

LF_SL::SKIP_LIST my_set;

class HISTORY {
public:
	int op;
	int i_value;
	bool o_value;
	HISTORY(int o, int i, bool re) : op(o), i_value(i), o_value(re) {}
};

void worker(vector<HISTORY>* history, int num_threads)
{
	for (int i = 0; i < operationNum / num_threads; ++i) {
		int op = rand() % 3;
		switch (op) {
		case 0: {
			int v = rand() % range;
			my_set.add(v);
			break;
		}
		case 1: {
			int v = rand() % range;
			my_set.remove(v);
			break;
		}
		case 2: {
			int v = rand() % range;
			my_set.contains(v);
			break;
		}
		}
	}
}

void worker_check(vector<HISTORY>* history, int num_threads)
{
	for (int i = 0; i < operationNum / num_threads; ++i) {
		int op = rand() % 3;
		switch (op) {
		case 0: {
			int v = rand() % range;
			history->emplace_back(0, v, my_set.add(v));
			break;
		}
		case 1: {
			int v = rand() % range;
			history->emplace_back(1, v, my_set.remove(v));
			break;
		}
		case 2: {
			int v = rand() % range;
			history->emplace_back(2, v, my_set.contains(v));
			break;
		}
		}
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
		if (val < 0) {
			cout << "The value " << i << " removed while it is not in the set.\n";
			exit(-1);
		}
		else if (val > 1) {
			cout << "The value " << i << " is added while the set already have it.\n";
			exit(-1);
		}
		else if (val == 0) {
			if (my_set.contains(i)) {
				cout << "The value " << i << " should not exists.\n";
				exit(-1);
			}
		}
		else if (val == 1) {
			if (false == my_set.contains(i)) {
				cout << "The value " << i << " shoud exists.\n";
				exit(-1);
			}
		}
	}
	cout << " OK\n";
}

int main()
{
	for (int num_threads = 1; num_threads <= 16; num_threads *= 2) {
		vector <thread> threads;
		array<vector <HISTORY>, 16> history;
		my_set.reset();
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker_check, &history[i], num_threads);
		for (auto& th : threads)
			th.join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = duration_cast<milliseconds>(exec_t).count();
		my_set.show();
		cout << num_threads << "Threads.  Exec Time : " << exec_ms << endl;
		check_history(history, num_threads);
	}

	for (int num_threads = 1; num_threads <= 16; num_threads *= 2) {
		vector <thread> threads;
		array<vector <HISTORY>, 16> history;
		my_set.reset();
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num_threads; ++i)
			threads.emplace_back(worker, &history[i], num_threads);
		for (auto& th : threads)
			th.join();
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		auto exec_ms = duration_cast<milliseconds>(exec_t).count();
		my_set.show();
		cout << num_threads << "Threads.  Exec Time : " << exec_ms << endl;
		check_history(history, num_threads);
	}
}