#pragma once

#include <variant>
#include <vector>

// GLSL Operations/Annotations
enum gloa : int {
	// Read and write operations
	eConstruct, eIndex, eComponent,

	// Types
	eNone, eInt32, eFloat32,
	eVec2, eVec3, eVec4,
	eMat2, eMat3, eMat4,

	// Shader inputs and outputs
	eLayoutInput,
	eLayoutOutput,
	ePushConstants,

	// Arithmetic
	eAdd, eSub, eMul, eDiv,

	// Instrinsics
	eGlPosition
};

static constexpr const char *GLOA_STRINGS[] {
	"Construct", "Index", "Component",

	"None", "Int32", "Float32",
	"Vec2", "Vec3", "Vec4",
	"Mat2", "Mat3", "Mat4",

	"LayoutInput",
	"LayoutOutput",
	"PushConstants",

	"Add", "Sub", "Mul", "Div",

	"gl_Position"
};

// TODO: size information as well...
constexpr size_t gloa_type_offset(gloa x)
{
	// NOTE: this is not the same as the actual size
	switch (x) {
	case eMat4:
		return 16 * sizeof(float);
	default:
		break;
	}

	throw "(cppsl) unknown type {} for offset";
}

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

// GLSL Compressed Intermediate Representation (graph)
struct gcir_graph {
	std::vector <gir_t> data;
	std::vector <std::vector <int>> refs;
};

// Compressing GIR into GCIR
gcir_graph compress(const gir_tree &);
