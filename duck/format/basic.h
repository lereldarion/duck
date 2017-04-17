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
	template <std::size_t N> class StaticCharArray : public ElementBase<StaticCharArray<N>> {
		// Reference to static char array (remove end '\0')
	public:
		explicit constexpr StaticCharArray (const char (&str)[N]) : str_ (str) {}
		constexpr std::size_t size () const { return N - 1; }
		template <typename OutputIt> constexpr OutputIt write (OutputIt it) const {
			return std::copy (std::begin (str_), std::end (str_) - 1, it);
		}

	private:
		const char (&str_)[N];
	};
	template <std::size_t N>
	constexpr StaticCharArray<N> format_element (const char (&str)[N], AdlTag) {
		return StaticCharArray<N>{str};
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

	class CStringRef : public ElementBase<CStringRef> {
		// Reference to c-string (const char *)
	public:
		CStringRef (const char * str, std::size_t len) : str_ (str), len_ (len) {}
		explicit CStringRef (const char * str) : CStringRef (str, std::strlen (str)) {}
		std::size_t size () const { return len_; }
		template <typename OutputIt> OutputIt write (OutputIt it) const {
			return std::copy_n (str_, len_, it);
		}

	private:
		const char * str_;
		std::size_t len_;
	};
	inline CStringRef format_element (const char * str, AdlTag) { return CStringRef{str}; }

	template <typename Int> class DecimalInteger : public ElementBase<DecimalInteger<Int>> {
		// Prints an integer in decimal base
	public:
		explicit DecimalInteger (Int i) : i_ (i) {}
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
}
}
