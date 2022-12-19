/*
*	2016182024 윤선규
*
*	c++ 20, x64, Release
* 
*   충돌시 성능하락이 조금 더 심하다. 노트북 모바일 등 에서 사용이 안된다. 일부 cpu 만 지원.(제온cpu 가 가장 잘 지원된다.)
*	시스템콜 호출하면 실패한다.
*   L1 캐시에서만 작동(모든 메모리가 L1캐시에 담겨야한다.)
*   => 이 한계를 MOB(momory order buffer) 와 결합하는 중 
* 
*/

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
#include "CAS.hpp"
#include "Timer.hpp"

using namespace std;

constexpr int MAX_THREAD = 16;
constexpr int BECNMARK_LOOP_COUNT = 400'0000;

struct Node {
	Node(int v) :value{ v } {}
	void disable() { on = false; };
	int value{};
	bool on{ true };
	Node* next{};
};

class SET {
	Node* head{}, * tail{};
public:
	SET() {
		head = new Node(std::numeric_limits<int>::min());
		tail = new Node(std::numeric_limits<int>::max());
		head->next = tail;
	}

	bool add(int x) {
		while (true) {
			auto prev = head;
			auto curr = prev->next;

			while (curr->value < x) {
				prev = curr;
				curr = curr->next;
			}

			auto node = new Node(x); // system call 은 밖으로

			if (_xbegin() != _XBEGIN_STARTED) continue;

			if (!validate(prev, curr)) {
				_xabort(0);
				continue;
			}

			if (curr->value != x) {
				node->next = curr;
				prev->next = node;
				_xend();
				return true;
			}
			else {
				_xend();
				return false;
			}
		}
	}

	bool remove(int x) {
		while (true) {
			auto prev = head;
			auto curr = prev->next;

			while (curr->value < x) {
				prev = curr;
				curr = curr->next;
			}

			if (_xbegin() != _XBEGIN_STARTED) continue;

			if (!validate(prev, curr)) {
				_xabort(0);
				continue;
			}

			if (curr->value == x) {
				curr->disable();
				prev->next = curr->next;
				_xend();
				return true;
			}
			else {
				_xend();
				return false;
			}
		}
	}

	bool contains(int x) {
		auto curr = head;
		while (curr->value < x) curr = curr->next;
		auto contains = curr->value == x;
		return contains;
	}

	void show() {
		auto node = head->next;
		for (int i = 0; i < 20; i++) {
			if (node == tail)break;
			cout << node->value << " ";
			node = node->next;
		}
		cout << endl;
	}

	void reset() {
		auto curr = head->next;
		while (curr != tail) {
			auto node = curr;
			curr = curr->next;
			delete node;
		}
		head->next = tail;
	}

	bool validate(Node* prev, Node* curr) {
		auto alive = prev->on && curr->on;
		auto noIntercept = prev->next == curr;
		return alive && noIntercept;
	}
};

SET s;

void Benchmark(int threadNum) {
	for (int i = 0; i < BECNMARK_LOOP_COUNT / threadNum; i++) {
		switch (rand() % 3)
		{
		case 0: { // ADD
			s.add(rand() % 1000);
		}break;
		case 1: { // REMOVE
			s.remove(rand() % 1000);
		}break;
		case 2: { // CONTAINS
			s.contains(rand() % 1000);
		}break;
		}
	}
}

int main()
{
	auto DoJob = []() {
		for (int threadNum = 1; threadNum <= MAX_THREAD; threadNum *= 2) {
			s.reset();
			timer::reset();
			vector<thread> threadPool; threadPool.reserve(threadNum);
			for (int i = 0; i < threadNum; i++)
				threadPool.push_back(move(thread(Benchmark, threadNum)));
			for (auto& t : threadPool) t.join();
			timer::elapsed("[" + to_string(threadNum) + " Threads] [", "]\t");
			s.show();
		}
	};

	cout << "=========== lazy transaction 게으른동기화 ===========" << endl;
	DoJob();
}


