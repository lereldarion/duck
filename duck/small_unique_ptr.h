#pragma once

// Analog to std::unique_ptr<T>, but has a local storage to avoid new() for small objects
// Intended to store small virtual classes with less overhead.
// STATUS: mature

#include <cassert>
#include <duck/type_traits.h> // For in_place_type_t<T> and <type_traits>
#include <utility>

namespace duck {

namespace Detail {
	template <typename DestroyFunc, DestroyFunc destroyer> class DestroyPointerAtEndOfScope {
		/* Stores a pointer that can either released, or destroyed by the deleter at end of scope.
		 * The destroyer function must do nothing for nullptr.
		 */
	public:
		DestroyPointerAtEndOfScope (void * p) noexcept : p_ (p) {}
		DestroyPointerAtEndOfScope (const DestroyPointerAtEndOfScope &) = delete;
		DestroyPointerAtEndOfScope & operator= (const DestroyPointerAtEndOfScope &) = delete;
		~DestroyPointerAtEndOfScope () { destroyer (p_); }
		void * get () const noexcept { return p_; }
		void * release () noexcept {
			auto tmp = p_;
			p_ = nullptr;
			return tmp;
		}

	private:
		void * p_;
	};
} // namespace Detail

/* Helper classes used to define a virtual move function.
 * Any type T that uses move operations in a SmallUniquePtr MUST inherit from
 * SmallUniquePtrMakeMovableAbstractBase.
 *
 * The minimal usage is to make the base class of the class hierarchy inherit
 * SmallUniquePtrMakeMovableAbstractBase.
 * Every concrete derived class that is expected to be stored in a SmallUniquePtr must override
 * small_unique_ptr_move(p) to perform a move construction of *this at p.
 * The override can be written manually, or generated using DUCK_SMALL_UNIQUE_PTR_MAKE_MOVABLE.
 * Abstract classes cannot be constructed at all, so they do not need an override.
 */
struct SmallUniquePtrMakeMovableBase {
	virtual ~SmallUniquePtrMakeMovableBase () = default;
	virtual void small_unique_ptr_move (void * to_storage) noexcept = 0;
};
#define DUCK_SMALL_UNIQUE_PTR_MAKE_MOVABLE                                                         \
	void small_unique_ptr_move (void * storage) noexcept override {                                  \
		new (storage) duck::remove_reference_t<decltype (*this)> (std::move (*this));                  \
	}

template <typename T, std::size_t StorageSize> class SmallUniquePtr {
	/* This class is analog to unique_ptr<T>, and has a similar API.
	 * However it has a local buffer used instead of allocation if the type is small enough.
	 *
	 * Only polymorphic T types are supported (virtual).
	 * Non polymorphic classes do not benefit from this class.
	 * Either construct them as is, or use unique_ptr (size is fixed).
	 *
	 * To support move operations, T must have a virtual move function.
	 * These functions are automatically generated by inheriting from [..]MakeMovable classes.
	 * A moved/release SmallUniquePtr is nullptr.
	 *
	 * Swap is not provided (complex and seldom used).
	 * Using the pointer as key in a data structure is discouraged, as it can change if the
	 * SmallUniquePtr is moved (if inline).
	 * For this reason, comparison operators and hash function are not provided.
	 */
private:
	static_assert (std::is_polymorphic<T>::value,
	               "This class should only be used for polymorphic types");
	using StorageType = typename std::aligned_storage<StorageSize>::type;

public:
	using type = T;
	using pointer = type *;
	using const_pointer = const type *;
	static constexpr auto storage_align = alignof (StorageType);
	static constexpr auto storage_size = StorageSize;

	// Constructors
	SmallUniquePtr () = default;
	SmallUniquePtr (std::nullptr_t) noexcept : SmallUniquePtr () {}
	SmallUniquePtr (pointer p) noexcept : data_ (p) {}

	SmallUniquePtr (const SmallUniquePtr &) = delete;
	template <typename U, std::size_t OtherStorageSize>
	SmallUniquePtr (SmallUniquePtr<U, OtherStorageSize> && other) noexcept {
		move_from_other (std::move (other));
	}

	template <typename U, typename... Args> SmallUniquePtr (in_place_type_t<U>, Args &&... args) {
		build<U> (std::forward<Args> (args)...);
	}
	template <typename U, typename V, typename... Args>
	SmallUniquePtr (in_place_type_t<U>, std::initializer_list<V> ilist, Args &&... args) {
		build<U> (ilist, std::forward<Args> (args)...);
	}

	~SmallUniquePtr () { reset (); }

	SmallUniquePtr & operator= (const SmallUniquePtr &) = delete;
	template <typename U, std::size_t OtherStorageSize>
	SmallUniquePtr & operator= (SmallUniquePtr<U, OtherStorageSize> && other) noexcept {
		reset ();
		move_from_other (std::move (other));
		return *this;
	}
	SmallUniquePtr & operator= (std::nullptr_t) noexcept {
		reset ();
		return *this;
	}
	SmallUniquePtr & operator= (pointer p) noexcept {
		reset (p);
		return *this;
	}

	// Modifiers
	pointer release () noexcept {
		if (data_) {
			if (is_allocated ()) {
				return release_pointer_unsafe ();
			} else {
				auto allocated_storage = create_storage_helper (storage_size, std::false_type{});
				move_to (allocated_storage);
				return static_cast<pointer> (allocated_storage);
			}
		} else {
			return nullptr;
		}
	}
	void reset () noexcept {
		if (data_) {
			if (is_allocated ())
				delete data_;
			else
				data_->~type ();
			data_ = nullptr;
		}
	}
	void reset (pointer p) noexcept {
		reset ();
		data_ = p;
	}
	template <typename U = type, typename... Args> void reset (in_place_type_t<U>, Args &&... args) {
		emplace<U> (std::forward<Args> (args)...);
	}
	template <typename U = type, typename V, typename... Args>
	void reset (in_place_type_t<U>, std::initializer_list<V> ilist, Args &&... args) {
		emplace<U> (ilist, std::forward<Args> (args)...);
	}

	template <typename U = type, typename... Args> void emplace (Args &&... args) {
		reset ();
		build<U> (std::forward<Args> (args)...);
	}
	template <typename U = type, typename V, typename... Args>
	void emplace (std::initializer_list<V> ilist, Args &&... args) {
		reset ();
		build<U> (ilist, std::forward<Args> (args)...);
	}

	// Observers
	constexpr pointer get () const noexcept { return data_; }
	constexpr operator bool () const noexcept { return data_; }
	pointer operator-> () const noexcept { return data_; }
	add_lvalue_reference_t<type> operator* () const noexcept { return *data_; }

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
		       data_as_byte < inline_storage_as_byte + storage_size;
	}
	bool is_allocated () const noexcept { return !is_inline (); }

