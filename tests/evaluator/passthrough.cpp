#include <gtest/gtest.h>
#include "environment/environment.h"
#include "backend/evaluator/evaluator.h"
#include "computerAPI/directoryManager.h"

class PassthroughEvaluatorTest : public ::testing::Test {
protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment {false};
	SharedEvaluator evaluator = nullptr;
	SharedCircuit circuit = nullptr;
	logic_state_t L = logic_state_t::LOW;
	logic_state_t H = logic_state_t::HIGH;
	logic_state_t Z = logic_state_t::FLOATING;
	logic_state_t X = logic_state_t::UNDEFINED;
	SharedCircuit passthroughCircuit = nullptr;
	BlockType PT;
};

void PassthroughEvaluatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	circuit = environment.getBackend().getCircuit(circuitId);
	evaluator_id_t evalId = environment.getBackend().createEvaluator(circuitId).value();
	evaluator = environment.getBackend().getEvaluator(evalId);
	ASSERT_TRUE(evaluator->isPause());

	CircuitFileManager& circuitFileManager = environment.getCircuitFileManager();
	circuit_id_t passthroughCircuitId = circuitFileManager.loadFromFile((DirectoryManager::getResourceDirectory() / "circuits" / "evaluator" / "passthrough.cir").string()).at(0);
	passthroughCircuit = environment.getBackend().getCircuitManager().getCircuit(passthroughCircuitId);
	PT = passthroughCircuit->getBlockType();
}

void PassthroughEvaluatorTest::TearDown() {
	circuit.reset();
	evaluator.reset();
}

TEST_F(PassthroughEvaluatorTest, PlacePassthrough) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, PT));
	EXPECT_EQ(evaluator->getState(blockPos), L);
}

TEST_F(PassthroughEvaluatorTest, PassthroughLogic) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, PT));
	Position switchPos(-1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));
	Position lightPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	EXPECT_EQ(evaluator->getState(lightPos), Z);
	evaluator->setState(switchPos, H);
	EXPECT_EQ(evaluator->getState(lightPos), Z);
	evaluator->setState(switchPos, L);
	EXPECT_EQ(evaluator->getState(lightPos), Z);

	// connect
	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, blockPos));
	ASSERT_TRUE(circuit->tryCreateConnection(blockPos, lightPos));

	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switchPos, H);
	EXPECT_EQ(evaluator->getState(lightPos), H);
	evaluator->setState(switchPos, L);
	EXPECT_EQ(evaluator->getState(lightPos), L);
}

TEST_F(PassthroughEvaluatorTest, PassthroughDelete) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, PT));
	Position switchPos(-1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));
	Position lightPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	// connect
	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, blockPos));
	ASSERT_TRUE(circuit->tryCreateConnection(blockPos, lightPos));

	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switchPos, H);
	EXPECT_EQ(evaluator->getState(lightPos), H);

	// delete passthrough
	ASSERT_TRUE(circuit->tryRemoveBlock(blockPos));

	// states should no longer propagate
	EXPECT_EQ(evaluator->getState(lightPos), Z);
	evaluator->setState(switchPos, L);
	EXPECT_EQ(evaluator->getState(lightPos), Z);
}

TEST_F(PassthroughEvaluatorTest, PassthroughDisconnectSwitch) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, PT));
	Position switchPos(-1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));
	Position lightPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	// connect
	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, blockPos));
	ASSERT_TRUE(circuit->tryCreateConnection(blockPos, lightPos));

	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switchPos, H);
	EXPECT_EQ(evaluator->getState(lightPos), H);

	// disconnect switch
	ASSERT_TRUE(circuit->tryRemoveConnection(switchPos, blockPos));

	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switchPos, L);
	EXPECT_EQ(evaluator->getState(lightPos), L);
}

