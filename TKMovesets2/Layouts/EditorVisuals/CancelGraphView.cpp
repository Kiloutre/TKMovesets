#include <algorithm>
#include <cmath>
#include <format>
#include <set>

#include <imgui_internal.h>

#include "CancelGraphView.hpp"
#include "EditorVisuals.hpp"
#include "Localization.hpp"

namespace
{
	const ImU32 CanvasColour = IM_COL32(18, 20, 26, 255);
	const ImU32 GridColour = IM_COL32(32, 35, 44, 255);
	const ImU32 NodeColour = IM_COL32(36, 40, 50, 255);
	const ImU32 BorderColour = IM_COL32(78, 86, 104, 255);
	const ImU32 RootColour = IM_COL32(230, 180, 60, 255);
	const ImU32 GenericColour = IM_COL32(70, 180, 190, 255);
	const ImU32 IncomingColour = IM_COL32(110, 190, 110, 255);
	const ImU32 HoverColour = IM_COL32(240, 240, 250, 255);
	const ImU32 EdgeColour = IM_COL32(120, 130, 155, 200);
	const ImU32 HighlightEdgeColour = IM_COL32(245, 215, 120, 255);
	const ImU32 TextColour = IM_COL32(230, 232, 238, 255);
	const ImU32 DimTextColour = IM_COL32(150, 156, 170, 255);
	const ImU32 LabelColour = IM_COL32(24, 27, 34, 235);

	// Move kinds: the node's border and the links leading to it (as the move list's attack and throw tints, and the
	// screw icon's orange)
	ImU32 KindColour(CancelGraph::Kind kind)
	{
		switch (kind) {
		case CancelGraph::Kind::Attack: return IM_COL32(215, 75, 75, 255);
		case CancelGraph::Kind::Throw: return IM_COL32(155, 95, 235, 255);
		case CancelGraph::Kind::Screw: return IM_COL32(240, 165, 40, 255);
		default: return 0;
		}
	}

	// Cancels and requirements written on an edge; the rest is in its tooltip
	constexpr int LabelCancels = 2, LabelRequirements = 3;
	// Below this zoom, edge labels are left out (hovering an edge's midpoint shows them)
	constexpr float LabelZoom = 0.6f;
	// Below this zoom, nodes are drawn without their thumbnail
	constexpr float ThumbnailZoom = 0.3f;

	ImVec2 Bezier(const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, float t)
	{
		const float u = 1 - t;
		return ImVec2(u * u * u * p0.x + 3 * u * u * t * p1.x + 3 * u * t * t * p2.x + t * t * t * p3.x,
			u * u * u * p0.y + 3 * u * u * t * p1.y + 3 * u * t * t * p2.y + t * t * t * p3.y);
	}
}

// -- Graph -- //

void CancelGraphView::Build(const Callbacks& callbacks)
{
	m_graph = callbacks.build(m_depth, m_incoming, !m_showCommon);
	m_built = true;
	Layout();

	std::set<uint32_t> shown;
	for (const auto& node : m_graph.nodes) shown.insert(node.moveId);
	std::erase_if(m_thumbnails, [&](const auto& entry) { return !shown.contains(entry.first); });
}

void CancelGraphView::Layout()
{
	// One column per depth (incoming moves on the left of the root), nodes in the order they were found: a move's
	// targets are next to each other, in the order of its cancel list.
	std::map<int, std::vector<int>> columns;
	for (int i = 0; i < int(m_graph.nodes.size()); ++i) columns[m_graph.nodes[i].depth].push_back(i);

	m_positions.assign(m_graph.nodes.size(), ImVec2(0, 0));
	for (const auto& [depth, column] : columns)
	{
		const float height = column.size() * NodeHeight + (column.size() - 1) * RowGap;
		for (size_t row = 0; row < column.size(); ++row) {
			m_positions[column[row]] = ImVec2(depth * (NodeWidth + ColumnGap), row * (NodeHeight + RowGap) - height / 2);
		}
	}
}

