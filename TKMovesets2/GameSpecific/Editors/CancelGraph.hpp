#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// Read-only graph of the moves a move cancels into, for the move forms' cancel graph (T7 and retail T8). Built by
// BuildCancelGraph from a CancelGraphSource, which each editor implements over its own records.
struct CancelGraph
{
	// What the move is, for its colour (and the colour of the links leading to it). Screw (T8: tornado) wins over
	// throw, throw over attack.
	enum class Kind { Other, Attack, Throw, Screw };
	struct Node {
		uint32_t moveId;
		std::string name;
		// 0 = the root, n = reached after n cancels, -1 = cancels into the root (incoming)
		int depth;
		// Generic move (alias target, or reached through an alias): shown, not expanded
		bool generic = false;
		// Has cancels that were not followed because of the depth or node limit
		bool cutOff = false;
		// Its cancel list lies outside the moveset (some retail T8 moves): not shown
		bool externalCancels = false;
		// The move's active frames (0 when it has none), for its thumbnail's still pose
		int firstActiveFrame = 0, lastActiveFrame = 0;
		Kind kind = Kind::Other;
	};
	struct Cancel {
		uint64_t command;
		// Command text ([d/f+1]), or empty for an input sequence or a cancel without input
		std::string input;
		// Input sequence id when the command is one, else -1
		int inputSequence = -1;
		// Requirements other than the list's terminator and empty (0) entries: {condition, parameter}
		std::vector<std::pair<uint32_t, int32_t>> requirements;
		// Comes from a grouped cancel list
		bool grouped = false;
		// Row of the cancel (or of the grouped cancel)
		uint32_t cancelId = 0;
	};
	// Every cancel from one move to another (several cancels between the same two moves share an edge)
	struct Edge {
		int from, to; // node indexes
		std::vector<Cancel> cancels;
	};
	std::vector<Node> nodes;
	std::vector<Edge> edges;
	// The node limit was reached
	bool truncated = false;
	// Co_ (common) moves left out, when asked to hide them
	size_t hiddenCommon = 0;
	static constexpr size_t MaxNodes = 400;
};

// What BuildCancelGraph reads from a moveset
class CancelGraphSource
{
public:
	virtual ~CancelGraphSource() = default;
	virtual uint32_t MoveCount() const = 0;
	// Move row of a move id as written in a cancel (aliases from 0x8000 resolved), -1 when it is not a move
	virtual int64_t ResolveMove(uint32_t moveId) const = 0;
	// A generic move: the target of an alias
	virtual bool IsGeneric(uint32_t row) const = 0;
	virtual std::string MoveName(uint32_t row) const = 0;
	virtual std::pair<int, int> ActiveFrames(uint32_t row) const = 0;
	// As the move list colours it: attack (complete hit data), throw (name ending in _y), screw / tornado (a hit
	// condition requires it)
	virtual CancelGraph::Kind MoveKind(uint32_t row) const = 0;
	// The move's cancel list lies outside the moveset, so it cannot be followed
	virtual bool ExternalCancels(uint32_t row) const { return false; }
	// Calls visit(move id as written, cancel) for each cancel of the move's main cancel list, grouped cancels expanded
	virtual void ForEachCancel(uint32_t row, const std::function<void(uint32_t, CancelGraph::Cancel&&)>& visit) const = 0;
};

// Moves reached from [root] through its cancel list, breadth first, up to [maxDepth] cancels away. [hideCommon]: moves named Co_... (common moves, but not the root) are left out. Generic moves,
// moves already in the graph and loops are not expanded again; at most CancelGraph::MaxNodes moves. [incoming]: also
// the moves whose cancel list leads to [root] (one level).
CancelGraph BuildCancelGraph(const CancelGraphSource& source, uint32_t root, int maxDepth, bool incoming, bool hideCommon = false);