TEST_F(PassthroughEvaluatorTest, PassthroughDisconnectLight) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, PT));
	Position switchPos(-1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));
	Position lightPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	// connect
	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, blockPos));
	ASSERT_TRUE(circuit->tryCreateConnection(blockPos, lightPos));

	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switchPos, H);
	EXPECT_EQ(evaluator->getState(lightPos), H);

	// disconnect light
	ASSERT_TRUE(circuit->tryRemoveConnection(blockPos, lightPos));

	EXPECT_EQ(evaluator->getState(lightPos), Z);
	evaluator->setState(switchPos, L);
	EXPECT_EQ(evaluator->getState(lightPos), Z);
}

TEST_F(PassthroughEvaluatorTest, DoublePassthrough) {
	Position block1Pos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(block1Pos, 0, PT));
	Position block2Pos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(block2Pos, 0, PT));
	Position switchPos(-1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));
	Position lightPos(2, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	// connect
	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, block1Pos));
	ASSERT_TRUE(circuit->tryCreateConnection(block1Pos, block2Pos));
	ASSERT_TRUE(circuit->tryCreateConnection(block2Pos, lightPos));

	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switchPos, H);
	EXPECT_EQ(evaluator->getState(lightPos), H);
	evaluator->setState(switchPos, L);
	EXPECT_EQ(evaluator->getState(lightPos), L);
}

TEST_F(PassthroughEvaluatorTest, PassthroughLoop) {
	Position block1Pos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(block1Pos, 0, PT));
	Position block2Pos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(block2Pos, 0, PT));
	Position switchPos(-1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));
	Position lightPos(2, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	// connect
	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, block1Pos));
	ASSERT_TRUE(circuit->tryCreateConnection(block1Pos, block2Pos));
	ASSERT_TRUE(circuit->tryCreateConnection(block2Pos, block1Pos));
	ASSERT_TRUE(circuit->tryCreateConnection(block2Pos, lightPos));

	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switchPos, H);
	EXPECT_EQ(evaluator->getState(lightPos), H);
	evaluator->setState(switchPos, L);
	EXPECT_EQ(evaluator->getState(lightPos), L);
}

TEST_F(PassthroughEvaluatorTest, DeleteSwitch) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, PT));
	Position switchPos(-1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));
	Position lightPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	// connect
	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, blockPos));
	ASSERT_TRUE(circuit->tryCreateConnection(blockPos, lightPos));

	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switchPos, H);
	EXPECT_EQ(evaluator->getState(lightPos), H);

	// delete switch
	ASSERT_TRUE(circuit->tryRemoveBlock(switchPos));

	// states should no longer propagate
	EXPECT_EQ(evaluator->getState(lightPos), L); // pulls state from switch inside passthrough
	evaluator->setState(switchPos, L); // does nothing since switch is deleted
	EXPECT_EQ(evaluator->getState(lightPos), L);
}

TEST_F(PassthroughEvaluatorTest, DeleteLight) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, PT));
	Position switchPos(-1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));
	Position lightPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	// connect
	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, blockPos));
	ASSERT_TRUE(circuit->tryCreateConnection(blockPos, lightPos));

	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switchPos, H);
	EXPECT_EQ(evaluator->getState(lightPos), H);

	// delete light
	ASSERT_TRUE(circuit->tryRemoveBlock(lightPos));

	// states should no longer propagate
	EXPECT_EQ(evaluator->getState(lightPos), X);
	evaluator->setState(switchPos, L);
	EXPECT_EQ(evaluator->getState(lightPos), X);
}