void CancelGraphView::Fit(ImVec2 size)
{
	if (m_positions.empty()) return;
	ImVec2 lo(FLT_MAX, FLT_MAX), hi(-FLT_MAX, -FLT_MAX);
	for (const auto& p : m_positions) {
		lo = ImMin(lo, p);
		hi = ImMax(hi, ImVec2(p.x + NodeWidth, p.y + NodeHeight));
	}
	const float margin = 60;
	const float width = hi.x - lo.x + 2 * margin, height = hi.y - lo.y + 2 * margin;
	const float fit = (std::min)(size.x / width, size.y / height);
	// Readable rather than complete: never below FitMinZoom (thumbnails and labels stay drawn); a graph that does not
	// fit then starts at the root, left aligned and centred vertically.
	m_zoom = std::clamp(fit, FitMinZoom, 1.0f);
	if (fit >= FitMinZoom)
		m_pan = ImVec2((size.x - (hi.x - lo.x) * m_zoom) / 2 - lo.x * m_zoom, (size.y - (hi.y - lo.y) * m_zoom) / 2 - lo.y * m_zoom);
	else {
		const ImVec2 root = m_positions[0];
		const float left = m_graph.nodes.size() > 0 && lo.x < root.x ? lo.x : root.x;
		m_pan = ImVec2(margin - left * m_zoom, size.y / 2 - (root.y + NodeHeight / 2) * m_zoom);
	}
}

std::string CancelGraphView::CancelLine(const CancelGraph::Cancel& cancel) const
{
	std::string line = cancel.inputSequence >= 0 ? std::format("{} #{}", _("edition.cancel_graph.input_sequence"), cancel.inputSequence)
		: cancel.input.empty() ? std::string(_("edition.cancel_graph.no_input")) : cancel.input;
	if (cancel.grouped) line += std::format(" ({})", _("edition.cancel_graph.grouped"));
	return line;
}

std::string CancelGraphView::RequirementLine(EditorVisuals* baseWindow, const std::pair<uint32_t, int32_t>& requirement) const
{
	const auto& [id, value] = requirement;
	const char* label = baseWindow->labels ? baseWindow->labels->GetText(int(id)) : nullptr;
	return label ? std::format("{} = {}", label, value) : std::format("{} / 0x{:x} = {}", id, id, value);
}

// -- Rendering -- //

bool CancelGraphView::IconButton(const char* id)
{
	const float size = ImGui::GetFrameHeight();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const bool clicked = ImGui::InvisibleButton(id, ImVec2(size, size));
	const bool hovered = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
	ImGui::SetItemTooltip("%s", _("edition.cancel_graph.open"));
	auto* draw = ImGui::GetWindowDrawList();
	const ImVec2 end(origin.x + size, origin.y + size);
	draw->AddRectFilled(origin, end, ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button), ImGui::GetStyle().FrameRounding);
	// Icon: one node on the left linked to two on the right
	const ImU32 ink = hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(170, 176, 190, 255);
	const float r = size * 0.11f;
	const ImVec2 a(origin.x + size * 0.27f, origin.y + size * 0.5f), b(origin.x + size * 0.73f, origin.y + size * 0.28f), c(origin.x + size * 0.73f, origin.y + size * 0.72f);
	draw->AddLine(a, b, ink, 1.5f);
	draw->AddLine(a, c, ink, 1.5f);
	draw->AddCircleFilled(a, r, ink);
	draw->AddCircleFilled(b, r, ink);
	draw->AddCircleFilled(c, r, ink);
	return clicked;
}

