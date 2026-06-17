#ifndef textParser_h
#define textParser_h

#include "backend/position/position.h"
#include "backend/container/block/blockDefs.h"
#include "backend/evaluator/simulator/logicState.h"

BlockType stringToBlockType(const std::string& str);
Orientation stringToOrientation(const std::string& str);
std::optional<logic_state_t> stringToLogicState(const std::string& str);
std::string blockTypeToString(BlockType type);
std::string orientationToString(Orientation orientation);

#endif