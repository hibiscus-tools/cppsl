#include <fmt/core.h>
#include <fmt/printf.h>

#include "cppsl.hpp"

// range: range(A, B, S) or range(A, cond, S)
// repeat(range, ftn)
// branch(cond, ftn1(...), ftn2(...))
// cases(cond1, cond2, ..., ftnIf1(...), ftnIf2(...), ..., ftnElse())
// where(cond, ftn1(...), ftn2(...))

// TODO: tree simplification/optimizer routine

void vertex_shader
(
	const layout_input <vec2, 0> &position,
	const layout_input <vec3, 1> &color,
	intrinsics::vertex &vintr,
	layout_output <vec3, 0> &out_color
)
{
	vintr.gl_Position = vec4(position, 0, 1);
	out_color = color;
}

void fragment_shader(layout_output <vec4, 0> &fragment, layout_output <vec4, 1> &ff)
{
	fragment = vec4(1, 0, 1, 1);
	fragment.x = 0.5;
	ff = vec4();
}

// NOTE: standard function in GLSL can be treated like objects with an overloaded operator()()

int main()
{
	// TODO: check for conflicting bindings
	// TODO: check vertex shader compability with vulkan vertex attributes (pass as an extra)
	auto vsource = translate <Stage::Vertex> (vertex_shader);
	fmt::println("\nvertex source:\n{}", vsource);

	// TODO: a way to check compability of vertex -> fragment stage (pass vertex shader as an extra)
	auto fsource = translate <Stage::Fragment> (fragment_shader);
	fmt::println("\nfragment source:\n{}", fsource);
}
