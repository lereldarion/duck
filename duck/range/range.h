#pragma once

// Range V2
// STATUS: WIP

#include <duck/type_traits.h>
#include <iterator>
#include <utility>

namespace duck {
namespace Range {
	/* Ranges base header file.
	 * Provide a common interface for iterables.
	 *
	 * A range represents something which can be iterated upon.
	 * The range is constant : after creation, they cannot be modified.
	 * The iterated objects can be modified however, if supported by the objects
	 * (const_iterator VS iterator).
	 * Some range may be assignable, but this is not guaranteed to be true.
	 *
	 * Ranges are created by calling range() on supported arguments.
	 * range() will select the right range class to use.
	 * This file provides most basic overloads of range().
	 *
	 * Most represent a reference to an iterable (range(vec)).
	 * Some store the object itself (range of a temporary object).
	 * The object in this case becomes constant and cannot be retrieved.
	 * To retrieve the object, store the temporary then call range on it (reference).
	 *
	 * TODO combinators.
	 * TODO algorithm.
	 */

	/*********************************************************************************
	 * Type traits.
	 */

	// Is an iterator: std::iterator_traits<It> is SFINAE compatible
	template <typename It, typename = void> struct IsIterator : std::false_type {};
	template <typename It>
	struct IsIterator<It, VoidT<typename std::iterator_traits<It>::iterator_category>>
	    : std::true_type {};

	namespace UnifiedCall {
		// namespace representing the "using std::sth; sth(object);" pattern.
		using std::begin;
		using std::end;
		template <typename T> auto call_begin (T && t) -> decltype (begin (std::forward<T> (t)));
		template <typename T> auto call_end (T && t) -> decltype (end (std::forward<T> (t)));
	} // namespace UnifiedCall
	template <typename T>
	using IteratorTypeOf = decltype (UnifiedCall::call_begin (std::declval<T> ()));

	// Is iterable: if we can call begin & end on the object.
	template <typename T, typename = void> struct IsIterable : std::false_type {};
	template <typename T>
	struct IsIterable<T, VoidT<decltype (UnifiedCall::call_begin (std::declval<T> ())),
	                           decltype (UnifiedCall::call_end (std::declval<T> ()))>>
	    : std::true_type {};

	// Is container: has empty(), size() methods and is Iterable.
	template <typename T, typename = void> struct IsContainer : std::false_type {};
	template <typename T>
	struct IsContainer<
	    T, VoidT<decltype (std::declval<T> ().empty ()), decltype (std::declval<T> ().size ())>>
	    : IsIterable<T> {};

	/**********************************************************************************
	 * Provides typedefs for Range types (must be specialised).
	 * Member typedefs:
	 * - Iterator: iterator type
	 * - SizeType: type returned by size() method
	 */
	template <typename RangeType> struct RangeTraits;

	/* Type trait to test if this is a range type.
	 * Impl: tests if RangeTraits is defined.
	 */
	template <typename T, typename = void> struct IsRange : std::false_type {};
	template <typename T>
	struct IsRange<T, VoidT<typename RangeTraits<T>::Iterator>> : std::true_type {};

	/* Common base interface for range types.
	 * Intended to be used in a CRTP pattern extending a derived range class.
	 * Derived classes can override these functions if they have a better implementation.
	 *
	 * Ranges are not guaranteed to inherit from Base.
	 * To take a range argument, prefer using SFINAE restriction with IsRange trait.
	 */
	template <typename Derived> class Base {
	private:
		constexpr const Derived & derived () const { return static_cast<const Derived &> (*this); }

	public:
		constexpr bool empty () const { return derived ().begin () == derived ().end (); }

		constexpr typename RangeTraits<Derived>::SizeType size () const {
			return std::distance (derived ().begin (), derived ().end ());
		}

		// TODO add interface of old Range which does not generate ranges
	};

	/**********************************************************************************
	 * IteratorPair: most basic range, stores a pair of iterator.
	 */
	template <typename It> class IteratorPair;

	template <typename It> struct RangeTraits<IteratorPair<It>> {
		using Iterator = It;
		using SizeType = typename std::iterator_traits<It>::difference_type;
	};

	template <typename It> class IteratorPair : public Base<IteratorPair<It>> {
		static_assert (IsIterator<It>::value, "IteratorPair<It>: It must be a valid iterator type");

	private:
		It begin_;
		It end_;

	public:
		constexpr IteratorPair (It begin_it, It end_it)
		    : begin_ (std::move (begin_it)), end_ (std::move (end_it)) {}

		constexpr It begin () const { return begin_; }
		constexpr It end () const { return end_; }
	};

