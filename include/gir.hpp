#pragma once

#include <variant>
#include <vector>

// GLSL Operations/Annotations
enum gloa : int {
	eFetch, eFetchArray, eStore, eStoreArray,
	eConstruct, eComponent,

	eInt32, eFloat32, eVec2, eVec3, eVec4,

	eLayoutInput,
	eLayoutOutput,

	eAdd, eSub, eMul, eDiv
};

static constexpr const char *GLOA_STRINGS[] {
	"Fetch", "FetchArray", "Store", "StoreArray",
	"Construct", "Component",

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
	void rehash(gir_t data_, bool cexpr_, const std::vector <gir_tree> &children_) {
		data = data_;
		cexpr = cexpr_;
		children = children_;
	}

	// Single element construction
	static gir_tree from(gir_t data, bool cexpr) {
		return {
			.data = data,
			.cexpr = cexpr,
			.children = {}
		};
	}

	// With children
	static gir_tree from(gir_t data, bool cexpr, const std::vector <gir_tree> &children) {
		return {
			.data = data,
			.cexpr = cexpr,
			.children = children
		};
	}

	// Constant alternatives
	static gir_tree cfrom(gir_t data) {
		return {
			.data = data,
			.cexpr = true,
			.children = {}
		};
	}

	static gir_tree cfrom(gir_t data, const std::vector <gir_tree> &children) {
		return {
			.data = data,
			.cexpr = true,
			.children = children
		};
	}

	// Variable alternatives
	// TODO: variadics?
	static gir_tree vfrom(gir_t data) {
		return {
			.data = data,
			.cexpr = false,
			.children = {}
		};
	}

	static gir_tree vfrom(gir_t data, const std::vector <gir_tree> &children) {
		return {
			.data = data,
			.cexpr = false,
			.children = children
		};
	}
};

// Performing constant expression simplifications
gir_tree ceval(const gir_tree &);
