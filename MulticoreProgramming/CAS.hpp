#pragma once
#include <atomic>
namespace CAS {
	using namespace std;

	template <class T> concept AtomicableType = atomic<T>::is_always_lock_free;

	template <AtomicableType T> inline bool CAS(atomic<T>* addr, T expected, T newValue) {
		return atomic_compare_exchange_strong(addr, &expected, newValue);
	}

	template <AtomicableType T> inline bool CAS(volatile T* addr, T expected, T newValue) {
		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic<T> *>(addr),
			&expected, newValue);
	}
}
