#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"  // Include Catch2 header
#include "../src/Span.hpp"  // Assuming Span class is defined in Span.hpp
#include "../src/CircularBuffer.hpp"  // Assuming CircularBuffer class is defined here

#include <vector>

// Test case for adding and retrieving elements from the buffer
TEST_CASE("CircularBuffer basic functionality", "[CircularBuffer]") {
    CircularBuffer<int, 3> buffer;

    REQUIRE(buffer.isEmpty() == true);
    REQUIRE(buffer.isFull() == false);
    REQUIRE(buffer.size() == 0);

    buffer.add(1);
    buffer.add(2);
    buffer.add(3);

    REQUIRE(buffer.isFull() == true);
    REQUIRE(buffer.size() == 3);

    // Retrieve elements
    auto item1 = buffer.get();
    REQUIRE(item1.has_value() == true);
    REQUIRE(item1.value() == 1);

    auto item2 = buffer.get();
    REQUIRE(item2.has_value() == true);
    REQUIRE(item2.value() == 2);

    auto item3 = buffer.get();
    REQUIRE(item3.has_value() == true);
    REQUIRE(item3.value() == 3);

    // Buffer should be empty now
    REQUIRE(buffer.isEmpty() == true);
    REQUIRE(buffer.get().has_value() == false);
}

// Test case for overwriting elements when the buffer is full
TEST_CASE("CircularBuffer overwrites oldest elements when full", "[CircularBuffer]") {
    CircularBuffer<int, 3> buffer;

    buffer.add(1);
    buffer.add(2);
    buffer.add(3);

    // Buffer is full now, adding a new element should overwrite the oldest (1)
    buffer.add(4);

    // The buffer should now contain 2, 3, and 4
    auto item = buffer.get();
    REQUIRE(item.has_value() == true);
    REQUIRE(item.value() == 2);

    item = buffer.get();
    REQUIRE(item.has_value() == true);
    REQUIRE(item.value() == 3);

    item = buffer.get();
    REQUIRE(item.has_value() == true);
    REQUIRE(item.value() == 4);
}

// Test case for getAll
TEST_CASE("CircularBuffer getAll method", "[CircularBuffer]") {
    CircularBuffer<int, 5> buffer;

    buffer.add(1);
    buffer.add(2);
    buffer.add(3);

    auto items = buffer.getSome(3);
    REQUIRE(items.size() == 3);
    REQUIRE(items[0] == 1);
    REQUIRE(items[1] == 2);
    REQUIRE(items[2] == 3);

    // Buffer should be empty now
    REQUIRE(buffer.isEmpty() == true);
    REQUIRE(buffer.size() == 0);
}

// Test case for using push_back to insert elements
TEST_CASE("CircularBuffer push_back method", "[CircularBuffer]") {
    CircularBuffer<int, 3> buffer;

    // Using push_back to insert elements
    int& item = buffer.push_back();
    item = 10;

    buffer.push_back() = 20;
    buffer.push_back() = 30;

    // Check if the elements are added correctly
    REQUIRE(buffer.isFull() == true);
    REQUIRE(buffer.size() == 3);

    auto items = buffer.getSome(3);
    REQUIRE(items.size() == 3);
    REQUIRE(items[0] == 10);
    REQUIRE(items[1] == 20);
    REQUIRE(items[2] == 30);
}

// Test case for Span support in add function
TEST_CASE("CircularBuffer add method with Span", "[CircularBuffer]") {
    CircularBuffer<int, 5> buffer;

    std::vector<int> data = {11, 12, 13, 14};
    Span<int> dataSpan(data);
    REQUIRE(dataSpan.size() == 4);

    buffer.add(dataSpan);
    REQUIRE(buffer.size() == 4);

    // Test buffer contents
    auto items = buffer.getSome(4);
    REQUIRE(items.size() == 4);
    REQUIRE(items[0] == 11);
    REQUIRE(items[1] == 12);
    REQUIRE(items[2] == 13);
    REQUIRE(items[3] == 14);
}