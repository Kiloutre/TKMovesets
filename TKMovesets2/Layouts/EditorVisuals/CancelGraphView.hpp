#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <imgui.h>

#include "AnimationPreview.hpp"
#include "CancelGraph.hpp"

class EditorVisuals;

// Read-only node graph of the moves a move cancels into, drawn in place of a move form's fields (T7 and retail T8).
// One node per move with its animation (still pose, plays on hover); edges show the cancels' inputs and requirements.
// Drag to pan, wheel to zoom, click a move to open it. What depends on the game comes from the Callbacks.
class CancelGraphView
{
public:
	struct Callbacks {
		// The graph of the form's move (BuildCancelGraph)
		std::function<CancelGraph(int depth, bool incoming, bool hideCommon)> build;
		// Animation of a node's move, for its thumbnail (token and load; the active frames are filled from the node)
		std::function<AnimationPreview::Input(const CancelGraph::Node&)> animation;
		// Opens the move's form
		std::function<void(uint32_t moveId)> open;
		// Legend of CancelGraph::Kind::Screw (retail T8: the tornado)
		const char* screwLabel = "edition.cancel_graph.kind_screw";
	};
private:
	CancelGraph m_graph;
	bool m_built = false;
	int m_depth = defaultDepth;
	// Moves cancelling into the root: collapsed (not searched) until asked for
	bool m_incoming = defaultIncoming;
	// Co_ (common) moves: hidden unless asked for, they crowd many cancel lists
	bool m_showCommon = defaultShowCommon;
	// Node positions (graph units, top left) and the view: screen = origin + pan + position * zoom
	std::vector<ImVec2> m_positions;
	ImVec2 m_pan = ImVec2(0, 0);
	float m_zoom = 1.0f;
	bool m_fitPending = true;
	// Autoscroll (middle click without dragging): on, its anchor, and the click that ended it is not released yet
	bool m_autoScroll = false, m_autoScrollRelease = false;
	ImVec2 m_autoScrollOrigin = ImVec2(0, 0);
	// One thumbnail per move, kept across rebuilds while the move stays in the graph
	std::map<uint32_t, std::unique_ptr<AnimationPreview>> m_thumbnails;

	void Build(const Callbacks& callbacks);
	void Layout();
	void Fit(ImVec2 size);
	std::string CancelLine(const CancelGraph::Cancel& cancel) const;
	std::string RequirementLine(EditorVisuals* baseWindow, const std::pair<uint32_t, int32_t>& requirement) const;
	void RenderCanvas(const Callbacks& callbacks, EditorVisuals* baseWindow);
public:
	static constexpr float NodeWidth = 160, ThumbnailHeight = 120, NodeHeight = ThumbnailHeight + 46;
	static constexpr float ColumnGap = 280, RowGap = 36;
	static constexpr int MaxDepth = 30;
	static constexpr float FitMinZoom = 0.75f;
	// Initial state of new graphs, for offscreen captures (anim-preview-shot graph=on): open in place of the fields,
	// depth, incoming cancels and Co_ moves shown
	static inline bool openByDefault = false, defaultIncoming = false, defaultShowCommon = false;
	static inline int defaultDepth = 3;

	// Draws the graph in the remaining space of the window (baseWindow: its requirement labels). Returns false when
	// the user goes back to the fields.
	bool Render(const Callbacks& callbacks, EditorVisuals* baseWindow);
	// The moveset changed: rebuild the graph on the next frame
	void Invalidate() { m_built = false; }
	const CancelGraph& Graph() const { return m_graph; }
	// Icon button for the move forms (frame height): three linked nodes. Returns true when clicked.
	static bool IconButton(const char* id);
};
