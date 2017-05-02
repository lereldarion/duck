#pragma once

// Formatting library: basic formatting elements.

#include <algorithm> // std::copy
#include <cstring>   // printing char* (strlen)
#include <duck/format/core.h>
#include <limits> // printing ints
#include <string> // printing string
#include <type_traits>
#include <utility> // forward / move

namespace duck {
namespace Format {
	/* Reference or cheap value basic element formatters.
	 * Hold either a reference (string), or a very cheap value to the object (int / char).
	 */

	class SingleChar : public ElementBase<SingleChar> {
		// Single character (does not support multibyte chararcters !)
	public:
		explicit constexpr SingleChar (char c) : c_ (c) {}
		constexpr std::size_t size () const { return 1; }
		template <typename OutputIt> OutputIt write (OutputIt it) const {
			*it = c_;
			return ++it;
		}

	private:
		char c_;
	};
	constexpr SingleChar format_element (char c, AdlTag) { return SingleChar (c); }

	template <std::size_t N> class StaticCharArray : public ElementBase<StaticCharArray<N>> {
		/* Reference to static char array (remove end '\0')
		 * Warning, const char(&)[N] decays to const char* very easily.
		 * duck::format ("blah") works.
		 * auto & s = "blah"; duck::format (s) too.
		 * auto s = "blah"; duck::format (s) is CStringRef.
		 */
	public:
		explicit constexpr StaticCharArray (const char (&str)[N]) : str_ (str) {}
		constexpr std::size_t size () const { return N - 1; }
		template <typename OutputIt> OutputIt write (OutputIt it) const {
			return std::copy (std::begin (str_), std::end (str_) - 1, it);
		}

	private:
		const char (&str_)[N];
	};
	template <std::size_t N>
	constexpr StaticCharArray<N> format_element (const char (&str)[N], AdlTag) {
		return StaticCharArray<N>{str};
	}

	class CStringRef : public ElementBase<CStringRef> {
		/* Reference to c-string (const char *).
		 * The format_element is template only to let StaticCharArray have priority.
		 * (without template, it is a better match than StaticCharArray and is always selected).
		 */
	public:
		constexpr CStringRef (const char * str, std::size_t len) : str_ (str), len_ (len) {}
		explicit CStringRef (const char * str) : CStringRef (str, std::strlen (str)) {}
		constexpr std::size_t size () const { return len_; }
		template <typename OutputIt> OutputIt write (OutputIt it) const {
			return std::copy_n (str_, len_, it);
		}

	private:
		const char * str_;
		std::size_t len_;
	};
	template <typename T,
	          typename = typename std::enable_if<std::is_convertible<T, const char *>::value>::type>
	CStringRef format_element (T str, AdlTag) {
		return CStringRef{str};
	}

	class StringRef : public ElementBase<StringRef> {
		// Reference to std::string
	public:
		explicit StringRef (const std::string & str) : str_ (str) {}
		std::size_t size () const { return str_.size (); }
		template <typename OutputIt> OutputIt write (OutputIt it) const {
			return std::copy (std::begin (str_), std::end (str_), it);
		}

	private:
		const std::string & str_;
	};
	inline StringRef format_element (const std::string & str, AdlTag) { return StringRef{str}; }

	class Bool : public ElementBase<Bool> {
		// Prints a bool value
	public:
		explicit constexpr Bool (bool b) : b_ (b) {}
		constexpr std::size_t size () const { return b_ ? 4 : 5; }
		template <typename OutputIt> OutputIt write (OutputIt it) const {
			return b_ ? format_element ("true", AdlTag{}).write (it)
			          : format_element ("false", AdlTag{}).write (it);
		}

	private:
		bool b_;
	};
	constexpr Bool format_element (bool b, AdlTag) { return Bool (b); }

	template <typename Int> class DecimalInteger : public ElementBase<DecimalInteger<Int>> {
		// Prints an integer in decimal base
	public:
		explicit constexpr DecimalInteger (Int i) : i_ (i) {}
		std::size_t size () const {
			Int i = i_;
			if (i == 0)
				return 1; // "0"
			std::size_t digits = 0;
			if (i < 0) {
				i = -i;
				++digits;
			}
			while (i != 0) {
				++digits;
				i /= 10;
			}
			return digits;
		}
		template <typename OutputIt> OutputIt write (OutputIt it) const {
			Int i = i_;
			if (i == 0) {
				*it = '0';
				return ++it;
			}
			if (i < 0) {
				i = -i;
				*it++ = '-';
			}
			char buf[max_digits];
			auto buf_it = std::end (buf);
			while (i != 0) {
				*--buf_it = (i % 10) + '0';
				i /= 10;
			}
			return std::copy (buf_it, std::end (buf), it);
		}

	private:
		static constexpr int max_digits = std::numeric_limits<Int>::digits10 + 1;
		Int i_;
	};
	template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
	DecimalInteger<T> format_element (T t, AdlTag) {
		return DecimalInteger<T>{t};
	}

	// TODO different base formatters

	/* Store a std::string value (may be costly to copy).
	 * This formatter should be used if it lives longer than the std::string.
	 * The default formatter for std::string is StringRef (less costly).
	 * An overload of format_element is available, for temporary string objects (move).
	 */
	class StringValue : public ElementBase<StringValue> {
	public:
		explicit StringValue (std::string str) : str_ (std::move (str)) {}
		std::size_t size () const { return str_.size (); }
		template <typename OutputIt> OutputIt write (OutputIt it) const {
			return std::copy (str_.begin (), str_.end (), it);
		}

	private:
		std::string str_;
	};
	inline StringValue format_element (std::string && str, AdlTag) {
		return StringValue (std::move (str));
	}

	/* TODO join with seps ?
	 * combinator, not easy
	 */
}
}