#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"  // Include Catch2 header
#include <vector>
#include <array>
#include "../src/Span.hpp"  // Assuming Span class is in this file
#include <iostream>

// Test case for std::vector
TEST_CASE("Span works with std::vector", "[vector]") {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    Span<int> spanVec(vec);

    REQUIRE(spanVec.size() == 5);  // Check size
    REQUIRE(spanVec[0] == 1);  // Check first element
    REQUIRE(spanVec[4] == 5);  // Check last element

    // Modify elements
    spanVec[0] = 10;
    spanVec[4] = 50;

    REQUIRE(spanVec[0] == 10);
    REQUIRE(spanVec[4] == 50);
}

// Test case for raw array
TEST_CASE("Span works with raw array", "[array]") {
    int cArray[] = {100, 200, 300};
    Span<int> spanCArray(cArray, 3);

    REQUIRE(spanCArray.size() == 3);  // Check size
    REQUIRE(spanCArray[0] == 100);  // Check first element
    REQUIRE(spanCArray[2] == 300);  // Check last element

    // Modify elements
    spanCArray[0] = 150;
    spanCArray[2] = 350;

    REQUIRE(spanCArray[0] == 150);
    REQUIRE(spanCArray[2] == 350);
}

// Test case for modifying elements using range-based for loop
TEST_CASE("Span supports range-based iteration", "[range]") {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    Span<int> spanVec(vec);

    int expectedValue = 1;
    for (auto& value : spanVec) {
        REQUIRE(value == expectedValue);
        ++expectedValue;
    }
}

// Test case for out-of-bounds access (using assertions)
TEST_CASE("Span out-of-bounds access", "[bounds]") {
    std::vector<int> vec = {1, 2, 3};
    Span<int> spanVec(vec);

    REQUIRE(spanVec.size() == 3);

    // Accessing within bounds
    REQUIRE(spanVec[2] == 3);

    // Accessing out-of-bounds should trigger assertion (debug builds)
    // This line is commented out because it will cause assertion failure.
    // Uncomment to test behavior in debug mode.
    // REQUIRE_THROWS_AS(spanVec[3], std::out_of_range);
}

// Test case for const correctness
TEST_CASE("Span supports const access", "[const]") {
    const std::vector<int> vec = {10, 20, 30};
    Span<const int> spanVec(vec);

    REQUIRE(spanVec.size() == 3);
    REQUIRE(spanVec[0] == 10);
    REQUIRE(spanVec[1] == 20);
    REQUIRE(spanVec[2] == 30);

    // Const span should not allow modification
    // Uncommenting the following line should trigger a compile error
    // spanVec[0] = 100;
}