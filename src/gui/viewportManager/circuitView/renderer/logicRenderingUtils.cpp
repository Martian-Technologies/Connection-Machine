#include "logicRenderingUtils.h"

constexpr float edgeDistance = 0.48f;
constexpr float sideShift = 0.25f;

FVector getOutputOffset(BlockType blockType) {
	FVector offset = { 0.5, 0.5 };
	if (blockType == BlockType::JUNCTION) return offset;

	return offset + FVector(edgeDistance, sideShift);
}

FVector getInputOffset(BlockType blockType) {
	FVector offset = { 0.5, 0.5 };
	if (blockType == BlockType::JUNCTION) return offset;

	return offset + FVector(-edgeDistance, -sideShift);
}

FVector getOutputOffset(BlockType blockType, Orientation orientation) {
	FVector offset = { 0.5, 0.5 };
	if (blockType == BlockType::JUNCTION) return offset;

	return offset + orientation * FVector(edgeDistance, sideShift);
}

FVector getInputOffset(BlockType blockType, Orientation orientation) {
	FVector offset = { 0.5, 0.5 };
	if (blockType == BlockType::JUNCTION) return offset;

	return offset + orientation * FVector(-edgeDistance, -sideShift);
}
