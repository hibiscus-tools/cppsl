#include <array>
#include <string>
#include <variant>
#include <vector>
#include <optional>
#include <functional>
#include <unordered_set>

#include <fmt/core.h>

#include "instruction.hpp"
#include "stream.hpp"
#include "format.hpp"

namespace cppsl {

// Static variables
tracking_system tracking;

// Establishing vector types via inheritance
struct scalar_base {};
struct vector_base {};
struct matrix_base {};

template <typename T>
concept scalar_type = std::is_base_of_v <scalar_base, T> || std::is_arithmetic_v <T>;

template <typename T>
concept vector_type = std::is_base_of_v <vector_base, T>;

template <typename T>
concept matrix_type = std::is_base_of_v <matrix_base, T>;

// Indexer types
// TODO: single for now
template <typename Owner, streamable T, uint32_t index>
// TODO: defer checking that owner is streamable? or simply ignore?
struct index_ref {
	Owner &owner;

	index_ref(Owner &o) : owner(o) {}
	
	// Assignment triggers a callback to the original source (owner)
	index_ref &operator=(const T &value) {
		owner.inject(concat(value, static_indexing { T::underlying, index, iop::store }));
		return *this;
	}

	// Constant value retrieval
	operator T() const {
		return concat(owner, static_indexing { T::underlying, index, iop::fetch });
	}
};

// Unit types
struct injection_promise {
	~injection_promise() {
		stream S = { branch_trigger::end() };
		tracking.inject(S, nullptr);

		// Removing degenerate ones
		for (stream *Sp : tracking.active) {
			stream &S = *Sp;
			// assert(S->size() >= 2);
			size_t len = S.size();
			if (auto value = S[len - 2].grab <branch_trigger> ()) {
				if (value->type == branch_trigger::eCondition) {
					auto start = std::find_if(S.rbegin(), S.rend(),
						[](const instruction &I) {
							if (auto value = I.grab <branch_trigger> ())
								return value->type == branch_trigger::eBegin;
							
							return false;
						}
					);

					auto last = S.begin() + std::distance(S.begin(), start.base()) - 1;
					S.erase(last, S.end());
				}
			}
		}
	}

	operator bool() const {
		return true;
	}
};

struct boolean : stream {
	static constexpr glsl underlying = glsl::tBool;

	enum {
		eIf, eElseIf, eElse
	} type = eIf;
	
	boolean(auto t) : type(t) {}

	boolean &as(auto t) {
		type = t;
		return *this;
	}

	// From an existing stream
	boolean(const stream &s = {}) : stream(s) {}

	// GLSL constructors
	boolean(bool x) : boolean {
		concat_args(instruction(x), constructor_for <boolean, 1> ())
	} {}

	// Injects the evaluation into exists thread variables
	injection_promise inject() const {
		// Create a tagged expression that indicates which expression
		// was checked for the branch
		stream S = *this;
		S.insert(S.begin(), branch_trigger::begin());
		S.push_back(branch_trigger::cond());
		fmt::print("branch injecting S = {}\n", S);
		tracking.inject(S, this);
		return {};
	}
};

struct f32 : stream, vector_base, scalar_base {
	static constexpr glsl underlying = glsl::tFloat;

	f32(const stream &s = {}) : stream(s) {}

	// GLSL constructors
	f32(float x) : f32 {
		concat_args(instruction(x), constructor_for <f32, 1> ())
	} {}
};

// Vector types
struct vec2 : stream, vector_base {
	static constexpr glsl underlying = glsl::tVec2;

	vec2(const stream &s = {}) : stream(s) {}
};

struct vec3 : stream, vector_base {
	static constexpr glsl underlying = glsl::tVec3;

	vec3(const stream &s = {}) : stream(s) {}
};

struct vec4 : stream, vector_base {
	static constexpr glsl underlying = glsl::tVec4;