bool CancelGraphView::Render(const Callbacks& callbacks, EditorVisuals* baseWindow)
{
	bool open = true;
	ImGui::PushID(this);

	// Toolbar
	if (ImGui::Button(_("edition.cancel_graph.back"))) open = false;
	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(_("edition.cancel_graph.depth"));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6);
	if (ImGui::InputInt("##depth", &m_depth, 1, 5)) {
		m_depth = std::clamp(m_depth, 1, MaxDepth);
		m_built = false; m_fitPending = true;
	}
	ImGui::SameLine();
	if (ImGui::Checkbox(_("edition.cancel_graph.show_common"), &m_showCommon)) m_built = false;
	ImGui::SetItemTooltip("%s", _("edition.cancel_graph.show_common_help"));
	ImGui::SameLine();
	if (ImGui::Button(_("edition.cancel_graph.refresh"))) m_built = false;
	ImGui::SameLine();
	if (ImGui::Button(_("edition.cancel_graph.fit"))) m_fitPending = true;

	if (!m_built) Build(callbacks);

	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	ImGui::TextDisabled("%zu %s", m_graph.nodes.size(), _("references.moves"));
	if (m_graph.hiddenCommon > 0) {
		ImGui::SameLine();
		ImGui::TextDisabled("(%zu %s)", m_graph.hiddenCommon, _("edition.cancel_graph.hidden_common"));
	}
	if (m_graph.truncated) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.3f, 1), "%s", _("edition.cancel_graph.truncated"));
	}
	// Legend of the move kind colours
	for (auto kind : { CancelGraph::Kind::Attack, CancelGraph::Kind::Throw, CancelGraph::Kind::Screw }) {
		ImGui::SameLine();
		const float size = ImGui::GetTextLineHeight() * 0.7f;
		const ImVec2 at = ImGui::GetCursorScreenPos();
		const float y = at.y + (ImGui::GetFrameHeight() - size) / 2;
		ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(at.x, y), ImVec2(at.x + size, y + size), KindColour(kind), 2);
		ImGui::Dummy(ImVec2(size, ImGui::GetFrameHeight()));
		ImGui::SameLine(0, 4);
		ImGui::TextUnformatted(_(kind == CancelGraph::Kind::Attack ? "edition.cancel_graph.kind_attack" : kind == CancelGraph::Kind::Throw ? "edition.cancel_graph.kind_throw" : callbacks.screwLabel));
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	ImGui::SetItemTooltip("%s", _("edition.cancel_graph.help"));

	RenderCanvas(callbacks, baseWindow);
	ImGui::PopID();
	return open;
}

