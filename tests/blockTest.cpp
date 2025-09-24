#include "blockTest.h"

#include "backend/container/block/block.h"

void BlockTest::SetUp() {
	blockDataManager.emplace(&dataUpdateEventManager);
	blockDataManager->initializeDefaults();
}

void BlockTest::TearDown() {
	blockDataManager.reset();
}

TEST_F(BlockTest, constructor) {
	Block noneBlock(&(blockDataManager.value()));
	ASSERT_EQ(noneBlock.type(), BlockType::NONE);
	for (int blockTypeI = (int)BlockType::NONE; blockTypeI <= blockDataManager->maxBlockId() + 5; ++blockTypeI) {
		BlockType blockType = (BlockType)blockTypeI;
		if (blockDataManager->blockExists(blockType)) {
			Block block = getBlockClass(&(blockDataManager.value()), blockType);
			ASSERT_EQ(block.type(), blockType);
			ASSERT_EQ(block.size(), blockDataManager->getBlockSize(blockType));
		} else {
			ASSERT_FALSE(blockDataManager->getBlockData(blockType));
		}
	}
}

TEST_F(BlockTest, copyConstructor) {
	Block block(&(blockDataManager.value()));
	Block copyBlock(block);
	ASSERT_EQ(block.type(), copyBlock.type());
	ASSERT_EQ(block.size(), copyBlock.size());
	ASSERT_NE(&block, &copyBlock);
}
