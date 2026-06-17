#include <gtest/gtest.h>
#include "environment/environment.h"
#include "backend/evaluator/simulator/evalLogicSimulator.h"
#include "computerAPI/directoryManager.h"
#include "loggingTestSetup.h"

class PassthroughEvaluatorTest : public ::testing::Test {
protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment {false};
	EvalLogicSimulator* simulator = nullptr;
	Circuit* circuit = nullptr;
	logic_state_t L = logic_state_t::LOW;
	logic_state_t H = logic_state_t::HIGH;
	logic_state_t Z = logic_state_t::FLOATING;
	logic_state_t X = logic_state_t::UNDEFINED;
	Circuit* passthroughCircuit = nullptr;
	BlockType PT;
};

void PassthroughEvaluatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
	simulator_id_t simulatorId = environment.getBackend().createSimulator(circuitId).value();
	simulator = environment.getBackend().getSimulator(simulatorId);
	ASSERT_TRUE(simulator->isPause());

	CircuitFileManager& circuitFileManager = environment.getCircuitFileManager();
	circuit_id_t passthroughCircuitId = circuitFileManager.loadFromFile((DirectoryManager::getResourceDirectory() / "circuits" / "evaluator" / "passthrough.cir").string()).at(0);
	passthroughCircuit = environment.getBackend().getCircuitManager().getCircuit(passthroughCircuitId);
	PT = passthroughCircuit->getBlockType();
}

void PassthroughEvaluatorTest::TearDown() {
	circuit = nullptr;
	simulator = nullptr;
}

TEST_F(PassthroughEvaluatorTest, PlacePassthrough) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, PT));
	EXPECT_EQ(simulator->getState(blockPos), L);
}

TEST_F(PassthroughEvaluatorTest, PassthroughLogic) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, PT));
	Position switchPos(-1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));
	Position lightPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	EXPECT_EQ(simulator->getState(lightPos), Z);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(lightPos), Z);
	simulator->setState(switchPos, L);
	EXPECT_EQ(simulator->getState(lightPos), Z);

	// connect
	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, blockPos));
	ASSERT_TRUE(circuit->tryCreateConnection(blockPos, lightPos));

	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(lightPos), H);
	simulator->setState(switchPos, L);
	EXPECT_EQ(simulator->getState(lightPos), L);
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

	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(lightPos), H);

	// delete passthrough
	ASSERT_TRUE(circuit->tryRemoveBlock(blockPos));

	// states should no longer propagate
	EXPECT_EQ(simulator->getState(lightPos), Z);
	simulator->setState(switchPos, L);
	EXPECT_EQ(simulator->getState(lightPos), Z);
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

	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(lightPos), H);

	// disconnect switch
	ASSERT_TRUE(circuit->tryRemoveConnection(switchPos, blockPos));

	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switchPos, L);
	EXPECT_EQ(simulator->getState(lightPos), L);
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

	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(lightPos), H);

	// disconnect light
	ASSERT_TRUE(circuit->tryRemoveConnection(blockPos, lightPos));

	EXPECT_EQ(simulator->getState(lightPos), Z);
	simulator->setState(switchPos, L);
	EXPECT_EQ(simulator->getState(lightPos), Z);
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

	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(lightPos), H);
	simulator->setState(switchPos, L);
	EXPECT_EQ(simulator->getState(lightPos), L);
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

	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(lightPos), H);
	simulator->setState(switchPos, L);
	EXPECT_EQ(simulator->getState(lightPos), L);
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

	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(lightPos), H);

	// delete switch
	ASSERT_TRUE(circuit->tryRemoveBlock(switchPos));

	// states should no longer propagate
	EXPECT_EQ(simulator->getState(lightPos), L); // pulls state from switch inside passthrough

	logging_test::setExpectedLogCounts(1, 0);
	simulator->setState(switchPos, L); // does nothing since switch is deleted

	EXPECT_EQ(simulator->getState(lightPos), L);
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

	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(lightPos), H);

	// delete light
	ASSERT_TRUE(circuit->tryRemoveBlock(lightPos));

	// states should no longer propagate
	EXPECT_EQ(simulator->getState(lightPos), X);
	simulator->setState(switchPos, L);
	EXPECT_EQ(simulator->getState(lightPos), X);
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

	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switch1Pos, H);
	EXPECT_EQ(simulator->getState(lightPos), X); // contention
	simulator->setState(switch1Pos, L);
	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switch2Pos, H);
	EXPECT_EQ(simulator->getState(lightPos), X); // contention
	simulator->setState(switch1Pos, H);
	EXPECT_EQ(simulator->getState(lightPos), H);
	simulator->setState(switch2Pos, L);
	EXPECT_EQ(simulator->getState(lightPos), X); // contention
	simulator->setState(switch1Pos, L);
	EXPECT_EQ(simulator->getState(lightPos), L);
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

	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switch1Pos, H);
	EXPECT_EQ(simulator->getState(lightPos), X); // contention

	// delete one switch
	ASSERT_TRUE(circuit->tryRemoveBlock(switch2Pos));
	EXPECT_EQ(simulator->getState(lightPos), H);
	simulator->setState(switch1Pos, L);
	EXPECT_EQ(simulator->getState(lightPos), L);
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

	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(lightPos), H);

	// move switch
	ASSERT_TRUE(passthroughCircuit->tryMoveBlock(Position(-2, -2), Position(-2, -3), Orientation()));
	EXPECT_EQ(simulator->getState(lightPos), L); // pulls state from switch inside passthrough
	simulator->setState(switchPos, L); // does nothing since switch is moved
	EXPECT_EQ(simulator->getState(lightPos), L);

	// move back
	ASSERT_TRUE(passthroughCircuit->tryMoveBlock(Position(-2, -3), Position(-2, -2), Orientation()));
	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(lightPos), H);
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

	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(lightPos), H);

	// move light
	ASSERT_TRUE(passthroughCircuit->tryMoveBlock(Position(2, -2), Position(2, -3), Orientation()));
	EXPECT_EQ(simulator->getState(lightPos), Z); // light is moved, so no input
	simulator->setState(switchPos, L);
	EXPECT_EQ(simulator->getState(lightPos), Z);

	// move back
	ASSERT_TRUE(passthroughCircuit->tryMoveBlock(Position(2, -3), Position(2, -2), Orientation()));
	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(lightPos), H);
}
