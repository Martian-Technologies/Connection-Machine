#include <cassert>

#include "blockContainer.h"
#include "../../util/emptyVector.h"
#include "../block/block.h"

bool BlockContainer::checkCollision(const Position& positionSmall, const Position& positionLarge) {
    for (cord_t x = positionSmall.x; x <= positionLarge.x; x++) {
        for (cord_t y = positionSmall.y; y <= positionLarge.y; y++) {
            if (checkCollision(Position(x, y))) return true;
        }
    }
    return false;
}

bool BlockContainer::tryRemoveBlock(const Position& position) {
    Cell* cell = getCell(position);
    if (cell == nullptr) return false;
    auto iter = blocks.find(cell->getBlockId());
    Block& block = iter->second;
    removeBlockCells(block.getPosition(), block);
    // make sure to remove all connections from this block
    for (unsigned int i = 0; i <= block.getConnectionContainer().getMaxConnectionId(); i++) {
        for (auto& connectionEnd : block.getConnectionContainer().getConnections(i)) {
            getBlock(connectionEnd.getBlockId())->getConnectionContainer().tryRemoveConnection(connectionEnd.getConnectionId(), ConnectionEnd(block.id(), i));
        }
    }
    block.destroy();
    blocks.erase(iter);
    return true;
}

// makes a copy of block and places it into the grid
// returns false if the block was not inserted
bool BlockContainer::tryInsertBlock(const Position& position, Rotation rotation, const Block& block) {
    if (checkCollision(position, position + Position(getBlockWidth(block.type(), rotation) - 1, getBlockHeight(block.type(), rotation) - 1))) return false;
    block_id_t id = getNewId();
    auto iter = blocks.insert(std::make_pair(id, block)).first;
    iter->second.setId(id);
    iter->second.setPosition(position);
    iter->second.setRotation(rotation);
    placeBlockCells(iter->second);
    return true;
}

// returns false if the block was not moved
bool BlockContainer::tryMoveBlock(const Position& positionOfBlock, const Position& position, Rotation rotation) {
    Cell* cell = getCell(positionOfBlock);
    if (cell == nullptr) return false;
    block_id_t blockId = cell->getBlockId();
    Block& block = blocks[blockId];
    block.setPosition(position);
    block.setRotation(rotation);
    if (checkCollision(position, position + Position(block.width(), block.height()))) return false;
    placeBlockCells(block);
    return true;
}

bool BlockContainer::connectionExists(const Position& outputPosition, const Position& inputPosition) const {
    const Block* input = getBlock(inputPosition);
    if (!input) return false;
    auto [inputConnectionId, inputSuccess] = input->getInputConnectionId(inputPosition);
    if (!inputSuccess) return false;
    const Block* output = getBlock(outputPosition);
    if (!output) return false;
    auto [outputConnectionId, outputSuccess] = output->getOutputConnectionId(outputPosition);
    if (!outputSuccess) return false;
    return input->getConnectionContainer().hasConnection(inputConnectionId, ConnectionEnd(output->id(), outputConnectionId));
}

const std::vector<ConnectionEnd>& BlockContainer::getInputConnections(const Position& position) const {
    const Block* block = getBlock(position);
    if (block == nullptr) return getEmptyVector<ConnectionEnd>();
    return block->getInputConnections(position);
}

const std::vector<ConnectionEnd>& BlockContainer::getOutputConnections(const Position& position) const {
    const Block* block = getBlock(position);
    if (block == nullptr) return getEmptyVector<ConnectionEnd>();
    return block->getOutputConnections(position);
}

bool BlockContainer::tryCreateConnection(const Position& outputPosition, const Position& inputPosition) {
    Block* input = getBlock(inputPosition);
    if (!input) return false;
    auto [inputConnectionId, inputSuccess] = input->getInputConnectionId(inputPosition);
    if (!inputSuccess) return false;
    Block* output = getBlock(outputPosition);
    if (!output) return false;
    auto [outputConnectionId, outputSuccess] = output->getOutputConnectionId(outputPosition);
    if (!outputSuccess) return false;
    if (input->getConnectionContainer().tryMakeConnection(inputConnectionId, ConnectionEnd(output->id(), outputConnectionId))) {
        assert(output->getConnectionContainer().tryMakeConnection(outputConnectionId, ConnectionEnd(input->id(), inputConnectionId)));
        return true;
    }
    return false;
}

bool BlockContainer::tryRemoveConnection(const Position& outputPosition, const Position& inputPosition) {
    Block* input = getBlock(inputPosition);
    if (!input) return false;
    auto [inputConnectionId, inputSuccess] = input->getInputConnectionId(inputPosition);
    if (!inputSuccess) return false;
    Block* output = getBlock(outputPosition);
    if (!output) return false;
    auto [outputConnectionId, outputSuccess] = output->getOutputConnectionId(outputPosition);
    if (!outputSuccess) return false;
    if (input->getConnectionContainer().tryRemoveConnection(inputConnectionId, ConnectionEnd(output->id(), outputConnectionId))) {
        assert(output->getConnectionContainer().tryRemoveConnection(outputConnectionId, ConnectionEnd(input->id(), inputConnectionId)));
        return true;
    }
    return false;
}

void BlockContainer::placeBlockCells(const Position& position, Rotation rotation, BlockType type, block_id_t blockId) {
    for (cord_t x = 0; x < getBlockWidth(type, rotation); x++) {
        for (cord_t y = 0; y < getBlockHeight(type, rotation); y++) {
            insertCell(position + Position(x, y), Cell(blockId));
        }
    }
}

void BlockContainer::placeBlockCells(const Block& block) {
    for (cord_t x = 0; x < block.width(); x++) {
        for (cord_t y = 0; y < block.height(); y++) {
            insertCell(block.getPosition() + Position(x, y), Cell(block.id()));
        }
    }
}

void BlockContainer::removeBlockCells(const Position& position, const Block& block) {
    for (cord_t x = 0; x < block.width(); x++) {
        for (cord_t y = 0; y < block.height(); y++) {
            removeCell(position + Position(x, y));
        }
    }
}