	template <typename It, typename = EnableIfV<IsIterator<It>>>
	constexpr IteratorPair<It> range (It begin_it, It end_it) {
		return {std::move (begin_it), std::move (end_it)};
	}

	/**********************************************************************************
	 * Integer range: iterates on [i, j[
	 */
	template <typename Int> class IntegerIterator {
		static_assert (std::is_integral<Int>::value,
		               "IntegerIterator<Int>: Int must be an integer type");

	public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type = Int;
		using difference_type = std::ptrdiff_t;
		using pointer = const value_type *;
		using reference = value_type; // Not standard compliant

		constexpr IntegerIterator () noexcept = default;
		constexpr IntegerIterator (Int n) noexcept : n_ (n) {}

		// Input / output
		IntegerIterator & operator++ () noexcept { return ++n_, *this; }
		constexpr reference operator* () const noexcept { return n_; }
		constexpr pointer operator-> () const noexcept { return &n_; }
		constexpr bool operator== (const IntegerIterator & o) const noexcept { return n_ == o.n_; }
		constexpr bool operator!= (const IntegerIterator & o) const noexcept { return n_ != o.n_; }

		// Forward
		IntegerIterator operator++ (int) noexcept {
			IntegerIterator tmp (*this);
			++*this;
			return tmp;
		}

		// Bidir
		IntegerIterator & operator-- () noexcept { return --n_, *this; }
		IntegerIterator operator-- (int) noexcept {
			IntegerIterator tmp (*this);
			--*this;
			return tmp;
		}

		// Random access
		IntegerIterator & operator+= (difference_type n) noexcept { return n_ += n, *this; }
		constexpr IntegerIterator operator+ (difference_type n) const noexcept {
			return IntegerIterator (n_ + n);
		}
		friend constexpr IntegerIterator operator+ (difference_type n,
		                                            const IntegerIterator & it) noexcept {
			return it + n;
		}
		IntegerIterator & operator-= (difference_type n) noexcept { return n_ -= n, *this; }
		constexpr IntegerIterator operator- (difference_type n) const noexcept {
			return IntegerIterator (n_ - n);
		}
		constexpr difference_type operator- (const IntegerIterator & o) const noexcept {
			return n_ - o.n_;
		}
		constexpr reference operator[] (difference_type n) const noexcept { return n_ + n; }
		constexpr bool operator< (const IntegerIterator & o) const noexcept { return n_ < o.n_; }
		constexpr bool operator> (const IntegerIterator & o) const noexcept { return n_ > o.n_; }
		constexpr bool operator<= (const IntegerIterator & o) const noexcept { return n_ <= o.n_; }
		constexpr bool operator>= (const IntegerIterator & o) const noexcept { return n_ >= o.n_; }

	private:
		Int n_{};
	};

	template <typename Int, typename = EnableIfV<std::is_integral<Int>>>
	constexpr IteratorPair<IntegerIterator<Int>> range (Int from, Int to) {
		return {IntegerIterator<Int>{from}, IntegerIterator<Int>{to}};
	}
	template <typename Int, typename = EnableIfV<std::is_integral<Int>>>
	constexpr IteratorPair<IntegerIterator<Int>> range (Int to) {
		return range (Int{0}, to);
	}

	/**********************************************************************************
	 * Iterable reference & value.
	 * Reference: const and mutable lvalue-references (T = const I& / I&).
	 * Value: store temporary object to guarantee its lifetime (T = I).
	 * This class is just a lazy iterator pair.
	 */
	template <typename T> class Iterable;

	// Utils to get base (decayed) stored type:
	// T = const I& / I& -> const I / I
	// T = I -> const I (a stored temporary is considered constant)
	template <typename T>
	using IterableBaseType = typename std::conditional<std::is_reference<T>::value,
	                                                   typename std::remove_reference<T>::type,
	                                                   typename std::add_const<T>::type>::type;

	// Same traits as the iterator pair.
	// std::remove_reference<T> gives us const I or I, to select I::const_iterator or I::iterator
	// TODO make a trait to select iterable base type
	template <typename T>
	struct RangeTraits<Iterable<T>> : RangeTraits<IteratorPair<IteratorTypeOf<IterableBaseType<T>>>> {
	};

