#pragma once

#include <cstdint>
#include <optional>
#include <variant>

namespace cppsl {

enum glsl : uint64_t {
	tBool,
	tInt,
	tFloat,
	tVec2,
	tVec3,
	tVec4,
	tMat3,
	tMat4
};

enum shader_io_type : uint64_t {
	eInput = 0,
	eOutput = 1,
	eLayout = 1 << 2
};

constexpr shader_io_type operator|(shader_io_type A, shader_io_type B)
{
	return shader_io_type((uint64_t) A |  (uint64_t) B);
}

enum class primitive_operation {
	add, sub, mul, div,
	uneg,
	cmp_gt, cmp_lt
};

enum class iop {
	fetch,
	store
};

struct shader_layout {
	glsl    type;
	int32_t binding;
	iop     fs;
};

// TODO: also fetch instrincs
enum class stash_intrinsic {
	si_gl_Position
};

struct fetch_push_constants {
	glsl    type;
	int32_t location;
};

struct constructor {
	glsl    type;
	int32_t nargs;
};

template <typename T, size_t N>
constructor constructor_for()
{
	return { T::underlying, N };
}

struct static_indexing {
	glsl     original;
	uint32_t index;
	iop      fs;    // either fetch or set
};

struct branch_trigger {
	enum {
		eBegin,
		eCondition,
		eEnd
	} type;

	static constexpr branch_trigger begin() {
		return branch_trigger { eBegin };
	}
	
	static constexpr branch_trigger cond() {
		return branch_trigger { eCondition };
	}
	
	static constexpr  branch_trigger end() {
		return branch_trigger { eEnd };
	}
};

using instruction_base = std::variant <
	primitive_operation,
	shader_layout,
	stash_intrinsic,
	fetch_push_constants,
	constructor,
	static_indexing,
	branch_trigger,
	bool,
	float,
	int32_t
>;

struct instruction : instruction_base {
	using instruction_base::instruction_base;

	template <typename T>
	bool holds() const {
		return std::holds_alternative <T> (*this);
	}

	template <typename T>
	std::optional <T> grab() const {
		if (holds <T> ())
			return std::get <T> (*this);
		
		return std::nullopt;
	}
};

}