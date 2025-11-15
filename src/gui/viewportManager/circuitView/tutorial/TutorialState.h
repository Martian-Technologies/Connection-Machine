#pragma once
#include <vector>
#include <string>
#include "backend/block/blockType.h"
#include "backend/helpers/position.h"
#include "backend/helpers/orientation.h"

struct TutorialBlockInstruction {
    BlockType type;
    Position position;
    Orientation orientation;

    TutorialBlockInstruction(BlockType t, Position p, Orientation o = Orientation())
        : type(t), position(p), orientation(o) {}
};

struct TutorialState {
    std::string name;
    std::vector<TutorialBlockInstruction> blocks;

    TutorialState(std::string n) : name(std::move(n)) {}
};
