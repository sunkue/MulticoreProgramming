/*
*	2016182024 윤선규
*
*	c++ 20, x64, Release
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
#include <atomic>

namespace CAS {
	template <class T> struct is_atomicable : std::bool_constant<std::atomic<T>::is_always_lock_free> {};
	template <class T> concept Atomicable = is_atomicable<T>::value;

	template <Atomicable T>
	inline bool CAS(std::atomic<T>& target, T expected, T newValue) {
		return std::atomic_compare_exchange_strong(&target, &expected, newValue);
	}

	template <Atomicable T>
	inline bool CAS(volatile T& target, T expected, T newValue) {
		return std::atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic<T>*>(&target),
			&expected, newValue);
	}

	template <class T> concept AtomicablePointer = is_atomicable<T>::value && std::is_pointer<T>::value;

	// V should be present within tagmask bits.
	// marked_ptr which is just size_t is faster than his method.
	template <AtomicablePointer T, class V>
	inline bool CAS(volatile T& targetPtr, T expectedPtr, T newValuePtr
		, volatile V& targetValue, V expectedValue, V newValueValue) {
#ifdef _WIN64 // xxx000
		constexpr size_t tagmask = 0b111;
#else // xxxx00
		constexpr size_t tagmask = 0b11;
#endif // _WIN64
		constexpr size_t ptrmask = (SIZE_MAX ^ tagmask);
		size_t compressedTarget = reinterpret_cast<size_t>(targetPtr) | (targetValue & tagmask);
		size_t compressedExpected = reinterpret_cast<size_t>(expectedPtr) | (expectedValue & tagmask);
		size_t compressedNew = reinterpret_cast<size_t>(newValuePtr) | (newValueValue & tagmask);
		bool success = CAS(compressedTarget, compressedExpected, compressedNew);
		targetPtr = reinterpret_cast<T>(compressedTarget & ptrmask);
		targetValue = compressedTarget & tagmask;
		return success;
	}

}

namespace timer {
	using clk = std::chrono::high_resolution_clock;
	using namespace std::chrono;
	using namespace std::literals::string_view_literals;
	clk::time_point static thread_local StopWatch;
	inline void reset() { StopWatch = clk::now(); }
	inline void elapsed(const std::string_view header = ""sv, const std::string_view footer = "\n"sv) {
		std::cout << header << duration_cast<milliseconds>(clk::now() - StopWatch) << footer;
	}
}


using namespace std;

constexpr int MAX_THREAD = 16;
constexpr int BECNMARK_LOOP_COUNT = 400'0000;

struct Node {
	Node(int value) :value{ value } {}
	Node(int value, Node* next) :value{ value }, next{ next } {}
	void disable() { on = false; };
	int value{};
	bool on{ true };
	Node* next{};

	pair<Node*, bool> getNextAndOn() {
		while (true) {
			Node* n = next;
			bool o = on;
			//if (CAS::CAS(next, n, n, on, o, o)) {
			return make_pair(n, o);
			//}
		}
	}
};

class SET {
	Node* head{}, * tail{};
public:
	SET() {
		head = new Node(std::numeric_limits<int>::min());
		tail = new Node(std::numeric_limits<int>::max());
		head->next = tail;
	}

	// prev , curr
	pair<Node*, Node*> find(int x) {
		while (true) {
		Retry:
			Node* prev, * curr, * next{};
			prev = head;
			curr = prev->next;
			while (true) {
				bool on;
				auto currInfo = curr->getNextAndOn();
				next = currInfo.first;
				on = currInfo.second;
				while (!on) { // 삭제된 노드 curr.
					if (CAS::CAS(prev->next, curr, next,
						prev->on, true, true)) {
						// delete curr;
					}
					else goto Retry;
					curr = next;
					currInfo = curr->getNextAndOn();
					next = currInfo.first;
					on = currInfo.second;
				}

				if (x <= curr->value)
					return make_pair(prev, curr);

				prev = curr;
				curr = next;
			}
		}
	}

	bool add(int x) {
		Node* prev, * curr, * node{};
		while (true) {
			auto target = find(x);
			prev = target.first;
			curr = target.second;
			if (curr->value == x)
				return false;
			node = new Node(x, curr);
			if (CAS::CAS(prev->next, curr, node
				, prev->on, true, true))
				return true;
		}
	}

	bool remove(int x) {
		while (true) {
			Node* prev, * curr, * next;
			auto target = find(x);
			prev = target.first;
			curr = target.second;

			if (curr->value != x)
				return false;

			next = curr->next;

			if (!CAS::CAS(curr->next, next, next
				, curr->on, true, false))
				continue;

			CAS::CAS(prev->next, curr, next
				, prev->on, true, true);
			return true;
		}
	}

	bool contains(int x) {
		auto curr = head;
		auto on = curr->on;
		while (curr->value < x) {
			auto target = curr->getNextAndOn();
			curr = target.first;
			on = target.second;
		}
		return curr->value == x && on;
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

	cout << "=========== Lock Free 1 ===========" << endl;
	DoJob();


}