	index_ref <vec4, f32, 0> x;
	index_ref <vec4, f32, 1> y;
	index_ref <vec4, f32, 2> z;
	index_ref <vec4, f32, 3> w;

	// Adapting from existing streams
	vec4(const stream &s = {}) : stream(s),
			x(*this), y(*this),
			z(*this), w(*this) {}

	// GLSL constructors
	vec4(const vec3 &v, float x) : vec4 {
		concat_args(v, x, constructor_for <vec4, 2> ())
	} {}

	vec4(float x, float y, float z, float w) : vec4 {
		concat_args(x, y, z, w, constructor_for <vec4, 4> ())
	} {}

	// Callback
	void inject(const stream &S) {
		insert(end(), S.begin(), S.end());
	}
};

// Matrix types
struct mat3 : stream, matrix_base {
	using stream::stream;
	static constexpr glsl underlying = glsl::tMat3;
	mat3(const stream &s) : stream(s) {}
};

struct mat4 : stream, matrix_base {
	using stream::stream;
	static constexpr glsl underlying = glsl::tMat4;
	mat4(const stream &s) : stream(s) {}
};

template <typename T, size_t binding>
struct in_layout {
	static constexpr shader_io_type io_type = eInput | eLayout;

	operator T() const {
		return stream { shader_layout { T::underlying, binding, iop::fetch } };
	}

	static constexpr T fetch() {
		return stream { shader_layout { T::underlying, binding, iop::fetch } };
	}
};

template <vector_type T, size_t binding>
struct out_layout : T {
	using T::T;
	
	static constexpr shader_io_type io_type = eOutput | eLayout;

	out_layout &operator=(const T &value) {
		this->clear();
		stream S = concat(value, shader_layout { T::underlying, binding, iop::store });
		this->insert(this->begin(), S.begin(), S.end());
		return *this;
	}
};

// Arithmetic operators
f32 operator+(const f32 &A, const f32 &B)
{
	return concat_args(A, B, primitive_operation::add);
}

vec2 operator+(const vec2 &A, const vec2 &B)
{
	return concat_args(A, B, primitive_operation::add);
}

f32 operator-(const f32 &A, const f32 &B)
{
	return concat_args(A, B, primitive_operation::sub);
}

f32 operator/(const f32 &A, const f32 &B)
{
	return concat_args(A, B, primitive_operation::div);
}

boolean operator>(const f32 &A, const f32 &B)
{
	return concat_args(A, B, primitive_operation::cmp_gt);
}

f32 operator-(const f32 &A)
{
	return concat_args(A, primitive_operation::uneg);
}

// Multiplication
template <typename A>
requires vector_type <A> || matrix_type <A>
A operator*(const A &x, const A &y)
{
	return concat_args(x, y, primitive_operation::mul);
}

// Heterogenous
template <matrix_type A, vector_type B>
B operator*(const A &x, const B &y)
{
	return concat_args(x, y, primitive_operation::mul);
}

template <vector_type A, scalar_type B>
A operator*(const A &x, const B &y)
{
	return concat_args(x, y, primitive_operation::mul);
}

template <vector_type A, scalar_type B>
A operator*(const B &x, const A &y)
{
	return concat_args(x, y, primitive_operation::mul);
}

// Vertex shader outputs
struct vertex_shader_intrinsics {
	static constexpr shader_io_type io_type = eOutput;

	// vec4 gl_Position;
	struct __si_gl_Position : vec4 {
		__si_gl_Position &operator=(const vec4 &value) {
			clear();
			stream S = concat(value, stash_intrinsic::si_gl_Position);
			insert(begin(), S.begin(), S.end());
			return *this;
		}
	} gl_Position;

	// TODO: more outputs for extensions?