	// Public internals used for move/release
	pointer release_pointer_unsafe () noexcept {
		// Assume is_allocated ()
		auto tmp = data_;
		data_ = nullptr;
		return tmp;
	}
	void move_to (void * storage) noexcept {
		// Assume is_inline ()
		static_assert (std::is_base_of<SmallUniquePtrMakeMovableBase, type>::value,
		               "move support requires that T derives from SmallUniquePtrMakeMovableBase");
		data_->small_unique_ptr_move (storage);
		data_->~type ();
		data_ = nullptr;
	}

private:
	/* Condition used to determine if a type U can be built inline.
	 * This is not an equivalence: a type U could be built inline and be stored allocated.
	 * (for example if result of a move)
	 */
	template <std::size_t Size, std::size_t Align>
	using BuildInline = bool_constant<(Size <= storage_size && Align <= storage_align)>;
	template <typename U> using BuildTypeInline = BuildInline<sizeof (U), alignof (U)>;

	// create_storage_helper(alloc_size, use_inline_storage)
	void * create_storage_helper (std::size_t, std::true_type) noexcept { return &inline_storage_; }
	static void * create_storage_helper (std::size_t size, std::false_type) {
		return ::operator new (size);
	}

	// Create storage for a type U
	template <typename U> void * create_storage_for_build () {
		return create_storage_helper (sizeof (U), BuildTypeInline<U>{});
	}

	// Create storage of at least Size
	template <typename FromPtr> void * create_storage_for_move () {
		return create_storage_helper (FromPtr::storage_size,
		                              BuildInline<FromPtr::storage_size, FromPtr::storage_align>{});
	}

	// Static Deleter of storage for a type U
	template <typename U> static void destroy_storage (void * storage) {
		destroy_storage_helper (storage, BuildTypeInline<U>{});
	}
	static void destroy_storage_helper (void *, std::true_type) {}
	static void destroy_storage_helper (void * storage, std::false_type) {
		::operator delete (storage);
	}

	template <typename U, typename... Args> void build (Args &&... args) {
		static_assert (std::is_base_of<type, U>::value, "build object must derive from T");
		// tmp serves as a RAII temporary storage for the buffer : storage is cleaned on Constr error.
		Detail::DestroyPointerAtEndOfScope<void (&) (void *), destroy_storage<U>> tmp{
		    create_storage_for_build<U> ()};
		new (tmp.get ()) U (std::forward<Args> (args)...);
		data_ = static_cast<pointer> (tmp.release ());
	}
	template <typename U, std::size_t OtherStorageSize>
	void move_from_other (SmallUniquePtr<U, OtherStorageSize> && other) noexcept {
		static_assert (std::is_base_of<type, U>::value,
		               "can only move from SmallUniquePtr<U> to SmallUniquePtr<T> if U derives from T");
		if (other) {
			if (other.is_allocated ()) {
				data_ = other.release_pointer_unsafe (); // Just steal pointer
			} else {
				// Call the move constructor to change the storage (no raii as move is noexcept)
				auto tmp = create_storage_for_move<SmallUniquePtr<U, OtherStorageSize>> ();
				other.move_to (tmp);
				data_ = static_cast<pointer> (tmp);
			}
		}
	}

	pointer data_{nullptr}; // Points to the T object, not the chosen storage
	StorageType inline_storage_;
};

// Creates a SmallUniquePtr<T, StorageSize> built in place (requires move support!)
template <typename T, std::size_t StorageSize = sizeof (T), typename... Args>
SmallUniquePtr<T, StorageSize> make_small_unique (Args &&... args) {
	return SmallUniquePtr<T, StorageSize>{in_place_type_t<T>{}, std::forward<Args> (args)...};
}
} // namespace duck
