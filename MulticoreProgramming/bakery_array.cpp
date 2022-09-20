#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <array>
#include <algorithm>
#include <mutex>
#include <assert.h>
using namespace std;

#define one_line
#define NO_MUTEX
constexpr auto MAX_THREAD_NUM = 8;
constexpr auto num = 50'000'000;

// ------------- 타이머 -------------------
namespace timer
{
	using clk = std::chrono::high_resolution_clock;
	using namespace std::chrono;
	using namespace std::literals::string_view_literals;

	static clk::time_point StopWatch;
	inline void start() {
		StopWatch = clk::now();
	}

	inline void end(const std::string_view mess = ""sv) {
		auto t = clk::now() - StopWatch;
		std::cout << mess << duration_cast<milliseconds>(t) << '\n';
	}
}

// ------------- 빵집 -------------------

thread_local static int tid;
template<size_t _Size>
class Bakery {
public:
	static_assert(_Size != 0);
public:
	Bakery() {
		flags.fill(false);
		labels.fill(0);
	}

	void lock() {
		const auto i = tid;
		flags.at(i) = true;
		const auto la_i = labels[i] = *ranges::max_element(labels) + 1;
#ifdef one_line
		for (size_t k = -1; ranges::any_of(labels,
			[&](const auto la_k) {
				k++; assert(la_k < 0x8fff'ffff'ffff'ffff);// warn_overflow
				return true
					&& this->flags[k]
					&& ((la_k < la_i) || ((la_k == la_i) && (k < i)));
			}); k = -1)
			;;;
#else
		for (volatile bool loop = true; loop; ) {
			loop = false;
			for (auto k = 0; k < _Size; k++) {
				const auto la_k = labels[k];
				assert(la_k < 0x8fff'ffff'ffff'ffff);// warn_overflow
				bool cmp{ (la_k < la_i) || ((la_k == la_i) && (k < i)) };
				if (flags[k] && cmp)
					loop = true;
			}
		}
#endif // one_line
	}

	void unlock() {
		flags.at(tid) = false;
	}

	void reset() {
		flags.fill(false);
		labels.fill(0);
	}
private:
	std::array<bool, _Size> flags;
	std::array<size_t, _Size> labels;

};




// -------------  sum, 상호배제 -------------------
volatile int64_t sum = 0;

#ifdef NO_MUTEX
Bakery<12> m;
#else
mutex m;
#endif // NO_MUTEX

// -------------  work 함수 -------------------
void _work(int num_of_thrs)
{
	for (auto i = 0; i < num / num_of_thrs; i++) {
		scoped_lock lck{ m };
		sum+=2;
	}
}

void work(const int thr_id, const int num_of_thrs)
{
	tid = thr_id;
	_work(num_of_thrs);
}

// -------------  main -------------------
int main()
{
	cout << fixed;
	for (auto i = 1; i <= MAX_THREAD_NUM; i *= 2) {
		sum = 0;
#ifdef NO_MUTEX
		m.reset();
#endif // NO_MUTEX
		vector<thread> threads; threads.reserve(i);
		timer::start();
		for (auto j = 0; j < i; j++)
			threads.emplace_back(work, j, i);
		for (auto& thr : threads)
			thr.join();
		cout << "threads[" << i << "]\tsum[" << sum << "]" << "error[" <<
			(1.l - (static_cast<long double>(sum) / static_cast<long double>(num*2))) * 100.l << "%]" << endl;
		timer::end("time spent :: ");
	}
	cout << "끝"; int i; cin >> i;
}
