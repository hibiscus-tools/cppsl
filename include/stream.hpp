#pragma once

#include <vector>
#include <unordered_set>

#include "instruction.hpp"

namespace cppsl {

// Forward declarations
struct stream;

// Defining the tracking system for streams
struct tracking_system {
	std::unordered_set <stream *> active;
	
	void put(stream *);
	void pop(stream *);
	void inject(const stream &, const stream *const);
} extern tracking;

// Defining streams
using stream_base = std::vector <instruction>;

struct stream : stream_base {
	// Semantic operations	
	stream(const std::initializer_list <instruction> &items = {})
			: stream_base(items) {
		tracking.put(this);
	}

	stream(const stream &other)
			: stream_base(other) {
		tracking.put(this);
	}

	stream &operator=(const stream &other) {
		if (this != &other) {
			tracking.put(this);
			for (const auto &instr : other)
				push_back(instr);
		}

		return *this;
	}

	~stream() {
		tracking.pop(this);
	}
};

stream concat(const stream &, const stream &);
stream concat(const stream &, const instruction &);
stream concat(const instruction &, const stream &);

template <typename T>
concept streamable = std::is_base_of_v <stream, T> || std::is_convertible_v <T, instruction>;

template <streamable T, streamable ... Args>
stream concat_args(const T &streamer, const Args &... args)
{
	if constexpr (sizeof...(Args)) {
		auto half = concat_args(args...);
		return concat(streamer, half);
	} else {
		if constexpr (std::is_base_of_v <stream, T>)
			return streamer;
		else
			return { streamer };
	}
}

}