#ifndef logicRenderingUtils_h
#define logicRenderingUtils_h

#include "backend/position/position.h"
#include "backend/container/block/blockDefs.h"

FVector getOutputOffset(BlockType blockType);
FVector getInputOffset(BlockType blockType);
FVector getOutputOffset(BlockType blockType, Orientation orientation);
FVector getInputOffset(BlockType blockType, Orientation orientation);

#endif
