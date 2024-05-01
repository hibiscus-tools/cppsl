#include <cassert>
#include <cstddef>
#include <functional>
#include <set>
#include <variant>
#include <vector>

#include <fmt/format.h>
#include <fmt/printf.h>

// range: range(A, B, S) or range(A, cond, S)
// repeat(range, ftn)
// branch(cond, ftn1(...), ftn2(...))
// cases(cond1, cond2, ..., ftnIf1(...), ftnIf2(...), ..., ftnElse())
// where(cond, ftn1(...), ftn2(...))

// GLSL Operations/Annotations
enum gloa : int {
	eFetch, eFetchArray, eStore, eStoreArray,
	eConstruct, eComponent,

	eFloat32, eVec2, eVec3, eVec4,

	eLayoutInput,
	eLayoutOutput,

	eAdd, eSub, eMul, eDiv
};

static constexpr const char *GLOA_STRINGS[] {
	"Fetch", "FetchArray", "Store", "StoreArray",
	"Construct", "Component",

	"Float32", "Vec2", "Vec3", "Vec4",

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

// Perform constant expression simplification if the node is cexpr
gir_tree ceval_construct(const std::vector <gir_tree> &);
gir_tree ceval_component(const std::vector <gir_tree> &);

gir_tree ceval(const gir_tree &gt)
{
	if (!gt.cexpr)
		return gt;

	if (std::holds_alternative <gloa> (gt.data)) {
		gloa x = std::get <gloa> (gt.data);

		// TODO: table/dispatcher
		switch (x) {
		case eConstruct:
			return ceval_construct(gt.children);
			break;
		case eComponent:
			return ceval_component(gt.children);
			break;
		default:
			break;
		}
	}

	return gt;
}

// Primitive types
struct f32;
struct vec2;

// NOTE: Constructors for primitive types are simply initializers; the types
// themself do not contain additional data, only interfaces to it.

struct f32 : gir_tree {
	f32(const gir_tree &gt) : gir_tree(gt) {}

	f32(float x = 0.0f) : gir_tree {
		gir_tree::cfrom(eConstruct, {
			gir_tree::cfrom(eFloat32),
			gir_tree::cfrom(x)
		})
	} {}
};

// Component references
enum glcomponents : int {
	cX, cY, cZ, cW,
};

// TODO: use OR for composite components
template <typename T, int N>
struct vector_type {
	using type = T;
	static constexpr int components = N;
};

// TODO: requires vector_type info (alias)
template <typename T, glcomponents glc>
struct component_ref {
	// Preload construction information
	using vtype = typename T::alias::type;

	std::reference_wrapper <T> ref;

	component_ref(const std::reference_wrapper <T> &ref_) : ref(ref_) {}

	// Assigning to the value
	component_ref &operator=(const vtype &v) {
		gir_tree ref_tree = ref.get();

		std::vector <gir_tree> cmps;
		cmps.resize(T::alias::components);
		for (int i = 0; i < T::alias::components; i++) {
			if (i == glc) {
				cmps[i] = ceval(v);
			} else {
				// Gather the atomic components
				gir_tree ct = gir_tree::from(eComponent, ref_tree.cexpr, {
					gir_tree::cfrom(i),
					ref_tree
				});

				cmps[i] = ceval(ct);
			}
		}

		cmps.insert(cmps.begin(), gir_tree::cfrom(T::alias::components));
		cmps.insert(cmps.begin(), gir_tree::cfrom(T::native_type));

		T &tref = ref.get();
		tref.rehash(eConstruct, ref_tree.cexpr, cmps);

		return *this;
	}

	// Fetching the result
	operator vtype() const {
		gir_tree ref_tree = ref.get();
		gir_tree cmp_tree = gir_tree::from(eComponent, ref_tree.cexpr, {
			gir_tree::cfrom(glc),
			ref_tree
		});

		return ceval(cmp_tree);
	}
};

struct vec2 : gir_tree {
	static constexpr gloa native_type = eVec2;

	using alias = vector_type <f32, 2>;

	component_ref <vec2, cX> x = std::ref(*this);
	component_ref <vec2, cY> y = std::ref(*this);

	vec2(const gir_tree &gt) : gir_tree(gt) {}

	vec2(float x = 0.0f, float y = 0.0f) : gir_tree {
		gir_tree::cfrom(eConstruct, {
			gir_tree::cfrom(eVec2),
			gir_tree::cfrom(2),
			gir_tree::cfrom(x),
			gir_tree::cfrom(y)
		})
	} {}
};

struct vec3 : gir_tree {
	static constexpr gloa native_type = eVec3;

	using alias = vector_type <f32, 3>;

	component_ref <vec3, cX> x = std::ref(*this);
	component_ref <vec3, cY> y = std::ref(*this);
	component_ref <vec3, cZ> z = std::ref(*this);

	vec3(const gir_tree &gt) : gir_tree(gt) {}

	vec3(float x = 0.0f, float y = 0.0f, float z = 0.0f) : gir_tree {
		gir_tree::cfrom(eConstruct, {
			gir_tree::cfrom(eVec3),
			gir_tree::cfrom(3),
			gir_tree::cfrom(x),
			gir_tree::cfrom(y),
			gir_tree::cfrom(z),
		})
	} {}
};

struct vec4 : gir_tree {
	static constexpr gloa native_type = eVec4;

	using alias = vector_type <f32, 4>;

	component_ref <vec4, cX> x = std::ref(*this);
	component_ref <vec4, cY> y = std::ref(*this);
	component_ref <vec4, cZ> z = std::ref(*this);
	component_ref <vec4, cW> w = std::ref(*this);

	vec4(const gir_tree &gt) : gir_tree(gt) {}

	// Copies need some care
	vec4(const vec4 &v) : gir_tree(v) {}

	vec4 operator=(const vec4 &v) {
		if (this != &v)
			gir_tree::operator=(v);
		return *this;
	}

	// TODO: disable moves

	vec4(float x = 0.0f, float y = 0.0f, float z = 0.0f, float w = 0.0f) : gir_tree {
		gir_tree::cfrom(eConstruct, {
			gir_tree::cfrom(eVec4),
			gir_tree::cfrom(4),
			gir_tree::cfrom(x),
			gir_tree::cfrom(y),
			gir_tree::cfrom(z),
			gir_tree::cfrom(w)
		})
	} {}

	vec4(const vec2 &v, float z = 0.0f, float w = 0.0f) : gir_tree {
		gir_tree::cfrom(eConstruct, {
			gir_tree::cfrom(eVec4),
			gir_tree::cfrom(3),
			v,
			gir_tree::cfrom(z),
			gir_tree::cfrom(w)
		})
	} {}
};

// TODO: arithmetic
gir_tree binary_operation(const gir_tree &A, const gir_tree &B, gloa op)
{
	gir_tree C;

	C.data = op;
	C.children.push_back(A);
	C.children.push_back(B);

	return C;
}

f32 operator+(f32 A, f32 B)
{
	return f32(binary_operation(A, B, eAdd));
}

// TODO: requires
template <typename T, int Binding>
struct layout_input {
	operator T() const {
		return gir_tree::vfrom(eLayoutInput, {
			gir_tree::cfrom(T::native_type),
			gir_tree::cfrom(Binding),
		});
	}
};

template <typename T, int Binding>
struct layout_output : T {
	using T::T;
};

// TODO: tree simplification/optimizer routine

namespace intrinsics {

struct vertex {
	vec4 gl_Position;
};

}

std::string format_as(const gir_t &v)
{
	struct visitor {
		std::string operator()(int x) {
			return std::to_string(x);
		}

		std::string operator()(float x) {
			return std::to_string(x);
		}

		std::string operator()(gloa x) {
			return GLOA_STRINGS[x];
		}
	};

	return std::visit(visitor {}, v);
}

std::string format_as(const gir_tree &gt, size_t indent = 0)
{
	std::string tab(indent, ' ');

	std::string variant = "int";
	if (std::holds_alternative <float> (gt.data))
		variant = "float";
	if (std::holds_alternative <gloa> (gt.data))
		variant = "gloa";

	std::string out = fmt::format("{}({:>5s}: {})", tab, variant, gt.data);
	for (const auto &cgt : gt.children)
		out += "\n" + format_as(cgt, indent + 4);
	return out;
}

// Constant expression evaluation of component access
gir_tree ceval_construct(const std::vector <gir_tree> &nodes)
{
	gloa type = std::get <gloa> (nodes[0].data);

	// TODO: branch on vector types
	// for scalar types, simply do ceval(1)
	if (type == eFloat32) {
		return ceval(nodes[1]);
	} else if (type == eVec4) {
		int count = std::get <int> (nodes[1].data);

		// std::vector <float> vv(4);
		// for (size_t i = 0; i < 4; i++) {
		// 	gir_tree ci = ceval(nodes[i + 2]);
		// 	float cix = std::get <float> (ci.data);
		// 	vv[i] = cix;
		// }
		assert(nodes.size() == 6);

		return gir_tree::cfrom(eConstruct, {
			gir_tree::cfrom(eVec4),
			gir_tree::cfrom(4),
			ceval(nodes[2]),
			ceval(nodes[3]),
			ceval(nodes[4]),
			ceval(nodes[5])
		});
	}

	assert(false);
}

using vector_variant = std::variant <std::vector <int>, std::vector <float>>;

struct vector_variant_component_visitor {
	int index;

	template <typename T>
	gir_tree operator()(const std::vector <T> &vv) {
		return gir_tree::cfrom(vv.at(index));
	}
};

vector_variant ceval_vector_variant(const gir_tree &gt)
{
	gir_tree cgt = ceval(gt);

	assert(std::get <gloa> (cgt.data) == eConstruct);

	auto nodes = cgt.children;
	gloa type = std::get <gloa> (nodes[0].data);

	if (type == eVec4) {
		std::vector <float> vv(4);
		for (size_t i = 0; i < 4; i++) {
			gir_tree ci = ceval(nodes[i + 2]);
			float cix = std::get <float> (ci.data);
			vv[i] = cix;
		}

		return vv;
	}

	assert(false);
}

int ceval_int(const gir_tree &gt)
{
	return std::get <int> (gt.data);
}

gir_tree ceval_component(const std::vector <gir_tree> &nodes)
{
	int index = ceval_int(nodes[0]);
	vector_variant v = ceval_vector_variant(nodes[1]);
	return std::visit(vector_variant_component_visitor { index }, v);
}

void vertex_shader(const layout_input <vec2, 0> &position, intrinsics::vertex &vintr)
{
	vintr.gl_Position = vec4(position, 0, 1);
}

void fragment_shader(layout_output <vec4, 0> &fragment)
{
	fragment = vec4(1, 0, 1, 1);
	fragment.x = 0.5;
}

// Translate into GLSL source code
std::string gloa_type_string(gloa x)
{
	switch (x) {
	case eFloat32:
		return "float";
	case eVec2:
		return "vec2";
	case eVec3:
		return "vec3";
	case eVec4:
		return "vec4";
	default:
		break;
	}

	return "<?>";
}

static const std::string LAYOUT_INPUT_PREFIX = "_lin";
static const std::string LAYOUT_OUTPUT_PREFIX = "_lout";

struct identifier {
	gloa type;
	std::string prefix;
	int id;
	std::string postfix; // Only for arrays, components

	std::string full_id() const {
		return prefix + std::to_string(id) + postfix;
	}
};

struct statement {
	identifier loc;
	std::string source;

	std::string full_loc() const {
		return loc.full_id();
	}
};

using statement_list = std::vector <statement>;

std::vector <statement> translate(const gir_tree &, int &);

std::string format_as(const statement &s)
{
	return gloa_type_string(s.loc.type) + " " + s.loc.full_id() + " = " + s.source + ";";
}

std::string format_as(const std::vector <statement> &statements)
{
	std::string out = "";
	for (const auto &s : statements)
		out += fmt::format("{}", s) + "\n";
	return out;
}

std::vector <statement> translate_layout_input(const std::vector <gir_tree> &nodes, int &generator)
{
	gloa type = std::get <gloa> (nodes[0].data);
	int binding = std::get <int> (nodes[1].data);
	return {
		statement {
			{ type, "_v", generator++ },
			fmt::format("{}{}", LAYOUT_INPUT_PREFIX, binding)
		}
	};
}

// TODO: return the location (identifier) of the stored result, can choose to inline it maybe
std::vector <statement> translate_construct(const std::vector <gir_tree> &nodes, int &generator)
{
	gloa type = std::get <gloa> (nodes[0].data);
	if (type == eFloat32) {
		auto v = translate(nodes[1], generator);
		statement vlast = v.back();
		statement after = { { eFloat32, "_v", generator++}, vlast.full_loc() };
		v.push_back(after);
		return v;
	} else if (type == eVec4) {
		int count = std::get <int> (nodes[1].data);

		std::string source_after = "vec4(";

		statement_list v;
		for (int i = 0; i < count; i++) {
			auto vi = translate(nodes[i + 2], generator);
			v.insert(v.end(), vi.begin(), vi.end());
			source_after += vi.back().full_loc();
			if (i < count - 1)
				source_after += ", ";
		}

		source_after += ")";

		// TODO: functional call helper
		statement after = { { eVec4, "_v", generator++ }, source_after };
		v.push_back(after);
		return v;
	}

	return {};

	// return { { { eFloat32, "_v", 0 }, { "1.0" } } };
}

std::vector <statement> translate(const gir_tree &gt, int &generator)
{
	struct visitor {
		gir_tree gt;
		int &generator;

		std::vector <statement> operator()(gloa x) {
			switch (x) {
			case eConstruct:
				return translate_construct(gt.children, generator);
			case eLayoutInput:
				return translate_layout_input(gt.children, generator);
			default:
				break;
			}

			assert(false);
		}

		std::vector <statement> operator()(int x) {
			assert(false);
			return {
				statement {
					identifier {
					}
				}
			};
		}

		std::vector <statement> operator()(float x) {
			return {
				// TODO: templated from constructors
				statement {
					identifier {
						eFloat32,
						"_v",
						generator++
					},
					fmt::format("{}", x)
				}
			};
		}
	};

	return std::visit(visitor { gt, generator }, gt.data);
}

std::set <std::pair <gloa, int>> used_layout_input_bindings(const gir_tree &gt)
{
	if (std::holds_alternative <gloa> (gt.data)) {
		gloa x = std::get <gloa> (gt.data);
		if (x == eLayoutInput) {
			gloa type = std::get <gloa> (gt.children[0].data);
			int binding = std::get <int> (gt.children[1].data);
			return { { type, binding } };
		}
	}

	std::set <std::pair <gloa, int>> used;
	for (const gir_tree &cgt : gt.children) {
		auto cused = used_layout_input_bindings(cgt);
		used.insert(cused.begin(), cused.end());
	}

	return used;
}

// TODO: support multi output programs

std::string translate_vertex_shader(const intrinsics::vertex &vin)
{
	int generator = 0;
	auto statements = translate(vin.gl_Position, generator);

	// Grab all used bindings
	auto used_lib = used_layout_input_bindings(vin.gl_Position);

	// Fill in the rest of the program
	std::string code = "#version 450\n";
	for (auto [type, binding] : used_lib) {
		code += fmt::format("layout (location = {}) out {} {}{};\n",
			binding, gloa_type_string(type), LAYOUT_INPUT_PREFIX, binding);
	}

	code += "void main() {\n";
	for (const statement &s : statements)
		code += fmt::format("  {}\n", s);

	code += fmt::format("  gl_Position = {};\n", statements.back().full_loc());
	code += "}\n";

	return code;
}

template <typename T, int N>
std::string translate_fragment_shader(const layout_output <T, N> &fout)
{
	int generator = 0;
	auto statements = translate(fout, generator);

	// Fill in the rest of the program
	std::string code = "#version 450\n";
	code += "layout (location = 0) out " + gloa_type_string(T::native_type) + " " + LAYOUT_OUTPUT_PREFIX + "0;\n";
	code += "void main() {\n";
	for (const statement &s : statements)
		code += fmt::format("  {}\n", s);
	// TODO: type checking for the sake of sanity
	code += "  " + LAYOUT_OUTPUT_PREFIX + "0 = " + statements.back().full_loc() + ";\n";
	code += "}\n";

	return code;
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
