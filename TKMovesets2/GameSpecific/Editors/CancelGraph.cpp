#include <deque>
#include <map>

#include "CancelGraph.hpp"

CancelGraph BuildCancelGraph(const CancelGraphSource& source, uint32_t rootId, int maxDepth, bool incoming, bool hideCommon)
{
	CancelGraph graph;
	const uint32_t moveCount = source.MoveCount();
	if (rootId >= moveCount) return graph;

	std::map<uint32_t, int> nodeOf;
	// Co_ moves left out (hideCommon), by row, cached: names can be slow to build (retail T8 name lookups)
	std::map<uint32_t, bool> common;
	auto isHidden = [&](uint32_t row) {
		if (!hideCommon || row == rootId) return false;
		auto [it, added] = common.try_emplace(row, false);
		if (added) { it->second = source.MoveName(row).starts_with("Co_"); graph.hiddenCommon += it->second; }
		return it->second;
	};
	std::map<std::pair<int, int>, int> edgeOf;
	auto addNode = [&](uint32_t moveId, int depth, bool generic) {
		const auto [first, last] = source.ActiveFrames(moveId);
		graph.nodes.push_back({ .moveId = moveId, .name = source.MoveName(moveId), .depth = depth, .generic = generic,
			.externalCancels = source.ExternalCancels(moveId), .firstActiveFrame = first, .lastActiveFrame = last,
			.kind = source.MoveKind(moveId) });
		return nodeOf[moveId] = int(graph.nodes.size() - 1);
	};
	auto addCancel = [&](int from, int to, CancelGraph::Cancel&& cancel) {
		auto [it, added] = edgeOf.try_emplace({ from, to }, int(graph.edges.size()));
		if (added) graph.edges.push_back({ .from = from, .to = to });
		graph.edges[it->second].cancels.push_back(std::move(cancel));
	};

	addNode(rootId, 0, source.IsGeneric(rootId));
	// Nodes whose cancel list was followed: their edges to the root are already in the graph
	std::vector<bool> expanded;
	std::deque<int> queue{ 0 };
	while (!queue.empty())
	{
		const int nodeIndex = queue.front();
		queue.pop_front();
		const uint32_t moveId = graph.nodes[nodeIndex].moveId;
		const int depth = graph.nodes[nodeIndex].depth;
		// The root is always expanded, even when generic (idle, ...)
		if (nodeIndex != 0 && graph.nodes[nodeIndex].generic) continue;
		if (expanded.size() <= size_t(nodeIndex)) expanded.resize(nodeIndex + 1, false);
		expanded[nodeIndex] = true;

		source.ForEachCancel(moveId, [&](uint32_t target, CancelGraph::Cancel&& cancel) {
			const int64_t resolved = source.ResolveMove(target);
			if (resolved < 0 || resolved >= moveCount) return;
			const uint32_t targetId = uint32_t(resolved);
			if (isHidden(targetId)) return;
			auto found = nodeOf.find(targetId);
			int to;
			if (found != nodeOf.end()) to = found->second; // Already in the graph or a loop into itself: an edge only
			else if (depth >= maxDepth || graph.nodes.size() >= CancelGraph::MaxNodes) {
				graph.nodes[nodeIndex].cutOff = true;
				graph.truncated |= graph.nodes.size() >= CancelGraph::MaxNodes;
				return;
			}
			else {
				// Stop at generic moves: reached through an alias, or a move that an alias points to
				to = addNode(targetId, depth + 1, target >= 0x8000 || source.IsGeneric(targetId));
				queue.push_back(to);
			}
			addCancel(nodeIndex, to, std::move(cancel));
		});
	}

	if (incoming)
	{
		for (uint32_t moveId = 0; moveId < moveCount; ++moveId)
		{
			if (moveId == rootId) continue;
			if (auto found = nodeOf.find(moveId); found != nodeOf.end() && size_t(found->second) < expanded.size() && expanded[found->second]) continue;
			int from = -1;
			source.ForEachCancel(moveId, [&](uint32_t target, CancelGraph::Cancel&& cancel) {
				if (from == -2) return; // A hidden Co_ move
				if (source.ResolveMove(target) != int64_t(rootId)) return;
				if (from == -1) {
					if (isHidden(moveId)) { from = -2; return; }
					auto found = nodeOf.find(moveId);
					if (found != nodeOf.end()) from = found->second; // Already shown on the outgoing side
					else if (graph.nodes.size() >= 2 * CancelGraph::MaxNodes) { graph.truncated = true; return; }
					else from = addNode(moveId, -1, source.IsGeneric(moveId));
				}
				addCancel(from, 0, std::move(cancel));
			});
		}
	}
	return graph;
}
