#pragma once
#include <iostream>
#include <chrono>

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