#pragma once

#include "Span.hpp"

#include <optional>
#include <vector>

template <typename T, size_t SIZE> class CircularBuffer
{
public:
	CircularBuffer() : buffer(SIZE), write_pos(0), read_pos(0), full(false) {}

	void clear() 
	{
		write_pos = read_pos = 0;
		full = false;
	}

	void add(const T& item)
	{
		buffer[write_pos] = item;
		write_pos = (write_pos + 1) % buffer.size();

		if (full)
		{
			read_pos = (read_pos + 1) % buffer.size(); // Move tail to overwrite oldest data
		}

		full = write_pos == read_pos;
	}

	void add(Span<T> items)
	{
		for (auto& i : items)
		{
            add(i);
		}
	}

	void add(Span<const T> items)
	{
		for (auto& i : items)
		{
            add(i);
		}
	}

	T& push_back()
	{
		T& item = buffer[write_pos];
		write_pos = (write_pos + 1) % buffer.size();

		if (full)
		{
			read_pos = (read_pos + 1) % buffer.size(); // Move tail to overwrite oldest data
		}

		full = write_pos == read_pos;

		return item;
	}

	std::optional<T> get()
	{
		if (isEmpty())
		{
			return std::nullopt;
		}

		T item = buffer[read_pos];
		full = false;
		read_pos = (read_pos + 1) % buffer.size();
		return item;
	}

	std::vector<T> getSome(size_t count = 1)
	{
		if (size() < count)
			return {};

		std::vector<T> items;
		items.reserve(count);

		for (int i = 0; i < count && !isEmpty(); ++i)
		{
			T item = get().value();
			items.push_back(item);
		}

		return std::move(items);
	}

	bool isFull() const { return full; }

	bool isEmpty() const { return (!full && (write_pos == read_pos)); }

	size_t capacity() const { return buffer.size(); }

	size_t size() const
	{
		size_t size = buffer.size();

		if (!full)
		{
			if (write_pos >= read_pos)
			{
				size = write_pos - read_pos;
			}
			else
			{
				size = buffer.size() + write_pos - read_pos;
			}
		}

		return size;
	}

private:
	std::vector<T> buffer;
	size_t write_pos;
	size_t read_pos;
	bool full;
};
