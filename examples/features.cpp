#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/printf.h>

#include "cppsl.hpp"

// range: range(A, B, S) or range(A, cond, S)
// repeat(range, ftn)
// branch(cond, ftn1(...), ftn2(...))
// cases(cond1, cond2, ..., ftnIf1(...), ftnIf2(...), ..., ftnElse())
// where(cond, ftn1(...), ftn2(...))
// tensor[...] to access ranges

// TODO: tree simplification/optimizer routine
struct mvp {
	mat4 model;
	mat4 view;
	mat4 proj;

	mvp() {
		push_constants_members(model, view, proj);
	}

	// TODO: free generation of concrete structure? or sizeof structure instead...
};

void vertex_shader
(
	const layout_input <vec3, 0> &position,
	intrinsics::vertex &vintr
)
{
	vec4 v = vec4(position, 1.0f);
	v.x = 2.0f;
	vintr.gl_Position = v;
}

// NOTE: standard function in GLSL can be treated like objects with an overloaded operator()()

int main()
{
	// TODO: check for conflicting bindings
	// TODO: check vertex shader compability with vulkan vertex attributes (pass as an extra)

	// TODO: a way to check compability of vertex -> fragment stage (pass vertex shader as an extra)
	// auto fsource = translate <Stage::Fragment> (fragment_shader);
	// fmt::println("\nfragment source:\n{}", fsource);

	auto vsource = translate <Stage::Vertex> (vertex_shader);
	fmt::println("\nvertex source:\n{}", vsource);
}
