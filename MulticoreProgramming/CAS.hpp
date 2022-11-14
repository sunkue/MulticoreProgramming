#pragma once
#include <atomic>
#include <bitset>

namespace CAS {
	template <class T> struct is_atomicable : std::bool_constant<std::atomic<T>::is_always_lock_free> {};
	template <class T> concept Atomicable = is_atomicable<T>::value;

	template <Atomicable T>
	inline bool CAS(volatile std::atomic<T>& target, T expected, const T& newValue) {
		return std::atomic_compare_exchange_strong(&target, &expected, newValue);
	}

	template <Atomicable T>
	inline bool CAS(volatile T& target, T expected, const T& newValue) {
		return std::atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic<T>*>(&target),
			&expected, newValue);
	}

	template <class T> concept AtomicablePointer = is_atomicable<T>::value && std::is_pointer<T>::value;

	// V should be present within tagmask bits.
	// marked_ptr which is just size_t is faster than his method.
	// !!!! 마지막 대입 부분에서 오류! 처음부터 하나의size_t 로 만들어서 사용해야함..
	template <AtomicablePointer T, class V>
	inline bool CAS(volatile T& targetPtr, T expectedPtr, T newValuePtr
		, volatile V& targetValue, V expectedValue, V newValueValue) {
		static_assert(std::_Always_false<T>, "is not atomic");
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

	template<class _Ty, class _TagTy = uint8_t>
	class TaggedPointer {
	public:
		using Ptr = _Ty*;
		using Tag = _TagTy;
	protected:
	public:
		size_t taggedPtr{};
	public:
		TaggedPointer(Ptr ptr = nullptr, Tag tag = 0) :taggedPtr{ reinterpret_cast<size_t>(ptr) } {
			checkTagOverflow(tag);
			taggedPtr |= tag;
		};
		auto operator<=>(const TaggedPointer&) const = default;
	public:
#ifdef _WIN64 // xxx000
		static inline constexpr size_t TAG_MASK = 0b111;
#else // xxxx00
		static inline constexpr size_t TAG_MASK = 0b11;
#endif // _WIN64
		static inline constexpr size_t PTR_MASK = (SIZE_MAX ^ TAG_MASK);
	public:
		Ptr getPtr()const {
			return reinterpret_cast<Ptr>(taggedPtr & PTR_MASK);
		}

		Tag getTag()const {
			return (taggedPtr & TAG_MASK);
		}

		std::pair<Ptr, Tag> getPtrTag()const {
			auto snapshot = taggedPtr;
			auto ptr = reinterpret_cast<Ptr>(snapshot) & PTR_MASK;
			auto tag = (snapshot & TAG_MASK);
			return std::make_pair(ptr, tag);
		}

		class TagOverflowed : public std::exception {
		public:
			TagOverflowed(Tag tag) noexcept : std::exception(("Tag bits overflowed." + std::to_string(size_t(tag))).c_str(), 1) {}
		};

		static void checkTagOverflow(Tag tag) {
			if ((tag & TAG_MASK) != tag) throw TagOverflowed(tag);
		}

		bool CAS(Ptr o_ptr, Ptr n_ptr, Tag o_tag, Tag n_tag) {
			checkTagOverflow(o_tag);
			checkTagOverflow(n_tag);
			auto oTaggedPtr = reinterpret_cast<size_t>(o_ptr) | o_tag;
			auto nTaggedPtr = reinterpret_cast<size_t>(n_ptr) | n_tag;
			return std::atomic_compare_exchange_strong(
				reinterpret_cast<volatile std::atomic_size_t*>(&taggedPtr), &oTaggedPtr, nTaggedPtr);
		}

		bool attemptTag(Ptr ptr, Tag tag) {
			checkTagOverflow(tag);
			return CAS(ptr, ptr, getTag(), tag);
		}
	};

#ifdef _WIN64
#else 
	template<class _Ty, class _TagTy = size_t>
	class TaggedPointerSuper {
	public:
		using Ptr = _Ty*;
		using Tag = _TagTy;
		using SuperLine = uint64_t;
	protected:
	public:
		volatile SuperLine taggedPtr{};
	public:
		TaggedPointerSuper(Ptr ptr = nullptr, Tag tag = 0) :taggedPtr{ reinterpret_cast<SuperLine>(ptr) << 32 } {
			taggedPtr |= tag;
		};
		auto operator<=>(const TaggedPointerSuper&) const = default;
	public:
		Ptr getPtr()const {
			return reinterpret_cast<Ptr>(taggedPtr >> 32);
		}

		Tag getTag()const {
			return static_cast<Tag>(taggedPtr);
		}

		std::pair<Ptr, Tag> getPtrTag()const {
			auto snapshot = taggedPtr;
			auto ptr = reinterpret_cast<Ptr>(snapshot >> 32);
			auto tag = static_cast<Tag>(snapshot);
			return std::make_pair(ptr, tag);
		}

		bool CAS(Ptr o_ptr, Ptr n_ptr, Tag o_tag, Tag n_tag) {
			auto oTaggedPtr = (reinterpret_cast<SuperLine>(o_ptr) << 32) | o_tag;
			auto nTaggedPtr = (reinterpret_cast<SuperLine>(n_ptr) << 32) | n_tag;
			return std::atomic_compare_exchange_strong(
				reinterpret_cast<volatile std::atomic<SuperLine>*>(&taggedPtr), &oTaggedPtr, nTaggedPtr);
		}

		bool attemptTag(Ptr ptr, Tag tag) {
			return CAS(ptr, ptr, getTag(), tag);
		}
	};
}
#endif // _WIN64
