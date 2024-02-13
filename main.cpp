#include <array>
#include <string>
#include <variant>
#include <vector>
#include <optional>
#include <functional>

#include <fmt/core.h>
#include <fmt/format.h>

namespace cppsl {

// Using runtime generation (JIT)
// TODO: convert to petal trees for optimization

enum glsl : uint64_t {
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
	add, sub, mul, div
};

struct fetch_layout {
	glsl    type;
	int32_t binding;
};

struct fetch_push_constants {
	glsl    type;
	int32_t location;
};

struct stash_layout {
	glsl    type;
	int32_t binding;
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

using instruction_base = std::variant <
	primitive_operation,
	fetch_layout,
	fetch_push_constants,
	stash_layout,
	constructor,
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

using stream = std::vector <instruction>;

stream concat(const stream &A, const stream &B)
{
	stream C;
	C.insert(C.begin(), A.begin(), A.end());
	C.insert(C.end(), B.begin(), B.end());
	return C;
}

stream concat(const stream &A, const instruction &I)
{
	stream C;
	C.insert(C.begin(), A.begin(), A.end());
	C.push_back(I);
	return C;
}

stream concat(const instruction &I, const stream &A)
{
	stream C;
	C.push_back(I);
	C.insert(C.end(), A.begin(), A.end());
	return C;
}

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

// Vector types
struct vec2 : stream {
	using stream::stream;
	static constexpr glsl underlying = glsl::tVec2;
	vec2(const stream &s) : stream(s) {}
};

struct vec3 : stream {
	using stream::stream;
	static constexpr glsl underlying = glsl::tVec3;
	vec3(const stream &s) : stream(s) {}
};

struct vec4 : stream {
	using stream::stream;

	static constexpr glsl underlying = glsl::tVec4;

	vec4(const stream &s) : stream { s } {}
	vec4(const vec3 &v, float x) : stream {
		concat_args(v, x, constructor_for <vec4, 2> ())
	} {}
};

// Matrix types
struct mat3 : stream {
	using stream::stream;
	static constexpr glsl underlying = glsl::tMat3;
	mat3(const stream &s) : stream(s) {}
};

struct mat4 : stream {
	using stream::stream;
	static constexpr glsl underlying = glsl::tMat4;
	mat4(const stream &s) : stream(s) {}
};

template <typename T, size_t binding>
struct in_layout {
	static constexpr shader_io_type io_type = eInput | eLayout;

	operator T() const {
		return stream { fetch_layout { T::underlying, binding } };
	}

	static constexpr T fetch() {
		return stream { fetch_layout { T::underlying, binding } };
	}
};

template <typename T, size_t binding>
struct out_layout : stream {
	using stream::stream;
	
	static constexpr shader_io_type io_type = eOutput | eLayout;

	out_layout &operator=(const T &value) {
		*this = concat(value, stash_layout { T::underlying, binding });
		return *this;
	}
};

vec2 operator+(const vec2 &A, const vec2 &B)
{
	return concat_args(A, B, primitive_operation::add);
}

vec4 operator*(const mat4 &M, const vec4 &A)
{
	return concat_args(M, A, primitive_operation::mul);
}

// Vertex shader outputs
struct vertex_shader_intrinsics {
	vec4 gl_Position;
};

// TODO: fragment shader outputs
template <typename T>
struct framebuffer_output {

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
concept process_io = requires {
	{ std::decay_t <T> ::io_type } -> std::same_as <const shader_io_type &>;
} || std::is_base_of_v <push_constant, T>;

template <process_io ... Args>
struct shader_args {
	using value_type = std::tuple <std::decay_t <Args>...>;
	static value_type value;
};

template <process_io ... Args>
shader_args <Args...>::value_type
shader_args <Args...> ::value {};

// Gather single shader elements/outputs
// TODO: concept for typed...
template <process_io T>
stream unit_gather(const T &value)
{
	// TODO: note that storage buffers are both output and input
	if constexpr (T::io_type & eOutput == eOutput) {
		return (stream) value;
	}

	return {};
}

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
		stream A = unit_gather <std::tuple_element_t <I, T>> (std::get <I> (args));
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
	auto args = shader_args <Args...> ::value;
	std::apply(ftn, args);
	stream S = gather <0, Args...> (args);
	fmt::print("  > stream: {}\n", S);
	// TODO: warn on empty shaders
}

// Formatting
auto format_as(const glsl t)
{
	switch (t) {
	case tFloat:
		return "float";
	case tVec2:
		return "vec2";
	case tVec3:
		return "vec3";
	case tVec4:
		return "vec4";
	case tMat3:
		return "mat3";
	case tMat4:
		return "mat4";
	}

	return "?";
}

auto format_as(const primitive_operation &primop)
{
	switch (primop) {
	case primitive_operation::add:
		return "add";
	case primitive_operation::mul:
		return "mul";
	}

	return "?";
}

std::string format_as(const instruction &I)
{
	if (auto value = I.grab <primitive_operation> ()) {
		return format_as(*value);
	}

	if (auto value = I.grab <fetch_layout> ()) {
		return fmt::format("fetch_layout <{}, {}>", value->type, value->binding);
	}
	
	if (auto value = I.grab <fetch_push_constants> ()) {
		return fmt::format("fetch_push_constants <{}, {}>", value->type, value->location);
	}
	
	if (auto value = I.grab <constructor> ()) {
		return fmt::format("constructor <{}, {}>", value->type, value->nargs);
	}
	
	if (auto value = I.grab <float> ()) {
		return fmt::to_string(*value);
	}

	return "<?>";
}

std::string format_as(const stream &s)
{
	std::string str = "";
	for (uint32_t i = 0; i < s.size(); i++) {
		str += fmt::format("{}", s[i]);
		if (i + 1 < s.size())
			str += ", ";
	}

	return "{" + str + "}";
}

template <typename T>
requires streamable <T>
auto format_as(const T &t)
{
	return format_as(stream(t));
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

void shader(const in_layout <vec3, 0> &I0, out_layout <vec3, 0> &O0)
{
	fmt::print("in 0: {}\n", I0);
	fmt::print("out 0: {}\n", O0);
	// fmt::print("push constants: {}, {}, {}\n", pc.model, pc.view, pc.proj);
}

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

	fmt::print("projeceted: {}\n", projected);
	
	M.gl_Position = projected;
}

void fragment_shader(const in_layout <vec2, 0> &p, const in_layout <vec2, 1> &q)
{
	vec2 r = p + q;
	fmt::print("r: {}\n", r);
}

}

int main()
{
	using namespace cppsl;
	
	in_layout <vec2, 0> p;
	in_layout <vec2, 1> q;

	shaders::fragment_shader(p, q);
	
	in_layout <vec3, 0> i0;
	in_layout <vec3, 1> i1;

	shaders::mvp pc;

	vertex_shader_intrinsics M;

	out_layout <vec2, 0> o0;

	shaders::vertex_shader(i0, i1, pc, M, o0);

	compile(shaders::shader);
}