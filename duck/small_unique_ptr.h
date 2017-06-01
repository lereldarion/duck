#pragma once

// Analog to std::unique_ptr<T>, but has a local storage to avoid new() for small objects
// Intended to store small virtual classes with less overhead.

#include <cassert>
#include <duck/type_traits.h> // For InPlace<T> and <type_traits>
#include <memory>
#include <utility>

namespace duck {

// TODO add alignment template arg ?

template <typename T, std::size_t StorageLen> class SmallUniquePtr {
	/* This class is analog to unique_ptr<T>, and has a similar API.
	 * However it has a local buffer used instead of allocation if the type is small enough.
	 *
	 * Only polymorphic T types are supported (virtual).
	 * Non polymorphic classes do not benefit from this class.
	 * Either construct them as is, or use unique_ptr (size is fixed).
	 *
	 * To support move operations, T must have a virtual move function.
	 * TODO base class to inherit from
	 */
private:
	static_assert (std::is_polymorphic<T>::value,
	               "This class should only be used for polymorphic types");
	using StorageType = typename std::aligned_storage<StorageLen>::type;
	static constexpr auto storage_align = alignof (StorageType);

public:
	using pointer = T *;
	using const_pointer = const T *;

	// Constructors
	SmallUniquePtr () = default;
	SmallUniquePtr (std::nullptr_t) noexcept : SmallUniquePtr () {}
	SmallUniquePtr (pointer p) noexcept : data_ (p) {}
	SmallUniquePtr (const SmallUniquePtr &) = delete;
	SmallUniquePtr (SmallUniquePtr && other) {
		if (other.is_allocated ()) {
			data_ = other.data_;
			other.data_ = nullptr;
		} else {
			// Move into place (may allocate) TODO
		}
	}

	template <typename U, typename... Args> SmallUniquePtr (InPlace<U>, Args &&... args) {
		build<U> (std::forward<Args> (args)...);
	}
	template <typename U, typename V, typename... Args>
	SmallUniquePtr (InPlace<U>, std::initializer_list<V> ilist, Args &&... args) {
		build<U> (std::move (ilist), std::forward<Args> (args)...);
	}

	// TODO movable

	~SmallUniquePtr () { reset (); }

	SmallUniquePtr & operator= (std::nullptr_t) noexcept {
		reset ();
		return *this;
	}

	// Modifiers
	pointer release () noexcept {
		// TODO improve
		make_storage_allocated_if_not ();
		auto tmp = data_;
		data_ = nullptr;
		return tmp;
	}
	void reset () noexcept {
		if (data_) {
			if (is_allocated ())
				delete data_;
			else
				data_->~T ();
			data_ = nullptr;
		}
	}
	void reset (pointer p) noexcept {
		reset ();
		data_ = p;
	}

	template <typename U = T, typename... Args> void emplace (Args &&... args) {
		reset ();
		build<U> (std::forward<Args> (args)...);
	}
	template <typename U = T, typename V, typename... Args>
	void emplace (std::initializer_list<V> ilist, Args &&... args) {
		reset ();
		build<U> (std::move (ilist), std::forward<Args> (args)...);
	}

	// Observers
	constexpr pointer get () const noexcept { return data_; }
	constexpr operator bool () const noexcept { return data_; }
	pointer operator-> () const noexcept { return data_; }
	typename std::add_lvalue_reference<T>::type operator* () const noexcept { return *data_; }

	// Both functions are undefined is pointer is null
	bool is_inline () const noexcept {
		/* T is polymorphic, we can expect derived classes to be used.
		 * Depending on the layout of derived classes, data_ may be different from the storage pointer.
		 * Checking that data_ == &inline_storage_ is not sufficient.
		 * The condition is to check that data_ is inside the inline_storage_ buffer.
		 */
		auto inline_storage_as_byte = reinterpret_cast<const unsigned char *> (&inline_storage_);
		auto data_as_byte = reinterpret_cast<const unsigned char *> (data_);
		return inline_storage_as_byte <= data_as_byte &&
		       data_as_byte < inline_storage_as_byte + StorageLen;
	}
	bool is_allocated () const noexcept { return !is_inline (); }

private:
	/* Condition used to determine if a type U can be built inline.
	 * This is not an equivalence: a type U could be built inline and be stored allocated.
	 * (for example if result of a move)
	 */
	template <typename U>
	using BuildInline =
	    std::integral_constant<bool, (sizeof (U) <= StorageLen && alignof (U) <= storage_align)>;

	// Create storage for a type U
	template <typename U> pointer create_storage () {
		return create_storage_helper (sizeof (U), BuildInline<U>{});
	}
	pointer create_storage_helper (std::size_t, std::true_type) {
		return reinterpret_cast<pointer> (&inline_storage_);
	}
	pointer create_storage_helper (std::size_t size, std::false_type) {
		return static_cast<pointer> (::operator new (size));
	}

	// Static Deleter of storage for a type U
	template <typename U> static void destroy_storage (pointer storage) {
		destroy_storage_helper (storage, BuildInline<U>{});
	}
	static void destroy_storage_helper (pointer, std::true_type) {}
	static void destroy_storage_helper (pointer storage, std::false_type) {
		::operator delete (storage);
	}

	template <typename U, typename... Args> void build (Args &&... args) {
		static_assert (std::is_base_of<T, U>::value, "build object must derive from T");
		// tmp serves as a RAII temporary storage for the buffer : storage is cleaned on Constr error.
		auto deleter = destroy_storage<U>;
		std::unique_ptr<T, decltype (deleter)> tmp{create_storage<U> (), deleter};
		new (tmp.get ()) U (std::forward<Args> (args)...);
		data_ = tmp.release ();
	}

	void make_storage_allocated_if_not () noexcept {
		if (is_inline ()) {
			// FIXME size is unknown but smaller than StorageLen if inline
			auto tmp = create_storage_helper (StorageLen, std::false_type{});
			data_ = new (tmp) T (std::move (*data_)); // FIXME use virtual move if virt
		}
	}

	pointer data_{nullptr}; // Points to the T object, not the chosen storage
	StorageType inline_storage_;
};

// Comparison TODO ?
}
