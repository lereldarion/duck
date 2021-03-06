#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include <type_traits>

#include <duck/small_unique_ptr.h>

namespace NoMoveSupport {
struct Base {
	virtual ~Base () = default;
	virtual int f () const = 0;
};
struct SmallDerived : public Base {
	int i_;
	SmallDerived (int i = 0) noexcept : i_ (i) {}
	int f () const noexcept override { return i_; }
};
struct BigDerived : public Base {
	int a_[4];
	int f () const noexcept override { return -1; }
};
} // namespace NoMoveSupport

TEST_CASE ("construction and access (flat hierarchy)") {
	using namespace NoMoveSupport;

	duck::SmallUniquePtr<Base, sizeof (SmallDerived)> p;
	CHECK (p.get () == nullptr);
	CHECK (!p);

	// Reset in place
	p.reset (duck::in_place_type_t<SmallDerived>{}, 42);
	CHECK (p);
	CHECK (p.is_inline ());
	CHECK (!p.is_allocated ());
	CHECK (p->f () == 42);

	// Emplace (equivalent to reset in place / test with big class)
	p.emplace<BigDerived> ();
	CHECK (p);
	CHECK (!p.is_inline ());
	CHECK (p.is_allocated ());
	CHECK (p->f () == -1);

	// Construction in place
	duck::SmallUniquePtr<Base, sizeof (SmallDerived)> inplace{duck::in_place_type_t<SmallDerived>{},
	                                                          4};
	CHECK (inplace);
	CHECK (inplace.is_inline ());
	CHECK (inplace->f () == 4);

	// Construction allocated
	duck::SmallUniquePtr<Base, sizeof (SmallDerived)> allocated{duck::in_place_type_t<BigDerived>{}};
	CHECK (allocated);
	CHECK (allocated.is_allocated ());
	CHECK (allocated->f () == -1);
}

namespace SupportsMove {
struct Base : public duck::SmallUniquePtrMakeMovableBase {
	virtual ~Base () = default;
	virtual int f () const = 0;
};
struct SmallDerived : public Base {
	DUCK_SMALL_UNIQUE_PTR_MAKE_MOVABLE;
	int i_;
	SmallDerived (int i = 0) noexcept : i_ (i) {}
	int f () const noexcept override { return i_; }
};
struct BigDerived : public Base {
	DUCK_SMALL_UNIQUE_PTR_MAKE_MOVABLE;
	int a_[4];
	int f () const noexcept override { return -1; }
};
} // namespace SupportsMove

TEST_CASE ("moves and releases (flat hierarchy)") {
	/* Checks more than the API.
	 * API just says that the object is moved to the other pointer.
	 * We also check the inline/allocated status, but this is just implementation detail.
	 */
	using namespace SupportsMove;

	duck::SmallUniquePtr<Base, sizeof (SmallDerived)> small{duck::in_place_type_t<SmallDerived>{}, 1};
	CHECK (small);
	CHECK (small->f ());

	// Move construction (should move in place)
	duck::SmallUniquePtr<Base, sizeof (SmallDerived)> inplace{std::move (small)};
	CHECK (inplace);
	CHECK (!small);
	CHECK (inplace.is_inline ());
	CHECK (inplace->f () == 1);

	// Move construction that must allocate (buffer too small)
	duck::SmallUniquePtr<Base, sizeof (SmallDerived) / 2> allocated{std::move (inplace)};
	CHECK (allocated);
	CHECK (!inplace);
	CHECK (allocated.is_allocated ());
	CHECK (allocated->f () == 1);
	allocated.reset ();

	// Move assign in place to in place
	inplace.emplace<SmallDerived> (5);
	duck::SmallUniquePtr<Base, sizeof (SmallDerived)> inplace2;
	CHECK (inplace);
	CHECK (!inplace2);
	inplace2 = std::move (inplace);
	CHECK (inplace2);
	CHECK (!inplace);
	CHECK (inplace2.is_inline ());
	CHECK (inplace2->f () == 5);
	inplace2.reset ();

	// Move assign from in place to allocated
	inplace.emplace<SmallDerived> (6);
	CHECK (inplace);
	CHECK (!allocated);
	allocated = std::move (inplace);
	CHECK (!inplace);
	CHECK (allocated);
	CHECK (allocated.is_allocated ());
	CHECK (allocated->f () == 6);

	// Move assign from allocated to allocated
	CHECK (!inplace2);
	inplace2 = std::move (allocated);
	CHECK (inplace2);
	CHECK (!allocated);
	CHECK (inplace2.is_allocated ());
	CHECK (inplace2->f () == 6);

	// Release from allocated
	auto raw_ptr = inplace2.release ();
	CHECK (!inplace2);
	CHECK (raw_ptr != nullptr);
	CHECK (raw_ptr->f () == 6);
	delete raw_ptr;

	// Release from inline
	inplace.emplace<SmallDerived> (7);
	auto raw_ptr2 = inplace.release ();
	CHECK (!inplace);
	CHECK (raw_ptr2 != nullptr);
	CHECK (raw_ptr2->f () == 7);
	delete raw_ptr2;
}

TEST_CASE ("conversion and make_small_unique") {
	using namespace SupportsMove;

	// Generate with Derived type
	auto p = duck::make_small_unique<SmallDerived> (42);
	using Is_MakeSmallUniqueSmallDerived_A_SupOfSmallDerived =
	    std::is_same<decltype (p), duck::SmallUniquePtr<SmallDerived, sizeof (SmallDerived)>>;
	CHECK (Is_MakeSmallUniqueSmallDerived_A_SupOfSmallDerived::value);
	CHECK (p);
	CHECK (p->f () == 42);

	// Convert to base type (should be in place)
	duck::SmallUniquePtr<Base, sizeof (SmallDerived)> p2{std::move (p)};
	CHECK (p2);
	CHECK (!p);
	CHECK (p2.is_inline ());
	CHECK (p2->f () == 42);

	// Create BigDerived, move + cast to p2.
	p2 = duck::make_small_unique<BigDerived> ();
	CHECK (p2);
	CHECK (p2.is_allocated ());
	CHECK (p2->f () == -1);
}
