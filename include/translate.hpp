#pragma once

#include <functional>
#include <optional>
#include <string>
#include <type_traits>

#include <fmt/format.h>

#include "gir.hpp"
#include "core.hpp"

// Translate into GLSL source code
struct identifier {
	gloa type;
	std::string prefix;
	int id;
	std::string postfix; // TODO: obselete? Only for arrays, components, etc.

	std::string full_id() const {
		if (type == eNone)
			return prefix + postfix;

		return prefix + std::to_string(id) + postfix;
	}

	static identifier from(gloa type, int &generator, const std::string &postfix) {
		return { type, "_v", generator++, postfix };
	}

	static identifier builtin_from(const std::string &prefix) {
		return { eNone, prefix };
	}
};

struct statement {
	identifier loc;
	std::string source;

	std::string full_loc() const {
		return loc.full_id();
	}

	static statement from(gloa type, const std::string &source, int &generator, const std::string &postfix = "") {
		return { identifier::from(type, generator, postfix), source };
	}

	static statement builtin_from(const std::string &prefix, const std::string &source) {
		return { identifier::builtin_from(prefix), source };
	}
};

using statement_list = std::vector <statement>;

// Un-templated structures
struct unt_layout_output {
	gloa type;
	int binding;
	gir_tree gt;
};

// TODO: support multi output programs
namespace detail {

// TODO: storage buffer outputs
// std::string translate_vertex_shader(const intrinsics::vertex &, const std::vector <unt_layout_output> &);
// std::string translate_fragment_shader(const std::vector <unt_layout_output> &);

std::string translate_gir_tree(const gir_tree &, const std::vector <unt_layout_output> &);

}

// TODO: template
// inline std::string translate_vertex_shader(const intrinsics::vertex &vintr, const std::vector <unt_layout_output> &louts)
// {
// 	return detail::translate_vertex_shader(vintr, louts);
// }
//
// inline std::string translate_fragment_shader(const std::vector <unt_layout_output> &louts)
// {
// 	return detail::translate_fragment_shader(louts);
// }

enum class Stage {
	Vertex,
	Fragment,
	// ...
	Compute
};

// TODO: check that all arguments are permissible
template <typename ... Args>
std::tuple <std::decay_t <Args>...> args_for_shader(const std::function <void (Args...)> &ftn)
{
	return {};
}

template <typename ... Args>
std::tuple <std::decay_t <Args>...> args_for_shader(void (&ftn)(Args...))
{
	return std::tuple <std::decay_t <Args>...> {};
}

// Gather all output types
struct shader_outputs {
	std::optional <intrinsics::vertex> vintr;
	std::vector <unt_layout_output> louts;
};

template <typename T>
struct gather_shader_single_output {
	shader_outputs operator()(const T &) {
		return {};
	}
};

template <>
struct gather_shader_single_output <intrinsics::vertex> {
	shader_outputs operator()(const intrinsics::vertex &vintr) {
		return { .vintr = vintr };
	}
};

template <typename T, int N>
struct gather_shader_single_output <layout_output <T, N>> {
	shader_outputs operator()(const layout_output <T, N> &lout) {
		unt_layout_output ulo { T::native_type, N, lout };
		return { {}, { ulo } };
	}
};

// TODO: pass shader stage to check vintr is false all the time
template <typename T, typename ... Args>
struct gather_shader_outputs {
	gather_shader_outputs() = default;
	gather_shader_outputs(void (&)(T, Args...)) {}
	gather_shader_outputs(const std::function <void (T, Args...)> &) {}

	shader_outputs operator()(const T &t, const Args &... args) {
		gather_shader_single_output <std::decay_t <T>> singler;
		shader_outputs vouts = singler(t);
		shader_outputs later;
		if constexpr (sizeof...(args)) {
			auto later_gatherer = gather_shader_outputs <Args...> {};
			later = later_gatherer(args...);
		}

		if (vouts.vintr) {
			if (later.vintr)
				fmt::system_error(1, "(cppsl) only one instance of intrinsics::vertex is allowed per vertex shader");
		} else {
			vouts.vintr = later.vintr;
		}

		vouts.louts.insert(vouts.louts.begin(), later.louts.begin(), later.louts.end());
		return vouts;
	}
};

template <Stage stage, typename F>
struct translate_dispatcher {
	std::string operator()(const F &ftn) {
		return "";
	}
};

template <typename F>
struct translate_dispatcher <Stage::Vertex, F> {
	std::string operator()(const F &ftn) {
		// TODO: check argument compatibility before, then no need for optional vintr
		auto args = args_for_shader(ftn);
		auto gatherer = gather_shader_outputs(ftn);
		std::apply(ftn, args);
		shader_outputs souts = std::apply(gatherer, args);

		// Unify all outputs into a single tree
		std::vector <gir_tree> outputs;

		bool cexpr = true;

		// TODO: check for presence of vintr
		vec4 gl_Position = souts.vintr->gl_Position;

		cexpr &= gl_Position.cexpr;
		outputs.push_back(gir_tree::from(eGlPosition, gl_Position.cexpr, { gl_Position }));

		for (const unt_layout_output &lout : souts.louts) {
			cexpr &= lout.gt.cexpr;
			outputs.push_back(gir_tree::from(eLayoutOutput, lout.gt.cexpr, {
				gir_tree::cfrom(lout.binding),
				lout.gt
			}));
		}

		gir_tree unified = gir_tree::from(eNone, cexpr, outputs);

		return detail::translate_gir_tree(unified, souts.louts);
	}
};

template <typename F>
struct translate_dispatcher <Stage::Fragment, F> {
	std::string operator()(const F &ftn) {
		// TODO: check argument compatibility before, then no need for optional vintr
		auto args = args_for_shader(ftn);
		auto gatherer = gather_shader_outputs(ftn);
		std::apply(ftn, args);
		shader_outputs souts = std::apply(gatherer, args);

		// Unify all outputs into a single tree
		std::vector <gir_tree> outputs;

		bool cexpr = true;
		for (const unt_layout_output &lout : souts.louts) {
			cexpr &= lout.gt.cexpr;
			outputs.push_back(gir_tree::from(eLayoutOutput, lout.gt.cexpr, {
				gir_tree::cfrom(lout.binding),
				lout.gt
			}));
		}

		gir_tree unified = gir_tree::from(eNone, cexpr, outputs);

		return detail::translate_gir_tree(unified, souts.louts);
	}
};

template <Stage stage, typename F>
std::string translate(const F &ftn)
{
	return translate_dispatcher <stage, F> {} (ftn);
}
