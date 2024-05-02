#include "gir.hpp"

// Replace contents
void gir_tree::rehash(gir_t data_, bool cexpr_, const std::vector <gir_tree> &children_)
{
	data = data_;
	cexpr = cexpr_;
	children = children_;
}

// Single element construction
gir_tree gir_tree::from(gir_t data, bool cexpr)
{
	return {
		.data = data,
		.cexpr = cexpr,
		.children = {}
	};
}

// With children
gir_tree gir_tree::from(gir_t data, bool cexpr, const std::vector <gir_tree> &children)
{
	return {
		.data = data,
		.cexpr = cexpr,
		.children = children
	};
}

// Constant alternatives
gir_tree gir_tree::cfrom(gir_t data)
{
	return {
		.data = data,
		.cexpr = true,
		.children = {}
	};
}

gir_tree gir_tree::cfrom(gir_t data, const std::vector <gir_tree> &children)
{
	return {
		.data = data,
		.cexpr = true,
		.children = children
	};
}

// Variable alternatives
// TODO: variadics?
gir_tree gir_tree::vfrom(gir_t data)
{
	return {
		.data = data,
		.cexpr = false,
		.children = {}
	};
}

gir_tree gir_tree::vfrom(gir_t data, const std::vector <gir_tree> &children)
{
	return {
		.data = data,
		.cexpr = false,
		.children = children
	};
}
