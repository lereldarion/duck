#pragma once

// Range V3 combinators
// STATUS: WIP

#include <algorithm>
#include <cassert>
#include <duck/range/range.h>
#include <limits>

namespace duck {
/********************************************************************************
 * Type traits.
 */
template <typename It, typename Category>
using is_iterator_of_category = std::is_base_of<Category, iterator_category_t<It>>;

// Is callable
template <typename F, typename Arg, typename = void> struct is_unary_invocable : std::false_type {};
template <typename F, typename Arg>
struct is_unary_invocable<F, Arg, void_t<decltype (std::declval<F> () (std::declval<Arg> ()))>>
    : std::true_type {};

// Is predicate
template <typename Predicate, typename Arg, typename = void>
struct is_unary_predicate : std::false_type {};
template <typename Predicate, typename Arg>
struct is_unary_predicate<
    Predicate, Arg,
    void_t<decltype (static_cast<bool> (std::declval<Predicate> () (std::declval<Arg> ())))>>
    : std::true_type {};

/********************************************************************************
 * Pop front.
 */
template <typename R> class pop_front_range {
	static_assert (is_range<R>::value, "pop_front_range<R>: R must be a range");

public:
	using iterator = range_iterator_t<R>;

	pop_front_range (R && r, iterator_difference_t<iterator> n)
	    : inner_ (std::forward<R> (r)), n_ (n) {
		assert (n_ >= 0);
		assert (n_ <= duck::size (inner_));
	}

	iterator begin () const { return std::next (duck::adl_begin (inner_), n_); }
	iterator end () const { return duck::adl_end (inner_); }
	iterator_difference_t<iterator> size () const { return duck::size (inner_) - n_; }

private:
	R inner_;
	iterator_difference_t<iterator> n_;
};

template <typename R>
pop_front_range<R> pop_front (R && r, iterator_difference_t<range_iterator_t<R>> n) {
	return {std::forward<R> (r), n};
}

struct pop_front_range_tag {
	std::ptrdiff_t n;
};
inline pop_front_range_tag pop_front (std::ptrdiff_t n = 1) {
	return {n};
}
template <typename R> pop_front_range<R> operator| (R && r, pop_front_range_tag tag) {
	return {std::forward<R> (r), static_cast<iterator_difference_t<range_iterator_t<R>>> (tag.n)};
}

/********************************************************************************
 * Pop back.
 */
namespace internal_range {
	template <typename R>
	range_iterator_t<R> nth_iterator_from_end_impl (R && r,
	                                                iterator_difference_t<range_iterator_t<R>> n,
	                                                std::bidirectional_iterator_tag) {
		return std::prev (end (std::forward<R> (r)), n);
	}
	template <typename R>
	range_iterator_t<R> nth_iterator_from_end_impl (R && r,
	                                                iterator_difference_t<range_iterator_t<R>> n,
	                                                std::forward_iterator_tag) {
		auto s = size (r);
		return std::next (begin (std::forward<R> (r)), s - n);
	}
	template <typename R>
	range_iterator_t<R> nth_iterator_from_end (R && r, iterator_difference_t<range_iterator_t<R>> n) {
		return nth_iterator_from_end_impl (std::forward<R> (r), n,
		                                   iterator_category_t<range_iterator_t<R>>{});
	}
} // namespace internal_range

template <typename R> class pop_back_range {
	static_assert (is_range<R>::value, "pop_back_range<R>: R must be a range");

public:
	using iterator = range_iterator_t<R>;

	pop_back_range (R && r, iterator_difference_t<iterator> n)
	    : inner_ (std::forward<R> (r)), n_ (n) {
		assert (n_ >= 0);
		assert (n_ <= duck::size (inner_));
	}

	iterator begin () const { return duck::adl_begin (inner_); }
	iterator end () const { return internal_range::nth_iterator_from_end (inner_, n_); }
	iterator_difference_t<iterator> size () const { return duck::size (inner_) - n_; }

private:
	R inner_;
	iterator_difference_t<iterator> n_;
};

template <typename R>
pop_back_range<R> pop_back (R && r, iterator_difference_t<range_iterator_t<R>> n) {
	return {std::forward<R> (r), n};
}

struct pop_back_range_tag {
	std::ptrdiff_t n;
};
inline pop_back_range_tag pop_back (std::ptrdiff_t n = 1) {
	return {n};
}
template <typename R> pop_back_range<R> operator| (R && r, pop_back_range_tag tag) {
	return {std::forward<R> (r), static_cast<iterator_difference_t<range_iterator_t<R>>> (tag.n)};
}

/********************************************************************************
 * General python like slicing (-n : n from end).
 * Transformed indexes must be in order.
 */
template <typename R> class slice_range {
	static_assert (is_range<R>::value, "slice_range<R>: R must be a range");

public:
	using iterator = range_iterator_t<R>;

	slice_range (R && r, iterator_difference_t<iterator> from, iterator_difference_t<iterator> to)
	    : inner_ (std::forward<R> (r)), from_ (from), to_ (to) {
		assert (internal_range::normalize_index (inner_, from_) >= 0);
		assert (internal_range::normalize_index (inner_, from_) <=
		        internal_range::normalize_index (inner_, to_));
		assert (internal_range::normalize_index (inner_, to_) <= duck::size (inner_));
	}

	iterator begin () const { return at (inner_, from_); }
	iterator end () const { return at (inner_, to_); }
	iterator_difference_t<iterator> size () const {
		return internal_range::normalize_index (inner_, to_) -
		       internal_range::normalize_index (inner_, from_);
	}

private:
	R inner_;
	iterator_difference_t<iterator> from_;
	iterator_difference_t<iterator> to_;
};

template <typename R> slice_range<R> slice (R && r, iterator_difference_t<range_iterator_t<R>> n) {
	return {std::forward<R> (r), n};
}

struct slice_range_tag {
	std::ptrdiff_t from;
	std::ptrdiff_t to;
};
inline slice_range_tag slice (std::ptrdiff_t from, std::ptrdiff_t to) {
	return {from, to};
}
template <typename R> slice_range<R> operator| (R && r, slice_range_tag tag) {
	return {std::forward<R> (r), static_cast<iterator_difference_t<range_iterator_t<R>>> (tag.from),
	        static_cast<iterator_difference_t<range_iterator_t<R>>> (tag.to)};
}

/********************************************************************************
 * Reverse order.
 */
template <typename R> class reverse_range {
	static_assert (is_range<R>::value, "reverse_range<R>: R must be a range");
	static_assert (
	    is_iterator_of_category<range_iterator_t<R>, std::bidirectional_iterator_tag>::value,
	    "reverse_range<R>: R must be at least bidirectional_iterator");

public:
	using iterator = std::reverse_iterator<range_iterator_t<R>>;

	reverse_range (R && r) : inner_ (std::forward<R> (r)) {}

	iterator begin () const { return iterator{duck::adl_end (inner_)}; }
	iterator end () const { return iterator{duck::adl_begin (inner_)}; }
	iterator_difference_t<iterator> size () const { return duck::size (inner_); }

private:
	R inner_;
};

template <typename R> reverse_range<R> reverse (R && r) {
	return {std::forward<R> (r)};
}

struct reverse_range_tag {};
inline reverse_range_tag reverse () {
	return {};
}
template <typename R> reverse_range<R> operator| (R && r, reverse_range_tag) {
	return {std::forward<R> (r)};
}

/********************************************************************************
 * Indexed range.
 * Returned value_type has an index field, and a value() member.
 * end() iterator index is UB.
 */
template <typename It, typename Int> class indexed_iterator {
	static_assert (is_iterator<It>::value, "indexed_iterator<It, Int>: It must be an iterator type");
	static_assert (std::is_integral<Int>::value, "indexed_iterator<It, Int>: Int must be integral");

public:
	using iterator_category = iterator_category_t<It>;
	struct value_type {
		It it;
		Int index;
		value_type () = default;
		value_type (It it_arg, Int index_arg) : it (it_arg), index (index_arg) {}
		iterator_reference_t<It> value () const { return *it; }
	};
	using difference_type = iterator_difference_t<It>;
	using pointer = const value_type *;
	using reference = value_type;

	indexed_iterator () = default;
	indexed_iterator (It it, Int index) : d_ (it, index) {}

	It base () const { return d_.it; }

	// Input / output
	indexed_iterator & operator++ () { return ++d_.it, ++d_.index, *this; }
	reference operator* () const { return d_; }
	pointer operator-> () const { return &d_; }
	bool operator== (const indexed_iterator & o) const { return d_.it == o.d_.it; }
	bool operator!= (const indexed_iterator & o) const { return d_.it != o.d_.it; }

	// Forward
	indexed_iterator operator++ (int) {
		indexed_iterator tmp (*this);
		++*this;
		return tmp;
	}

	// Bidir
	indexed_iterator & operator-- () { return --d_.it, --d_.index, *this; }
	indexed_iterator operator-- (int) {
		indexed_iterator tmp (*this);
		--*this;
		return tmp;
	}

	// Random access
	indexed_iterator & operator+= (difference_type n) { return d_.it += n, d_.index += n, *this; }
	indexed_iterator operator+ (difference_type n) const {
		return indexed_iterator (d_.it + n, d_.index + n);
	}
	friend indexed_iterator operator+ (difference_type n, const indexed_iterator & it) {
		return it + n;
	}
	indexed_iterator & operator-= (difference_type n) { return d_.it -= n, d_.index -= n, *this; }
	indexed_iterator operator- (difference_type n) const {
		return indexed_iterator (d_.it - n, d_.index - n);
	}
	difference_type operator- (const indexed_iterator & o) const { return d_.it - o.d_.it; }
	value_type operator[] (difference_type n) const { return *(*this + n); }
	bool operator< (const indexed_iterator & o) const { return d_.it < o.d_.it; }
	bool operator> (const indexed_iterator & o) const { return d_.it > o.d_.it; }
	bool operator<= (const indexed_iterator & o) const { return d_.it <= o.d_.it; }
	bool operator>= (const indexed_iterator & o) const { return d_.it >= o.d_.it; }

private:
	value_type d_{};
};

template <typename R, typename Int> class indexed_range {
	static_assert (is_range<R>::value, "indexed_range<R, Int>: R must be a range");
	static_assert (std::is_integral<Int>::value,
	               "indexed_range<R, Int>: Int must be an integral type");

public:
	using iterator = indexed_iterator<range_iterator_t<R>, Int>;

	indexed_range (R && r) : inner_ (std::forward<R> (r)) {}

	iterator begin () const { return {duck::adl_begin (inner_), 0}; }
	iterator end () const { return {duck::adl_end (inner_), std::numeric_limits<Int>::max ()}; }
	iterator_difference_t<iterator> size () const { return duck::size (inner_); }

private:
	R inner_;
};

template <typename Int, typename R> indexed_range<R, Int> index (R && r) {
	return {std::forward<R> (r)};
}

template <typename Int> struct indexed_range_tag {};
template <typename Int = int> indexed_range_tag<Int> indexed () {
	return {};
}
template <typename Int, typename R>
indexed_range<R, Int> operator| (R && r, indexed_range_tag<Int>) {
	return {std::forward<R> (r)};
}

/********************************************************************************
 * Filter with a predicate.
 */
template <typename R, typename Predicate> class filtered_range {
	static_assert (is_range<R>::value, "filtered_range<R, Predicate>: R must be a range");
	static_assert (is_unary_predicate<Predicate, iterator_reference_t<range_iterator_t<R>>>::value,
	               "filtered_range<R, Predicate>: Predicate must be callable on R values");

public:
	using inner_iterator = range_iterator_t<R>;
	class iterator {
	public:
		// At most this is a bidirectional_iterator
		using iterator_category =
		    common_type_t<std::bidirectional_iterator_tag, iterator_category_t<inner_iterator>>;
		using value_type = iterator_value_type_t<inner_iterator>;
		using difference_type = iterator_difference_t<inner_iterator>;
		using pointer = iterator_pointer_t<inner_iterator>;
		using reference = iterator_reference_t<inner_iterator>;

		iterator () = default;
		iterator (inner_iterator it, const filtered_range & range) : it_ (it), range_ (&range) {}

		inner_iterator base () const { return it_; }

		// Input / output
		iterator & operator++ () { return it_ = range_->next_after (it_), *this; }
		reference operator* () const { return *it_; }
		pointer operator-> () const { return it_.operator-> (); }
		bool operator== (const iterator & o) const { return it_ == o.it_; }
		bool operator!= (const iterator & o) const { return it_ != o.it_; }

		// Forward
		iterator operator++ (int) {
			iterator tmp (*this);
			++*this;
			return tmp;
		}

		// Bidir
		iterator & operator-- () { return it_ = range_->previous_before (it_), *this; }
		iterator operator-- (int) {
			iterator tmp (*this);
			--*this;
			return tmp;
		}

	private:
		inner_iterator it_{};
		const filtered_range * range_{nullptr};
	};

	filtered_range (R && r, const Predicate & predicate)
	    : inner_ (std::forward<R> (r)), predicate_ (predicate) {}
	filtered_range (R && r, Predicate && predicate)
	    : inner_ (std::forward<R> (r)), predicate_ (std::move (predicate)) {}

	iterator begin () const { return {next (duck::adl_begin (inner_)), *this}; }
	iterator end () const { return {duck::adl_end (inner_), *this}; }

private:
	inner_iterator next (inner_iterator from) const {
		return std::find_if (from, duck::adl_end (inner_), predicate_);
	}
	inner_iterator next_after (inner_iterator from) const {
		if (from == duck::adl_end (inner_))
			return from;
		else
			return next (++from);
	}
	inner_iterator previous_before (inner_iterator from) const {
		auto b = duck::adl_begin (inner_);
		if (from == b) {
			return from;
		} else {
			// Try to find a match before from
			// If this fails, from was first, return it
			using RevIt = std::reverse_iterator<inner_iterator>;
			auto rev_end = RevIt{b};
			auto rev_next = std::find_if (RevIt{--from}, rev_end, predicate_);
			if (rev_next != rev_end)
				return rev_next.base ();
			else
				return from;
		}
	}

	R inner_;
	Predicate predicate_;
};

template <typename R, typename Predicate>
filtered_range<R, remove_cvref_t<Predicate>> filter (R && r, Predicate && predicate) {
	return {std::forward<R> (r), std::forward<Predicate> (predicate)};
}

template <typename Predicate> struct filtered_range_tag {
	filtered_range_tag (const Predicate & p) : predicate (p) {}
	filtered_range_tag (Predicate && p) : predicate (std::move (p)) {}
	Predicate predicate;
};
template <typename Predicate>
filtered_range_tag<remove_cvref_t<Predicate>> filter (Predicate && predicate) {
	return {std::forward<Predicate> (predicate)};
}
template <typename R, typename Predicate>
filtered_range<R, Predicate> operator| (R && r, filtered_range_tag<Predicate> tag) {
	return {std::forward<R> (r), std::move (tag.predicate)};
}

/********************************************************************************
 * Processed range.
 * Apply function f to each element.
 */
template <typename R, typename Function> class mapped_range {
	static_assert (is_range<R>::value, "mapped_range<R, Function>: R must be a range");
	static_assert (is_unary_invocable<Function, iterator_reference_t<range_iterator_t<R>>>::value,
	               "mapped_range<R, Function>: Function must be callable on R values");

public:
	class iterator {
	public:
		using inner_iterator = range_iterator_t<R>;
		using iterator_category = iterator_category_t<inner_iterator>;
		using value_type = invoke_result_t<Function, iterator_reference_t<inner_iterator>>;
		using difference_type = iterator_difference_t<inner_iterator>;
		using pointer = void;
		using reference = value_type; // No way to take references on function_ result

		iterator () = default;
		iterator (inner_iterator it, const mapped_range & range) : it_ (it), range_ (&range) {}

		inner_iterator base () const { return it_; }

		// Input / output
		iterator & operator++ () { return ++it_, *this; }
		reference operator* () const { return range_->function_ (*it_); }
		// No operator->
		bool operator== (const iterator & o) const { return it_ == o.it_; }
		bool operator!= (const iterator & o) const { return it_ != o.it_; }

		// Forward
		iterator operator++ (int) {
			iterator tmp (*this);
			++*this;
			return tmp;
		}

		// Bidir
		iterator & operator-- () { return --it_, *this; }
		iterator operator-- (int) {
			iterator tmp (*this);
			--*this;
			return tmp;
		}

		// Random access
		iterator & operator+= (difference_type n) { return it_ += n, *this; }
		iterator operator+ (difference_type n) const { return iterator (it_ + n, range_); }
		friend iterator operator+ (difference_type n, const iterator & it) { return it + n; }
		iterator & operator-= (difference_type n) { return it_ -= n, *this; }
		iterator operator- (difference_type n) const { return iterator (it_ - n, range_); }
		difference_type operator- (const iterator & o) const { return it_ - o.it_; }
		reference operator[] (difference_type n) const { return *(*this + n); }
		bool operator< (const iterator & o) const { return it_ < o.it_; }
		bool operator> (const iterator & o) const { return it_ > o.it_; }
		bool operator<= (const iterator & o) const { return it_ <= o.it_; }
		bool operator>= (const iterator & o) const { return it_ >= o.it_; }

	private:
		inner_iterator it_{};
		const mapped_range * range_{nullptr};
	};

	mapped_range (R && r, const Function & function)
	    : inner_ (std::forward<R> (r)), function_ (function) {}
	mapped_range (R && r, Function && function)
	    : inner_ (std::forward<R> (r)), function_ (std::move (function)) {}

	iterator begin () const { return {duck::adl_begin (inner_), *this}; }
	iterator end () const { return {duck::adl_end (inner_), *this}; }
	iterator_difference_t<iterator> size () const { return duck::size (inner_); }

private:
	R inner_;
	Function function_;
};

template <typename R, typename Function>
mapped_range<R, remove_cvref_t<Function>> map (R && r, Function && function) {
	return {std::forward<R> (r), std::forward<Function> (function)};
}

template <typename Function> struct mapped_range_tag {
	mapped_range_tag (const Function & f) : function (f) {}
	mapped_range_tag (Function && f) : function (std::move (f)) {}
	Function function;
};
template <typename Function> mapped_range_tag<remove_cvref_t<Function>> map (Function && function) {
	return {std::forward<Function> (function)};
}
template <typename R, typename Function>
mapped_range<R, Function> operator| (R && r, mapped_range_tag<Function> tag) {
	return {std::forward<R> (r), std::move (tag.function)};
}
} // namespace duck