void CancelGraphView::RenderCanvas(const Callbacks& callbacks, EditorVisuals* baseWindow)
{
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const ImVec2 size = ImMax(ImGui::GetContentRegionAvail(), ImVec2(50, 50));
	const ImVec2 end(origin.x + size.x, origin.y + size.y);
	auto& io = ImGui::GetIO();

	if (m_fitPending) { Fit(size); m_fitPending = false; }

	ImGui::InvisibleButton("##cancelGraphCanvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
	ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
	const bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive();

	// Pan with any button, zoom around the pointer with the wheel
	if (active && (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2) || ImGui::IsMouseDragging(ImGuiMouseButton_Right, 2) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 2))) {
		m_pan.x += io.MouseDelta.x; m_pan.y += io.MouseDelta.y;
	}
	// Autoscroll, as in a browser: a middle click without dragging starts it; the view then moves towards the pointer,
	// faster the further it is from the click (both directions). Any click or Escape stops it.
	bool consumedClick = false;
	if (m_autoScroll) {
		const ImVec2 offset(io.MousePos.x - m_autoScrollOrigin.x, io.MousePos.y - m_autoScrollOrigin.y);
		auto speed = [](float d) { const float dead = 8; return std::abs(d) <= dead ? 0.0f : (d - (d > 0 ? dead : -dead)) * 6.0f; };
		m_pan.x -= speed(offset.x) * io.DeltaTime;
		m_pan.y -= speed(offset.y) * io.DeltaTime;
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			m_autoScroll = false;
			consumedClick = true;
		}
	}
	else if (!m_autoScrollRelease && hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Middle) && io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Middle] < 16) {
		m_autoScroll = true;
		m_autoScrollOrigin = io.MousePos;
	}
	// The click that stopped autoscroll does nothing else (until its button is released)
	if (consumedClick) m_autoScrollRelease = true;
	if (m_autoScrollRelease && !ImGui::IsAnyMouseDown()) { m_autoScrollRelease = false; consumedClick = true; }
	consumedClick |= m_autoScrollRelease;

	if (hovered && io.MouseWheel != 0) {
		const float zoom = std::clamp(m_zoom * std::pow(1.15f, io.MouseWheel), 0.1f, 2.5f);
		const ImVec2 mouse(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
		m_pan = ImVec2(mouse.x - (mouse.x - m_pan.x) * zoom / m_zoom, mouse.y - (mouse.y - m_pan.y) * zoom / m_zoom);
		m_zoom = zoom;
	}

	auto toScreen = [&](ImVec2 p) { return ImVec2(origin.x + m_pan.x + p.x * m_zoom, origin.y + m_pan.y + p.y * m_zoom); };
	const float nodeW = NodeWidth * m_zoom, nodeH = NodeHeight * m_zoom;

	// Incoming cancels button, on the left side of the root: shows (searches) or hides the moves cancelling into it.
	// The root stays in place, so the view is kept.
	const ImVec2 rootPos = m_positions.empty() ? ImVec2(0, 0) : toScreen(m_positions[0]);
	const float incomingRadius = 11 * (std::max)(m_zoom, 0.6f);
	const ImVec2 incomingCentre(rootPos.x - incomingRadius - 6 * m_zoom, rootPos.y + nodeH / 2);
	bool incomingHovered = false;
	if (hovered && !m_positions.empty()) {
		const float dx = io.MousePos.x - incomingCentre.x, dy = io.MousePos.y - incomingCentre.y;
		incomingHovered = dx * dx + dy * dy <= incomingRadius * incomingRadius;
	}
	if (!consumedClick && incomingHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] < 16) {
		m_incoming = !m_incoming;
		m_built = false;
	}

	// Node under the pointer (the last drawn is on top)
	int hoveredNode = -1;
	if (hovered && !incomingHovered) {
		for (int i = int(m_graph.nodes.size()) - 1; i >= 0; --i) {
			const ImVec2 p = toScreen(m_positions[i]);
			if (io.MousePos.x >= p.x && io.MousePos.x < p.x + nodeW && io.MousePos.y >= p.y && io.MousePos.y < p.y + nodeH) { hoveredNode = i; break; }
		}
	}
	if (!consumedClick && hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] < 16 && hoveredNode >= 0) {
		callbacks.open(m_graph.nodes[hoveredNode].moveId);
	}
	if (!consumedClick && hovered && hoveredNode < 0 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) Fit(size);

	auto* draw = ImGui::GetWindowDrawList();
	draw->PushClipRect(origin, end, true);
	draw->AddRectFilled(origin, end, CanvasColour);
	{
		// Background grid, moving with the view
		const float step = 64 * m_zoom;
		if (step >= 8) {
			for (float x = std::fmod(m_pan.x, step); x < size.x; x += step) draw->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, end.y), GridColour);
			for (float y = std::fmod(m_pan.y, step); y < size.y; y += step) draw->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(end.x, origin.y + y), GridColour);
		}
	}

	ImFont* font = ImGui::GetFont();
	const float fontSize = ImGui::GetFontSize() * m_zoom;
	const float lineHeight = fontSize * 1.2f;
	auto visible = [&](ImVec2 a, ImVec2 b) { return b.x >= origin.x && a.x <= end.x && b.y >= origin.y && a.y <= end.y; };

	// -- Edges -- //
	int hoveredEdge = -1;
	struct Label { int edge; ImVec2 at; };
	std::vector<Label> labels;
	for (int e = 0; e < int(m_graph.edges.size()); ++e)
	{
		const auto& edge = m_graph.edges[e];
		const ImVec2 a = toScreen(m_positions[edge.from]), b = toScreen(m_positions[edge.to]);
		ImVec2 p0, p1, p2, p3;
		if (edge.from == edge.to) {
			// Loop into itself: an arc above the node's right corner
			p0 = ImVec2(a.x + nodeW, a.y + nodeH * 0.3f);
			p3 = ImVec2(a.x + nodeW * 0.75f, a.y);
			p1 = ImVec2(p0.x + 70 * m_zoom, p0.y - 10 * m_zoom);
			p2 = ImVec2(p3.x + 20 * m_zoom, p3.y - 70 * m_zoom);
		}
		else if (b.x > a.x + nodeW) {
			// Forward: right side to left side
			p0 = ImVec2(a.x + nodeW, a.y + nodeH / 2);
			p3 = ImVec2(b.x, b.y + nodeH / 2);
			const float dx = (p3.x - p0.x) * 0.5f;
			p1 = ImVec2(p0.x + dx, p0.y);
			p2 = ImVec2(p3.x - dx, p3.y);
		}
		else {
			// Back to an earlier column or within one: under the nodes, bottom to bottom
			p0 = ImVec2(a.x + nodeW * 0.4f, a.y + nodeH);
			p3 = ImVec2(b.x + nodeW * 0.6f, b.y + nodeH);
			const float drop = 60 * m_zoom + std::abs(p3.x - p0.x) * 0.25f;
			p1 = ImVec2(p0.x, (std::max)(p0.y, p3.y) + drop);
			p2 = ImVec2(p3.x, (std::max)(p0.y, p3.y) + drop);
		}
		// Marker and label: a forward edge near its target (edges leaving one move fan out, so their labels stay apart)
		const bool forward = edge.from != edge.to && b.x > a.x + nodeW;
		const ImVec2 mid = Bezier(p0, p1, p2, p3, forward ? 0.7f : 0.5f);
		const ImVec2 lo = ImMin(ImMin(p0, p3), ImMin(p1, p2)), hi = ImMax(ImMax(p0, p3), ImMax(p1, p2));
		if (!visible(ImVec2(lo.x - 200, lo.y - 100), ImVec2(hi.x + 200, hi.y + 100))) continue;

		const bool highlight = hoveredNode >= 0 && (edge.from == hoveredNode || edge.to == hoveredNode);
		// The colour of the move it leads to (attack, throw, screw); otherwise incoming links green, others grey
		const ImU32 kindColour = KindColour(m_graph.nodes[edge.to].kind);
		const ImU32 colour = highlight ? HighlightEdgeColour : kindColour ? (kindColour & 0x00FFFFFF) | 0xD0000000 : m_graph.nodes[edge.from].depth < 0 ? IncomingColour : EdgeColour;
		const float thickness = (highlight ? 2.5f : 1.5f) * (std::max)(m_zoom, 0.5f);
		draw->AddBezierCubic(p0, p1, p2, p3, colour, thickness);
		// Arrow head, along the curve's end
		{
			const ImVec2 dir = ImVec2(p3.x - p2.x, p3.y - p2.y);
			const float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
			if (length > 0.01f) {
				const ImVec2 n(dir.x / length, dir.y / length), side(-n.y, n.x);
				const float s = 9 * (std::max)(m_zoom, 0.5f);
				draw->AddTriangleFilled(p3, ImVec2(p3.x - n.x * s + side.x * s * 0.5f, p3.y - n.y * s + side.y * s * 0.5f),
					ImVec2(p3.x - n.x * s - side.x * s * 0.5f, p3.y - n.y * s - side.y * s * 0.5f), colour);
			}
		}
		// Midpoint marker: hovering it shows the edge's cancels
		const float markerRadius = 4 * (std::max)(m_zoom, 0.6f);
		draw->AddCircleFilled(mid, markerRadius, colour);
		if (hovered && hoveredNode < 0) {
			const float dx = io.MousePos.x - mid.x, dy = io.MousePos.y - mid.y;
			if (dx * dx + dy * dy <= (markerRadius + 4) * (markerRadius + 4)) hoveredEdge = e;
		}
		if (m_zoom >= LabelZoom || highlight) labels.push_back({ e, mid });
	}

	// Labels over the edges: the inputs, and under each one its requirements
	for (const auto& [e, at] : labels)
	{
		const auto& edge = m_graph.edges[e];
		std::vector<std::pair<std::string, bool>> lines; // text, dim
		for (int c = 0; c < (std::min)(int(edge.cancels.size()), LabelCancels); ++c) {
			const auto& cancel = edge.cancels[c];
			lines.push_back({ CancelLine(cancel), false });
			for (int r = 0; r < (std::min)(int(cancel.requirements.size()), LabelRequirements); ++r)
				lines.push_back({ "  " + RequirementLine(baseWindow, cancel.requirements[r]), true });
			if (int(cancel.requirements.size()) > LabelRequirements)
				lines.push_back({ std::format("  +{} {}", cancel.requirements.size() - LabelRequirements, _("edition.cancel_graph.more")), true });
		}
		if (int(edge.cancels.size()) > LabelCancels)
			lines.push_back({ std::format("+{} {}", edge.cancels.size() - LabelCancels, _("edition.cancel_graph.more_cancels")), true });

		const float labelFont = fontSize * 0.85f, labelLine = labelFont * 1.2f, pad = 4 * m_zoom;
		float width = 0;
		for (const auto& [text, dim] : lines) width = (std::max)(width, font->CalcTextSizeA(labelFont, FLT_MAX, 0, text.c_str()).x);
		const ImVec2 boxMin(at.x - width / 2 - pad, at.y + 6 * m_zoom), boxMax(at.x + width / 2 + pad, at.y + 6 * m_zoom + lines.size() * labelLine + 2 * pad);
		if (!visible(boxMin, boxMax)) continue;
		draw->AddRectFilled(boxMin, boxMax, LabelColour, 3 * m_zoom);
		float y = boxMin.y + pad;
		for (const auto& [text, dim] : lines) {
			draw->AddText(font, labelFont, ImVec2(boxMin.x + pad, y), dim ? DimTextColour : TextColour, text.c_str());
			y += labelLine;
		}
		if (hovered && hoveredNode < 0 && ImGui::IsMouseHoveringRect(boxMin, boxMax)) hoveredEdge = e;
	}

	// -- Nodes -- //
	for (int i = 0; i < int(m_graph.nodes.size()); ++i)
	{
		const auto& node = m_graph.nodes[i];
		const ImVec2 a = toScreen(m_positions[i]), b(a.x + nodeW, a.y + nodeH);
		if (!visible(a, b)) continue;
		const bool isHovered = i == hoveredNode;
		const float rounding = 6 * m_zoom;
		draw->AddRectFilled(a, b, NodeColour, rounding);

		// Thumbnail: still pose, plays while hovered
		const ImVec2 thumbMin(a.x + 4 * m_zoom, a.y + 4 * m_zoom), thumbMax(b.x - 4 * m_zoom, a.y + ThumbnailHeight * m_zoom);
		if (m_zoom >= ThumbnailZoom) {
			auto& thumbnail = m_thumbnails[node.moveId];
			if (!thumbnail) thumbnail = std::make_unique<AnimationPreview>();
			AnimationPreview::Input input = callbacks.animation(node);
			input.active.clear();
			if (node.firstActiveFrame > 0 && node.lastActiveFrame >= node.firstActiveFrame) input.active.emplace_back(node.firstActiveFrame, node.lastActiveFrame);
			thumbnail->Thumbnail(input, draw, thumbMin, thumbMax, isHovered);
		}
		else draw->AddRectFilled(thumbMin, thumbMax, IM_COL32(22, 24, 30, 255));

		// Id and name under the thumbnail
		draw->PushClipRect(a, b, true);
		const float textX = a.x + 6 * m_zoom;
		float textY = a.y + (ThumbnailHeight + 4) * m_zoom;
		const auto idText = std::format("{}", node.moveId);
		draw->AddText(font, fontSize, ImVec2(textX, textY), DimTextColour, idText.c_str());
		textY += lineHeight;
		draw->AddText(font, fontSize, ImVec2(textX, textY), TextColour, node.name.c_str());
		if (node.generic) {
			// Tag over the thumbnail
			const char* tag = _("edition.cancel_graph.generic");
			const ImVec2 tagSize = font->CalcTextSizeA(fontSize * 0.85f, FLT_MAX, 0, tag);
			draw->AddRectFilled(ImVec2(thumbMin.x, thumbMin.y), ImVec2(thumbMin.x + tagSize.x + 8 * m_zoom, thumbMin.y + tagSize.y + 4 * m_zoom), GenericColour, 3 * m_zoom);
			draw->AddText(font, fontSize * 0.85f, ImVec2(thumbMin.x + 4 * m_zoom, thumbMin.y + 2 * m_zoom), IM_COL32(10, 20, 24, 255), tag);
		}
		draw->PopClipRect();

		const ImU32 kindColour = KindColour(node.kind);
		const ImU32 border = isHovered ? HoverColour : i == 0 ? RootColour : kindColour ? kindColour : node.generic ? GenericColour : node.depth < 0 ? IncomingColour : BorderColour;
		// The kind also as a strip under the thumbnail (the root keeps its gold border)
		if (kindColour) draw->AddRectFilled(ImVec2(a.x + 1, thumbMax.y), ImVec2(b.x - 1, thumbMax.y + 4 * m_zoom), kindColour);
		draw->AddRect(a, b, border, rounding, 0, (i == 0 || isHovered ? 2.5f : 1.5f) * (std::max)(m_zoom, 0.5f));
		// Cancels beyond the depth limit: a "+" on the right side
		if (node.cutOff) {
			const ImVec2 c(b.x, a.y + nodeH / 2);
			const float r = 8 * (std::max)(m_zoom, 0.5f);
			draw->AddCircleFilled(c, r, BorderColour);
			draw->AddLine(ImVec2(c.x - r * 0.5f, c.y), ImVec2(c.x + r * 0.5f, c.y), TextColour, 1.5f);
			draw->AddLine(ImVec2(c.x, c.y - r * 0.5f), ImVec2(c.x, c.y + r * 0.5f), TextColour, 1.5f);
		}
		// Cancel list outside the moveset (not followed): a "!" on the right side
		else if (node.externalCancels) {
			const ImVec2 c(b.x, a.y + nodeH / 2);
			const float r = 8 * (std::max)(m_zoom, 0.5f);
			draw->AddCircleFilled(c, r, IM_COL32(220, 140, 50, 255));
			draw->AddLine(ImVec2(c.x, c.y - r * 0.55f), ImVec2(c.x, c.y + r * 0.15f), TextColour, 1.5f);
			draw->AddCircleFilled(ImVec2(c.x, c.y + r * 0.5f), 1.2f * (std::max)(m_zoom, 0.5f), TextColour);
		}
	}
	if (m_graph.nodes.size() == 1 && m_graph.edges.empty() && !m_graph.nodes[0].externalCancels) {
		draw->AddText(ImVec2(origin.x + 12, origin.y + 10), DimTextColour, _("edition.cancel_graph.no_cancel"));
	}
	if (!m_positions.empty() && visible(ImVec2(incomingCentre.x - incomingRadius, incomingCentre.y - incomingRadius), ImVec2(incomingCentre.x + incomingRadius, incomingCentre.y + incomingRadius))) {
		// Arrow pointing left (show what leads here), or right once shown (hide them)
		draw->AddCircleFilled(incomingCentre, incomingRadius, incomingHovered ? IM_COL32(70, 150, 70, 255) : m_incoming ? IM_COL32(55, 115, 55, 255) : BorderColour);
		draw->AddCircle(incomingCentre, incomingRadius, IncomingColour, 0, 1.5f);
		const float t = incomingRadius * 0.45f, dir = m_incoming ? 1.0f : -1.0f;
		draw->AddTriangleFilled(ImVec2(incomingCentre.x + dir * t, incomingCentre.y), ImVec2(incomingCentre.x - dir * t * 0.8f, incomingCentre.y - t),
			ImVec2(incomingCentre.x - dir * t * 0.8f, incomingCentre.y + t), TextColour);
	}
	if (m_autoScroll) {
		// Autoscroll anchor: a disc with the four directions
		const ImVec2 c = m_autoScrollOrigin;
		draw->AddCircleFilled(c, 14, IM_COL32(30, 33, 42, 220));
		draw->AddCircle(c, 14, TextColour, 0, 1.5f);
		draw->AddCircleFilled(c, 2, TextColour);
		for (int i = 0; i < 4; ++i) {
			const ImVec2 d = i == 0 ? ImVec2(0, -1) : i == 1 ? ImVec2(0, 1) : i == 2 ? ImVec2(-1, 0) : ImVec2(1, 0), s(-d.y, d.x);
			const ImVec2 tip(c.x + d.x * 11, c.y + d.y * 11);
			draw->AddTriangleFilled(tip, ImVec2(tip.x - d.x * 5 + s.x * 4, tip.y - d.y * 5 + s.y * 4), ImVec2(tip.x - d.x * 5 - s.x * 4, tip.y - d.y * 5 - s.y * 4), TextColour);
		}
	}
	draw->PopClipRect();

	// Tooltips
	if (incomingHovered) {
		ImGui::BeginTooltip();
		if (m_incoming) {
			int count = 0;
			for (const auto& node : m_graph.nodes) count += node.depth < 0;
			ImGui::TextUnformatted(_("edition.cancel_graph.incoming_hide"));
			ImGui::TextDisabled("%d %s", count, _("references.moves"));
		}
		else ImGui::TextUnformatted(_("edition.cancel_graph.incoming_help"));
		ImGui::EndTooltip();
	}
	else if (hoveredNode >= 0) {
		const auto& node = m_graph.nodes[hoveredNode];
		ImGui::BeginTooltip();
		ImGui::Text("%u - %s", node.moveId, node.name.c_str());
		if (node.generic) ImGui::TextColored(ImVec4(0.3f, 0.75f, 0.8f, 1), "%s", _("edition.cancel_graph.generic_help"));
		if (node.cutOff) ImGui::TextDisabled("%s", _("edition.cancel_graph.cut_off"));
		if (node.externalCancels) ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.3f, 1), "%s", _("edition.cancel_graph.external"));
		ImGui::TextDisabled("%s", _("edition.cancel_graph.click_to_open"));
		ImGui::EndTooltip();
	}
	else if (hoveredEdge >= 0) {
		const auto& edge = m_graph.edges[hoveredEdge];
		ImGui::BeginTooltip();
		ImGui::Text("%s  ->  %s", m_graph.nodes[edge.from].name.c_str(), m_graph.nodes[edge.to].name.c_str());
		ImGui::Separator();
		for (const auto& cancel : edge.cancels) {
			ImGui::Text("%s", CancelLine(cancel).c_str());
			ImGui::SameLine();
			ImGui::TextDisabled("(%s %u)", _(cancel.grouped ? "edition.grouped_cancel.window_name" : "references.cancel.title"), cancel.cancelId);
			for (const auto& requirement : cancel.requirements) ImGui::TextDisabled("    %s", RequirementLine(baseWindow, requirement).c_str());
		}
		ImGui::EndTooltip();
	}
}
