#include <cassert>

#include <fmt/printf.h>

#include "fmt.hpp"
#include "gir.hpp"

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