	operator stream() const {
		stream S = gl_Position;
		return S;
	}
};

// Push constants
struct push_constant {
	// TODO: requires
	template <size_t I = 0, typename T, typename ... Args>
	void register_data(T &m, Args &... args) {
		m = stream { fetch_push_constants { T::underlying, I } };
		if constexpr (sizeof...(Args))
			register_data <I + 1> (args...);
	}
};

// TODO: this means we need wrappers over c++ primitives (f32, i32) etc

// Automatically generating arguments for the shaders
template <typename T>
concept raw_process_io = requires {
	{ std::decay_t <T> ::io_type } -> std::same_as <const shader_io_type &>;
};

template <typename T>
concept process_io = requires {
	{ std::decay_t <T> ::io_type } -> std::same_as <const shader_io_type &>;
} || std::is_base_of_v <push_constant, std::decay_t <T>>;

template <process_io ... Args>
struct shader_args {
	using value_type = std::tuple <std::decay_t <Args>...>;
	static value_type value;
};

template <process_io ... Args>
shader_args <Args...>::value_type
shader_args <Args...> ::value {};

// Gather single shader elements/outputs
template <typename T>
struct unit_gather {};

template <raw_process_io T>
struct unit_gather <T> {
	static stream go(const T &value) {
		if constexpr ((T::io_type & eOutput) == eOutput)
			return (stream) value;

		return {};
	}
};

template <typename T>
requires std::is_base_of_v <push_constant, std::decay_t <T>>
struct unit_gather <T> {
	static stream go(const T &value) {
		return {};
	}
};

// Gathering shader code from output types
template <size_t I, process_io ... Args>
stream gather(typename shader_args <Args...> ::value_type args)
{
	using T = typename shader_args <Args...> ::value_type;

	stream G;
	if constexpr (I >= sizeof...(Args)) {
		// End of processing
		return G;
	} else {
		stream A = unit_gather <std::tuple_element_t <I, T>> ::go(std::get <I> (args));
		stream B = gather <I + 1, Args...> (args);
		return concat(A, B);
	}

	// TODO: warn on empty shaders
}

// Compiling shaders
// TODO: lambda/std::function support
template <process_io ... Args>
void compile(void (*ftn)(Args... ))
{
	fmt::print("\ncompiling shader\n");
	auto args = shader_args <Args...> ::value;
	std::apply(ftn, args);
	stream S = gather <0, Args...> (args);
	fmt::print("  > stream: {}\n", S);
	// TODO: warn on empty shaders
}

}

namespace shaders {

using namespace cppsl;

struct mvp : push_constant {
	mat4 model;
	mat4 view;
	mat4 proj;

	mvp() {
		register_data(model, view, proj);	
	}
};

void vertex_shader
(
	// Shader inputs
	const in_layout <vec3, 0> &position,
	const in_layout <vec3, 1> &normal,

	const mvp &pc,

	// Shader outputs
	vertex_shader_intrinsics &M,

	out_layout <vec2, 0> &out_normal
)
{
	// TODO: allow implicit conversion such as the following
	// out_normals = normals;	

	vec4 appended = vec4(position, 1);
	vec4 projected = pc.proj * pc.view * pc.model * vec4(position, 1);
	projected.y = -projected.y;
	projected.z = (projected.z + projected.w) / 2.0f;

	M.gl_Position = projected;
}

#define sl_if(X) if ((X).inject())
#define sl_elif(X) if ((X).as(boolean::eElseIf).inject())
#define sl_else(X) if (boolean(boolean::eElse).inject())

void fragment_shader(out_layout <vec4, 0> &fragment)
{
	f32 x = 1.0f;
	fmt::print("fragment after: {}\n", fragment);

	fragment = vec4 { 1, 0, 0, 0 };
	sl_if(x > 0.5f) {
	}
	
	sl_elif(x > 0.5f) {
		
	}
	
	sl_else(x > 0.5f) {
		// fragment.y = 4.0f * x;
	}

	fmt::print("end fragment {}\n", fragment);
}

}

int main()
{
	using namespace cppsl;
	
	compile(shaders::fragment_shader);
	// compile(shaders::vertex_shader);
}