#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <duck/range/combinator.h>
#include <duck/range/range.h>
#include <forward_list>
#include <iterator>
#include <list>
#include <vector>

// FIXME
#include <iostream>
template <typename Derived>
static std::ostream & operator<< (std::ostream & os, const duck::Range::Base<Derived> & r) {
	os << "Range(";
	for (const auto & v : r.derived ())
		os << v << ",";
	return os << ")";
}

TYPE_TO_STRING (std::vector<int>);
TYPE_TO_STRING (std::list<int>);
TYPE_TO_STRING (std::forward_list<int>);

using BidirContainers = doctest::Types<std::vector<int>, std::list<int>>;
using ForwardContainers = doctest::Types<std::vector<int>, std::list<int>, std::forward_list<int>>;

namespace DRC = duck::Range::Combinator;
const auto values = {0, 1, 2, 3, 4};

TEST_CASE_TEMPLATE ("reverse", Container, BidirContainers) {
	auto range = duck::range (Container{values}) | DRC::reverse ();
	CHECK (range.size () == values.size ());
	using RevIt = std::reverse_iterator<typename std::initializer_list<int>::iterator>;
	CHECK (range == duck::range (RevIt{values.end ()}, RevIt{values.begin ()}));

	CHECK ((duck::range (Container{}) | DRC::reverse ()).empty ());
}

TEST_CASE_TEMPLATE ("index", Container, ForwardContainers) {
	for (auto & iv : duck::range (Container{values}) | DRC::index<int> ()) {
		CHECK (iv.index == iv.value ());
	}
	// Empty, also check that index<Int> has a default
	CHECK ((duck::range (Container{}) | DRC::index ()).empty ());
}

TEST_CASE_TEMPLATE ("filter", Container, ForwardContainers) {
	auto r = duck::range (Container{values});

	auto filtered_range = r | DRC::filter ([](int i) { return i % 2 == 0; });
	CHECK (filtered_range.size () == 3);
	CHECK (filtered_range == duck::range ({0, 2, 4}));

	auto chained_filtered_range =
	    r | DRC::filter ([](int i) { return i < 2; }) | DRC::filter ([](int i) { return i > 0; });
	CHECK (chained_filtered_range.size () == 1);
	CHECK (chained_filtered_range.front () == 1);

	CHECK ((duck::range (Container{}) | DRC::filter ([](int) { return true; })).empty ());
}

TEST_CASE_TEMPLATE ("apply", Container, ForwardContainers) {
	auto applied_range = duck::range (Container{values}) | DRC::apply ([](int i) { return i - 2; }) |
	                     DRC::filter ([](int i) { return i >= 0; });
	CHECK (applied_range.size () == 3);
	CHECK (applied_range == duck::range (0, 3));

	CHECK ((duck::range (Container{}) | DRC::apply ([](int i) { return i; })).empty ());
}
