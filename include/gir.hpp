#pragma once

#include <variant>
#include <vector>

// GLSL Operations/Annotations
enum gloa : int {
	eConstruct, eIndex, eComponent,

	eInt32, eFloat32, eVec2, eVec3, eVec4,

	eLayoutInput,
	eLayoutOutput,

	eAdd, eSub, eMul, eDiv
};

static constexpr const char *GLOA_STRINGS[] {
	"Construct", "Index", "Component",

	"Int32", "Float32", "Vec2", "Vec3", "Vec4",

	"LayoutInput",
	"LayoutOutput",

	"Add", "Sub", "Mul", "Div"
};

// GLSL Intermediate Representation (atom)
using gir_t = std::variant <int, float, gloa>;

// GLSL Intermediate Representation (tree)
struct gir_tree {
	// Packet of data
	gir_t data;

	// Indicates whether the expression is constant
	bool cexpr;

	// Dependencies of the expression (i.e. expression tree)
	std::vector <gir_tree> children;

	// Replace contents
	void rehash(gir_t, bool, const std::vector <gir_tree> &);

	static gir_tree from(gir_t, bool);
	static gir_tree from(gir_t, bool, const std::vector <gir_tree> &);

	static gir_tree cfrom(gir_t);
	static gir_tree cfrom(gir_t, const std::vector <gir_tree> &);

	static gir_tree vfrom(gir_t);
	static gir_tree vfrom(gir_t, const std::vector <gir_tree> &);
};

// Performing constant expression simplifications
gir_tree ceval(const gir_tree &);
