#include "rectPacker.h"

RectPacker::RectPacker(Vec2Int size) : m_size(size), m_nextID(1) {
	if (size.x <= 0 || size.y <= 0) throw std::runtime_error("RectPacker: invalid size");

	m_nodes.resize(size.x);
	m_context = std::make_unique<stbrp_context>();
	stbrp_init_target(m_context.get(), size.x, size.y, m_nodes.data(), size.x);
}

RectPacker::RectID RectPacker::tryAddRect(Vec2Int size) {
	if (size.x <= 0 || size.y <= 0) return 0;

	// Try to reuse a freed space first
	for (auto it = m_freeRects.begin(); it != m_freeRects.end(); ++it) {
		auto& [pos, sz] = *it;
		if (size.x <= sz.x && size.y <= sz.y) {
			RectID id = m_nextID++;
			m_rects[id] = { pos, size };

			// Optional: shrink remaining free space
			m_freeRects.erase(it);
			return id;
		}
	}

	// Otherwise, pack a new one
	stbrp_rect rect{};
	rect.id = 1;
	rect.w = size.x;
	rect.h = size.y;

	if (!stbrp_pack_rects(m_context.get(), &rect, 1) || !rect.was_packed) return 0;

	RectID id = m_nextID++;
	logInfo("tryAddRect found pos {} for size {}", "RectPacker", Vec2Int(rect.x, rect.y), size);
	m_rects[id] = { Vec2Int(rect.x, rect.y), size };
	return id;
}

bool RectPacker::removeRect(RectID id) {
	auto it = m_rects.find(id);
	if (it == m_rects.end()) return false;

	// Mark its area as free (optional: merge later for optimization)
	m_freeRects.push_back(it->second);
	m_rects.erase(it);
	return true;
}

std::pair<Vec2Int, Vec2Int> RectPacker::getRect(RectID id) const {
	auto it = m_rects.find(id);
	if (it == m_rects.end()) throw std::runtime_error("RectPacker: invalid RectID");
	return it->second;
}

std::vector<RectPacker::RectID> RectPacker::getAllRectIds() const {
	std::vector<RectID> ids;
	ids.reserve(m_rects.size());
	for (auto& [id, _] : m_rects) ids.push_back(id);
	return ids;
}