	template <typename T> class Iterable : public Base<Iterable<T>> {
		static_assert (IsIterable<T>::value, "Iterable<T>: requires that T is iterable");
		static_assert (!std::is_rvalue_reference<T>::value,
		               "Iterable<T>: T must be 'const I&', 'I&', or 'I'");

	protected:
		T iterable_; // Reference for const I& / I&, object for I.

	public:
		// T = const I& / I& : T&& -> const I& / I &, just copy reference (reference collapsing)
		// T = I : T&& -> I&&, construct I by move
		constexpr Iterable (T && t) : iterable_ (std::forward<T> (t)) {}

		// T = const I& / I& : returns I::const_iterator / I::iterator
		// T = I : returns const_iterator, object is constant
		constexpr typename RangeTraits<Iterable<T>>::Iterator begin () const {
			using std::begin;
			return begin (iterable_);
		}
		constexpr typename RangeTraits<Iterable<T>>::Iterator end () const {
			using std::end;
			return end (iterable_);
		}
	};

	// range() overload
	// Matchings: const I& -> Iterable<const I&> ; I& -> Iterable<I&> ; I&& -> Iterable<I>.
	template <typename T, typename = EnableIf<IsIterable<T>::value && !IsContainer<T>::value>>
	constexpr Iterable<T> range (T && iterable) {
		return {std::forward<T> (iterable)};
	}

	/**********************************************************************************
	 * Container reference & value.
	 * Similar to iterable, but specialised for STL compatible containers.
	 * Provide optimized functions using the container directly (empty, size).
	 */
	template <typename C> class Container;

	// Traits
	template <typename C> struct RangeTraits<Container<C>> : RangeTraits<Iterable<C>> {
		using SizeType = typename IterableBaseType<C>::size_type;
	};

	template <typename C> class Container : public Iterable<C> {
		static_assert (IsContainer<C>::value, "Container<C>: C must be a container");

	protected:
		using Iterable<C>::iterable_;

	public:
		constexpr Container (C && c) : Iterable<C> (std::forward<C> (c)) {}

		constexpr bool empty () const { return iterable_.empty (); }
		constexpr typename RangeTraits<Container<C>>::SizeType size () const {
			return iterable_.size ();
		}
	};

	// range () overloads
	template <typename C, typename = EnableIfV<IsContainer<C>>>
	constexpr Container<C> range (C && container) {
		return {std::forward<C> (container)};
	}

		// TODO type tests !
		// TODO add special case to just propagate ranges in range()
		// TODO add a simple combinator for test !

#if 0
	template <typename It> class Range : public Base<It> {
		/* Represents a matching pair of iterators.
		 *
		 * This class has constant value semantics.
		 * It can not be modified, but new ranges can be built from it.
		 * It can be passed by value.
		 *
		 * This class has the same properties has the underlying iterator type.
		 * This applies for invalidation, and which operations can be used.
		 * Some operations may work inefficiently for low capability iterators (like size ()).
		 * Beware with less than forward iterators (risk of iterating on them in size()).
		 *
		 * TODO more slices (to_end, from_start).
		 * TODO checked operations ?
		 */
	public:
		using ValueType = typename std::iterator_traits<It>::value_type;
		using ReferenceType = typename std::iterator_traits<It>::reference;
		using DifferenceType = typename std::iterator_traits<It>::difference_type;
		using IteratorCategory = typename std::iterator_traits<It>::iterator_category;

		// Input/forward iterator
		constexpr ReferenceType front () const { return *begin (); }
		Range pop_front () const { return Base<It>{std::next (begin ()), end ()}; }

		// Bidirectional iterator
		ReferenceType back () const { return *std::prev (end ()); }
		Range pop_back () const { return Base<It>{begin (), std::prev (end ())}; }

		// Random access iterator
		ReferenceType operator[] (DifferenceType n) const { return begin ()[n]; }
		Range pop_front (DifferenceType n) const { return Base<It>{std::next (begin (), n), end ()}; }
		Range pop_back (DifferenceType n) const { return Base<It>{begin (), std::prev (end (), n)}; }

		// Interval-like API
		constexpr bool contains (It it) const { return begin () <= it && it < end (); }
		DifferenceType offset_of (It it) const { return std::distance (begin (), it); }

		// "nicer" api (python like slice ; but at(size ()) return end ())
		// TODO improve...
		It at (DifferenceType n) const {
			auto index = n < 0 ? n + size () : n;
			return std::next (begin (), index);
		}
		Range slice (DifferenceType from, DifferenceType to) const {
			return Base<It>{at (from), at (to)};
		}
		Range slice_to (DifferenceType to) const { return Base<It>{begin (), at (to)}; }
		Range slice_from (DifferenceType from) const { return Base<It>{at (from), end ()}; }

		// Build a container from this range
		template <typename Container> Container to_container () const {
			return Container (begin (), end ());
		}
	};
#endif
} // namespace Range

// Pull main range() function in namespace duck::
using Range::range;
} // namespace duck