TEST_F(PassthroughEvaluatorTest, MultipleSwitches) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, PT));
	Position switch1Pos(-1, -1);
	ASSERT_TRUE(circuit->tryInsertBlock(switch1Pos, 0, BlockType::SWITCH));
	Position switch2Pos(-1, 1);
	ASSERT_TRUE(circuit->tryInsertBlock(switch2Pos, 0, BlockType::SWITCH));
	Position lightPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	// connect
	ASSERT_TRUE(circuit->tryCreateConnection(switch1Pos, blockPos));
	ASSERT_TRUE(circuit->tryCreateConnection(switch2Pos, blockPos));
	ASSERT_TRUE(circuit->tryCreateConnection(blockPos, lightPos));

	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switch1Pos, H);
	EXPECT_EQ(evaluator->getState(lightPos), X); // contention
	evaluator->setState(switch1Pos, L);
	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switch2Pos, H);
	EXPECT_EQ(evaluator->getState(lightPos), X); // contention
	evaluator->setState(switch1Pos, H);
	EXPECT_EQ(evaluator->getState(lightPos), H);
	evaluator->setState(switch2Pos, L);
	EXPECT_EQ(evaluator->getState(lightPos), X); // contention
	evaluator->setState(switch1Pos, L);
	EXPECT_EQ(evaluator->getState(lightPos), L);
}

TEST_F(PassthroughEvaluatorTest, MultipleSwitchesDeleteOne) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, PT));
	Position switch1Pos(-1, -1);
	ASSERT_TRUE(circuit->tryInsertBlock(switch1Pos, 0, BlockType::SWITCH));
	Position switch2Pos(-1, 1);
	ASSERT_TRUE(circuit->tryInsertBlock(switch2Pos, 0, BlockType::SWITCH));
	Position lightPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	// connect
	ASSERT_TRUE(circuit->tryCreateConnection(switch1Pos, blockPos));
	ASSERT_TRUE(circuit->tryCreateConnection(switch2Pos, blockPos));
	ASSERT_TRUE(circuit->tryCreateConnection(blockPos, lightPos));

	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switch1Pos, H);
	EXPECT_EQ(evaluator->getState(lightPos), X); // contention

	// delete one switch
	ASSERT_TRUE(circuit->tryRemoveBlock(switch2Pos));
	EXPECT_EQ(evaluator->getState(lightPos), H);
	evaluator->setState(switch1Pos, L);
	EXPECT_EQ(evaluator->getState(lightPos), L);
}

TEST_F(PassthroughEvaluatorTest, PassthroughMoveSwitch) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, PT));
	Position switchPos(-1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));
	Position lightPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	// connect
	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, blockPos));
	ASSERT_TRUE(circuit->tryCreateConnection(blockPos, lightPos));

	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switchPos, H);
	EXPECT_EQ(evaluator->getState(lightPos), H);

	// move switch
	ASSERT_TRUE(passthroughCircuit->tryMoveBlock(Position(-2, -2), Position(-2, -3), Orientation()));
	EXPECT_EQ(evaluator->getState(lightPos), L); // pulls state from switch inside passthrough
	evaluator->setState(switchPos, L); // does nothing since switch is moved
	EXPECT_EQ(evaluator->getState(lightPos), L);

	// move back
	ASSERT_TRUE(passthroughCircuit->tryMoveBlock(Position(-2, -3), Position(-2, -2), Orientation()));
	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switchPos, H);
	EXPECT_EQ(evaluator->getState(lightPos), H);
}

TEST_F(PassthroughEvaluatorTest, PassthroughMoveLight) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, PT));
	Position switchPos(-1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));
	Position lightPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	// connect
	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, blockPos));
	ASSERT_TRUE(circuit->tryCreateConnection(blockPos, lightPos));

	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switchPos, H);
	EXPECT_EQ(evaluator->getState(lightPos), H);

	// move light
	ASSERT_TRUE(passthroughCircuit->tryMoveBlock(Position(2, -2), Position(2, -3), Orientation()));
	EXPECT_EQ(evaluator->getState(lightPos), Z); // light is moved, so no input
	evaluator->setState(switchPos, L);
	EXPECT_EQ(evaluator->getState(lightPos), Z);

	// move back
	ASSERT_TRUE(passthroughCircuit->tryMoveBlock(Position(2, -3), Position(2, -2), Orientation()));
	EXPECT_EQ(evaluator->getState(lightPos), L);
	evaluator->setState(switchPos, H);
	EXPECT_EQ(evaluator->getState(lightPos), H);
}
