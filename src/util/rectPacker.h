#ifndef rectPacker_h
#define rectPacker_h

#include "stb_rect_pack.h"

#include "vec2.h"

class RectPacker {
public:
    typedef unsigned int RectID; // 0 = invalid

    RectPacker(Vec2Int size);

    RectID tryAddRect(Vec2Int size);
    bool removeRect(RectID id);

    std::pair<Vec2Int, Vec2Int> getRect(RectID id) const;
    std::vector<RectID> getAllRectIds() const;

    Vec2Int getSize() const { return m_size; }
    size_t getRectCount() const { return m_rects.size(); }

private:
    Vec2Int m_size;
    std::unique_ptr<stbrp_context> m_context{};
    std::vector<stbrp_node> m_nodes;
    RectID m_nextID;

    // Stored as {position, size}
    std::unordered_map<RectID, std::pair<Vec2Int, Vec2Int>> m_rects;
    std::vector<std::pair<Vec2Int, Vec2Int>> m_freeRects;
};

#endif /* rectPacker_h */
