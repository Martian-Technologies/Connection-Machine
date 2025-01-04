#ifndef block_h
#define block_h

#include "../position/position.h"
#include "../connection/connectionContainer.h"
#include "../defs.h"

inline void rotateWidthAndHeight(Rotation rotation, block_size_t& width, block_size_t& height) noexcept {
    if (isRotated(rotation)) std::swap(width, height);
}

constexpr block_size_t getBlockWidth(BlockType type) noexcept {
    // add if not 1
    switch (type) {
    default: return 1;
    }
}

constexpr block_size_t getBlockHeight(BlockType type) noexcept {
    // add if not 1
    switch (type) {
    default: return 1;
    }
}

constexpr block_size_t getBlockWidth(BlockType type, Rotation rotation) noexcept {
    return isRotated(rotation) ? getBlockHeight(type) : getBlockWidth(type);
}

constexpr block_size_t getBlockHeight(BlockType type, Rotation rotation) noexcept {
    return isRotated(rotation) ? getBlockWidth(type) : getBlockHeight(type);
}

inline std::pair<connection_end_id_t, bool> getInputConnectionId(BlockType type, const Position& relativePos) {
    switch (type) {
    case BlockType::SWITCH: return { 0, false };
    case BlockType::BUTTON: return { 0, false };
    case BlockType::TICK_BUTTON: return { 0, false };
    case BlockType::LIGHT: return { 0, true };
    default:
        if (relativePos.x == 0 && relativePos.y == 0) return { 0, true };
        return { 0, false };
    }
}

inline std::pair<connection_end_id_t, bool> getOutputConnectionId(BlockType type, const Position& relativePos) {
    switch (type) {
    case BlockType::SWITCH: return { 0, true };
    case BlockType::BUTTON: return { 0, true };
    case BlockType::TICK_BUTTON: return { 0, true };
    case BlockType::LIGHT: return { 0, false };
    default:
        if (relativePos.x == 0 && relativePos.y == 0) return { 1, true };
        return { 0, false };
    }
}

inline std::pair<connection_end_id_t, bool> getInputConnectionId(BlockType type, Rotation rotation, const Position& relativePos) {
    if (isRotated(rotation)) {
        return getInputConnectionId(type, Position(relativePos.y, relativePos.x));
    }
    return getInputConnectionId(type, relativePos);
}

inline std::pair<connection_end_id_t, bool> getOutputConnectionId(BlockType type, Rotation rotation, const Position& relativePos) {
    if (isRotated(rotation)) {
        return getOutputConnectionId(type, Position(relativePos.y, relativePos.x));
    }
    return getOutputConnectionId(type, relativePos);
}

inline std::pair<Position, bool> getConnectionPosition(BlockType type, connection_end_id_t connectionId) {
    switch (type) {
    case BlockType::SWITCH: if (connectionId) return { Position(), false }; return { Position(0, 0), true };
    case BlockType::BUTTON: if (connectionId) return { Position(), false }; return { Position(0, 0), true };
    case BlockType::TICK_BUTTON: if (connectionId) return { Position(), false }; return { Position(0, 0), true };
    case BlockType::LIGHT: if (connectionId) return { Position(), false }; return { Position(0, 0), true };
    default:
        if (connectionId < 2) return { Position(0, 0), true };
        return { Position(), false };
    }
}

inline std::pair<Position, bool> getConnectionPosition(BlockType type, Rotation rotation, connection_end_id_t connectionId) {
    if (isRotated(rotation)) {
        return getConnectionPosition(type, connectionId);
    }
    return getConnectionPosition(type, connectionId);
}

constexpr connection_end_id_t getMaxConnectionId(BlockType type) {
    switch (type) {
    case BlockType::SWITCH: return 0;
    case BlockType::BUTTON: return 0;
    case BlockType::TICK_BUTTON: return 0;
    case BlockType::LIGHT: return 0;
    default: return 1;
    }
}

constexpr bool isConnectionInput(BlockType type, connection_end_id_t connectionId) {
    switch (type) {
    case BlockType::SWITCH: return false;
    case BlockType::BUTTON: return false;
    case BlockType::TICK_BUTTON: return false;
    default:
        return connectionId == 0;
    }
}

class Block {
    friend class BlockContainer;
    friend Block getBlockClass(BlockType type);
public:
    inline Block() : Block(BLOCK) {}

    // getters
    block_id_t id() const { return blockId; }
    BlockType type() const { return blockType; }

    inline const Position& getPosition() const { return position; }
    inline Position getLargestPosition() const { return position + Position(width(), height()); }
    inline Rotation getRotation() const { return rotation; }

    inline block_size_t width() const { return getBlockWidth(type(), getRotation()); }
    inline block_size_t height() const { return getBlockHeight(type(), getRotation()); }
    inline block_size_t widthNoRotation() const { return getBlockWidth(type()); }
    inline block_size_t heightNoRotation() const { return getBlockHeight(type()); }

    inline bool withinBlock(const Position& position) const { return position.withinArea(getPosition(), getLargestPosition()); }

    inline const ConnectionContainer& getConnectionContainer() const { return connections; }
    inline const std::vector<ConnectionEnd>& getInputConnections(const Position& position) const {
        auto [connectionId, success] = getInputConnectionId(position);
        return success ? getConnectionContainer().getConnections(connectionId) : getEmptyVector<ConnectionEnd>();
    }
    inline const std::vector<ConnectionEnd>& getOutputConnections(const Position& position) const {
        auto [connectionId, success] = getOutputConnectionId(position);
        return success ? getConnectionContainer().getConnections(connectionId) : getEmptyVector<ConnectionEnd>();
    }
    inline std::pair<connection_end_id_t, bool> getInputConnectionId(const Position& position) const {
        return withinBlock(position) ? ::getInputConnectionId(type(), getRotation(), position - getPosition()) : std::make_pair<connection_end_id_t, bool>(0, false);
    }
    inline std::pair<connection_end_id_t, bool> getOutputConnectionId(const Position& position) const {
        return withinBlock(position) ? ::getOutputConnectionId(type(), getRotation(), position - getPosition()) : std::make_pair<connection_end_id_t, bool>(0, false);
    }
    inline std::pair<Position, bool> getConnectionPosition(connection_end_id_t connectionId) const {
        auto output = ::getConnectionPosition(type(), getRotation(), connectionId);
        output.first += getPosition();
        return output;
    }
    inline bool isConnectionInput(connection_end_id_t connectionId) const { return ::isConnectionInput(type(), connectionId); }

protected:
    inline void destroy() {}
    inline ConnectionContainer& getConnectionContainer() { return connections; }
    inline void setPosition(const Position& position) { this->position = position; }
    inline void setRotation(Rotation rotation) { this->rotation = rotation; }
    inline void setId(block_id_t id) { blockId = id; }

    inline Block(BlockType blockType) : Block(blockType, 0) {}
    inline Block(BlockType blockType, block_id_t id) : blockType(blockType), blockId(id), connections(blockType), position(), rotation() {}

    // const data
    BlockType blockType;
    block_id_t blockId;

    // helpers
    ConnectionContainer connections;

    // changing data
    Position position;
    Rotation rotation;
};

inline Block getBlockClass(BlockType type) { return Block(type); }

#endif /* block_h */
