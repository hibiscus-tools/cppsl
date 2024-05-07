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

void shader
(
	const layout_input <vec3, 0> &in_color,
	layout_output <vec4, 0> &fragment
)
{
	fragment = vec4(in_color, 1);
}

// NOTE: standard function in GLSL can be treated like objects with an overloaded operator()()

int main()
{
	// TODO: check for conflicting bindings
	// TODO: check vertex shader compability with vulkan vertex attributes (pass as an extra)
	// TODO: a way to check compability of vertex -> fragment stage (pass vertex shader as an extra)

	auto source = translate <Stage::Fragment> (shader);
	fmt::println("\ntranslated source:\n{}", source);
}
