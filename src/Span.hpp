#pragma once

#include <cstddef>
#include <cassert>
#include <functional>

inline constexpr std::size_t dynamic_extent = SIZE_MAX;

template <typename ElementType, std::size_t Extent = dynamic_extent> class Span;

template <typename ElementType, std::size_t Extent> class Span
{
	template <typename T>
	using uncvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

	template <typename> struct is_span : std::false_type
	{
	};

	template <typename T, std::size_t S> struct is_span<Span<T, S>> : std::true_type
	{
	};

	template <typename> struct is_std_array : std::false_type
	{
	};

	template <typename T, std::size_t N> struct is_std_array<std::array<T, N>> : std::true_type
	{
	};

    // Default value if size() and data() methods are not available.
	template <typename, typename = void> struct has_size_and_data : std::false_type
	{
	};

	// Check if U type has size() and data() methods.
	template <typename U>
	struct has_size_and_data<U,
							 std::void_t<decltype(std::size(std::declval<U>())),
										 decltype(std::data(std::declval<U>()))>> : std::true_type
	{
	};

	template <typename C, typename U = uncvref_t<C>> struct is_container
	{
		static constexpr bool value = !is_span<U>::value && !is_std_array<U>::value &&
				!std::is_array<U>::value && has_size_and_data<C>::value;
	};

	template <typename T> using remove_pointer_t = typename std::remove_pointer<T>::type;

	template <typename, typename, typename = void>
	struct is_container_element_type_compatible : std::false_type
	{
	};

	template <typename T, typename E>
	struct is_container_element_type_compatible<
			T, E,
			typename std::enable_if<!std::is_same<typename std::remove_cv<decltype(std::data(
														  std::declval<T>()))>::type,
												  void>::value &&
									std::is_convertible<remove_pointer_t<decltype(std::data(
																std::declval<T>()))> (*)[],
														E (*)[]>::value>::type> : std::true_type
	{
	};

	template <typename E, std::size_t S> struct span_storage
	{
		constexpr span_storage() noexcept = default;

		constexpr span_storage(E* p_ptr, std::size_t /*unused*/) noexcept : ptr(p_ptr) {}

		E* ptr = nullptr;
		static constexpr std::size_t size = S;
	};

	using storage_type = span_storage<ElementType, Extent>;

public:
	using element_type = ElementType;
	using value_type = typename std::remove_cv<ElementType>::type;
	using pointer = element_type*;
    using const_pointer = const element_type*;

	// Constructors

	// pointer and size
	Span(ElementType* ptr, std::size_t count) : data_(ptr), size_(count), use_indexing_(false) {}

	// std::array
	template <typename T, std::size_t N, std::size_t E = Extent,
			  typename std::enable_if<(E == dynamic_extent || N == E) &&
											  is_container_element_type_compatible<
													  const std::array<T, N>&, ElementType>::value,
									  int>::type = 0>
	Span(const std::array<T, N>& arr) noexcept : data_(arr.data()), size_(N), use_indexing_(false)
	{
	}

	// container with data and size
	template <typename Container, std::size_t E = Extent,
			  typename std::enable_if<
					  E == dynamic_extent && is_container<Container>::value &&
							  is_container_element_type_compatible<Container&, ElementType>::value,
					  int>::type = 0>
	Span(Container& container) :
			data_(container.data()), size_(container.size()), use_indexing_(false)
	{
	}

	// Constructor for containers with operator[]
	// template <typename Container, typename std::enable_if<!has_size_and_data<Container>::value>>
	// Span(Container& container) :
	// 		container_((void*)&container), size_(container.size()), use_indexing_(true)
	// {
	// 	access_fn_ = [&container](std::size_t index) -> const element_type&
	// 	{ return container[index]; };
	// }

	// Accessor functions
	element_type& operator[](std::size_t index)
	{
		assert(index < size_);
		if (use_indexing_)
		{
			return ((element_type*)container_)[index]; // Use operator[] for container
		}
		else
		{
			return data_[index]; // Use pointer for raw array
		}
	}

	const element_type& operator[](std::size_t index) const
	{
		assert(index < size_);
		if (use_indexing_)
		{
			return ((element_type*)container_)[index]; // Use operator[] for container
		}
		else
		{
			return data_[index]; // Use pointer for raw array
		}
	}

	// Returns size of the span
	std::size_t size() const { return size_; }

	// Returns pointer to the underlying data
	element_type* data() { return data_; }
	const element_type* data() const { return data_; }

	// Iterator support (index-based)
	class Iterator
	{
	public:
		Iterator(Span* span, std::size_t index) : span_(span), index_(index) {}

		bool operator!=(const Iterator& other) const { return index_ != other.index_; }
		Iterator& operator++()
		{
			++index_;
			return *this;
		}
		element_type& operator*() { return (*span_)[index_]; }

	private:
		Span* span_;
		std::size_t index_;
	};

	Iterator begin() { return Iterator(this, 0); }
	Iterator end() { return Iterator(this, size_); }

private:
	std::function<const element_type&(std::size_t)> access_fn_;
	storage_type storage_{};
	element_type* data_ = nullptr; // Pointer to the first element (used for raw arrays or vectors)
	void* container_ = nullptr; // Pointer to a container that has operator[] (e.g., vector)
	std::size_t size_ = 0; // Number of elements
	bool use_indexing_ = false; // Flag to switch between raw array or container mode
};