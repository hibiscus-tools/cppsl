#pragma once

#include "gir.hpp"

// Primitive types
// struct f32;
// struct vec2;

// NOTE: Constructors for primitive types are simply initializers; the types
// themself do not contain additional data, only interfaces to it.

struct scalar_type {};

struct f32 : gir_tree {
	explicit f32(const gir_tree &gt) : gir_tree(gt) {}

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

		bool cexpr = ref_tree.cexpr & v.cexpr;

		T &tref = ref.get();
		tref.rehash(eConstruct, cexpr, cmps);

		return *this;
	}

	// Fetching the result
	operator vtype() const {
		gir_tree ref_tree = ref.get();
		gir_tree cmp_tree = gir_tree::from(eComponent, ref_tree.cexpr, {
			gir_tree::cfrom(glc),
			ref_tree
		});

		return vtype(ceval(cmp_tree));
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

	// Copies need some care
	vec3(const vec3 &v) : gir_tree(v) {}

	vec3 operator=(const vec3 &v) {
		if (this != &v)
			gir_tree::operator=(v);
		return *this;
	}

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
		gir_tree::from(eConstruct, v.cexpr, {
			gir_tree::cfrom(eVec4),
			gir_tree::cfrom(3),
			v,
			gir_tree::cfrom(z),
			gir_tree::cfrom(w)
		})
	} {}

	vec4(const vec3 &v, float w = 0.0f) : gir_tree {
		gir_tree::from(eConstruct, v.cexpr, {
			gir_tree::cfrom(eVec4),
			gir_tree::cfrom(2),
			v,
			gir_tree::cfrom(w)
		})
	} {}

	// Constructors involving f32
	vec4(const vec3 &v, f32 w) : gir_tree {
		gir_tree::cfrom(eConstruct, {
			gir_tree::cfrom(eVec4),
			gir_tree::cfrom(2),
			v, w
		})
	} {}
};

struct mat4 : gir_tree {
	static constexpr gloa native_type = eMat4;

	// NOTE: no components for now; that means no need for special care yet
	mat4(const gir_tree &gt) : gir_tree(gt) {}

	mat4(float x = 0.0f) : gir_tree {
		gir_tree::cfrom(eConstruct, {
				gir_tree::cfrom(eMat4),
				gir_tree::cfrom(1),
				gir_tree::cfrom(x),
		})
	} {}
};

// TODO: arithmetic
// TODO: header
inline gir_tree binary_operation(const gir_tree &A, const gir_tree &B, gloa op)
{
	return gir_tree::from(op, A.cexpr & B.cexpr, { A, B });
}

// TODO: templatize
inline f32 operator+(f32 A, f32 B)
{
	return f32(binary_operation(A, B, eAdd));
}

inline f32 operator*(f32 A, f32 B)
{
	return f32(binary_operation(A, B, eMul));
}

inline vec4 operator*(mat4 A, vec4 B)
{
	return vec4(binary_operation(A, B, eMul));
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

template <int Binding>
struct layout_input <vec3, Binding> {
	// TODO: function to generate this,
	// OR vec3 <Injected> specialization with required injection
	// and vec3 = vec3 <Regular>
	const f32 x = f32(gir_tree::vfrom(eComponent, {
		gir_tree::cfrom(0),
		gir_tree::vfrom(eLayoutInput, {
			gir_tree::cfrom(eVec3),
			gir_tree::cfrom(Binding),
		})
	}));

	const f32 y = f32(gir_tree::vfrom(eComponent, {
		gir_tree::cfrom(1),
		gir_tree::vfrom(eLayoutInput, {
			gir_tree::cfrom(eVec3),
			gir_tree::cfrom(Binding),
		})
	}));

	const f32 z = f32(gir_tree::vfrom(eComponent, {
		gir_tree::cfrom(2),
		gir_tree::vfrom(eLayoutInput, {
			gir_tree::cfrom(eVec3),
			gir_tree::cfrom(Binding),
		})
	}));

	layout_input() {}

	operator vec3() const {
		return gir_tree::vfrom(eLayoutInput, {
			gir_tree::cfrom(eVec3),
			gir_tree::cfrom(Binding),
		});
	}
};

template <typename T, int Binding>
struct layout_output : T {
	using T::T;

	template <typename A>
	layout_output &operator=(const A &v) {
		T::operator=(v);
		return *this;
	}
};

// NOTE: Only one push constants per shader, therefore no ID tracking is needed
template <typename T, typename ... Args>
void push_constants_members_proxy(size_t N, size_t offset, T &sub, Args &... args)
{
	// TODO: add size information to pad structures appropriately...
	sub = gir_tree::vfrom(ePushConstants, {
		gir_tree::cfrom(T::native_type),
		gir_tree::cfrom((int) N),
		gir_tree::cfrom((int) offset)
	});

	if constexpr (sizeof...(Args) > 0)
		push_constants_members_proxy(N + 1, offset + gloa_type_offset(T::native_type), args...);
}

template <typename ... Args>
void push_constants_members(Args &... args)
{
	push_constants_members_proxy(0, 0, args...);
}

namespace intrinsics {

struct vertex {
	vec4 gl_Position = vec4(0.0f);
};

}
