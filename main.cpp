#include <fmt/printf.h>

#include "cppsl.hpp"

// range: range(A, B, S) or range(A, cond, S)
// repeat(range, ftn)
// branch(cond, ftn1(...), ftn2(...))
// cases(cond1, cond2, ..., ftnIf1(...), ftnIf2(...), ..., ftnElse())
// where(cond, ftn1(...), ftn2(...))

// TODO: tree simplification/optimizer routine

void vertex_shader(const layout_input <vec2, 0> &position, intrinsics::vertex &vintr)
{
	vintr.gl_Position = vec4(position, 0, 1);
}

void fragment_shader(layout_output <vec4, 0> &fragment)
{
	fragment = vec4(1, 0, 1, 1);
	fragment.x = 0.5;
}

// NOTE: standard function in GLSL can be treated like objects with an overloaded operator()()

int main()
{
	layout_input <vec2, 0> position;
	intrinsics::vertex vintr;

	vertex_shader(position, vintr);

	fmt::println("vintr: gl_Position = {}", vintr.gl_Position);

	layout_output <vec4, 0> fragment;

	fragment_shader(fragment);

	fmt::println("fragment: {}", fragment);

	// int generator = 0;
	// auto source = translate(vintr.gl_Position, generator);

	auto vsource = translate_vertex_shader(vintr);
	fmt::println("vertex source:\n{}", vsource);

	auto fsource = translate_fragment_shader(fragment);
	fmt::println("fragment source:\n{}", fsource);
}
