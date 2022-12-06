#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <array>
#include <omp.h>
#include "CAS.hpp"

using namespace std;
using namespace chrono;

// compile with /openmp

int main()
{
	int sum = 0; int target = 5000'0000;

	int tid; int nthreads;
#pragma omp parallel shared(sum)
	{
#pragma omp for schedule(dynamic) nowait
		for (int i = 0; i < target; i++)
#pragma omp atomic
			sum += 2;
	}

	std::cout << "::" << sum << std::endl;
}



