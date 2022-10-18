#pragma once
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
