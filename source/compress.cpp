#include <map>
#include <iostream>

#include "gir.hpp"

// Performing compressions on a GIR
int fill_compressed_representation(const gir_tree &gt, gcir_graph &graph)
{
	int size = graph.data.size();
	graph.data.push_back(gt.data);
	graph.refs.push_back({});

	std::vector <int> refs;
	for (const auto &cgt : gt.children)
		refs.push_back(fill_compressed_representation(cgt, graph));
	graph.refs[size] = refs;
	return size;
}

bool tree_cmp(const gcir_graph &gcir, int A, int B)
{
	if (gcir.data[A] != gcir.data[B])
		return false;

	if (gcir.refs[A].size() != gcir.refs[B].size())
		return false;

	for (size_t i = 0; i < gcir.refs[A].size(); i++) {
		if (!tree_cmp(gcir, gcir.refs[A][i], gcir.refs[B][i]))
			return false;
	}

	return true;
}

int tree_size(const gcir_graph &gcir, int T)
{
	int size = 1;
	for (const auto &C : gcir.refs[T])
		size += tree_size(gcir, C);
	return size;
}

int readdress_compressed(const gcir_graph &gcir, int T, gcir_graph &graph, std::map <int, int> &filled)
{
	if (filled.count(T))
		return filled[T];

	int size = graph.data.size();
	graph.data.push_back(gcir.data[T]);
	graph.refs.push_back({});
	filled[T] = size;

	std::vector <int> refs;
	for (int C : gcir.refs[T])
		refs.push_back(readdress_compressed(gcir, C, graph, filled));
	graph.refs[size] = refs;
	return size;
}

gcir_graph compress(const gir_tree &gt)
{
	gcir_graph graph;
	fill_compressed_representation(gt, graph);

	while (true) {
		// printf("BEFORE: %lu nodes\n", graph.data.size());

		std::vector <int> node_sizes;
		for (size_t i = 0; i < graph.data.size(); i++)
			node_sizes.push_back(tree_size(graph, i));

		int max = 0;
		int max_reffed = 0;

		std::map <int, int> equals;
		for (size_t i = 0; i < graph.data.size(); i++) {
			if (equals.count(i))
				continue;

			for (size_t j = i + 1; j < graph.data.size(); j++) {
				if (tree_cmp(graph, i, j)) {
					equals[j] = i;

					if (node_sizes[i] > max) {
						max = node_sizes[i];
						max_reffed = i;
					}

					// printf("equal branches: %lu, %lu\n", i, j);
				}
			}
		}

		if (equals.empty()) {
			// printf("Out of compressions, done.\n");
			break;
		}

		// printf("equal branch maps: CHOSEN %d\n", max_reffed);
		// for (auto [j, i] : equals) {
		// 	if (i != max_reffed)
		// 		continue;
		// 	printf("  %d -> %d\n", j, i);
		// }

		// Replace references
		for (size_t i = 0; i < graph.data.size(); i++) {
			for (int &r : graph.refs[i]) {
				if (equals.count(r) && equals[r] == max_reffed)
					r = max_reffed;
			}
		}

		// Remove unused nodes by reconstructing the graph
		gcir_graph regraph;
		std::map <int, int> filled;
		readdress_compressed(graph, 0, regraph, filled);
		graph = regraph;

		// printf("AFTER: %lu nodes\n", graph.data.size());

		// TODO: repeat until no more replacements...
	}

	return graph;
}
