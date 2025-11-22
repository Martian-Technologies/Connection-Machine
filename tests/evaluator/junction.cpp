#include <gtest/gtest.h>
#include "environment/environment.h"
#include "backend/evaluator/evaluator.h"
#include "computerAPI/directoryManager.h"
#include "backend/blockData/blockDataManager.h"

class JunctionEvaluatorTest : public ::testing::Test {
protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment { false };
	SharedEvaluator evaluator = nullptr;
	SharedCircuit circuit = nullptr;
	logic_state_t L = logic_state_t::LOW;
	logic_state_t H = logic_state_t::HIGH;
	logic_state_t Z = logic_state_t::FLOATING;
	logic_state_t X = logic_state_t::UNDEFINED;
	// BlockType loadCircuit(const std::filesystem::path& path);
	const BlockData* getBlockData(BlockType type);
};

// BlockType JunctionEvaluatorTest::loadCircuit(const std::filesystem::path& path) {
// 	CircuitFileManager& circuitFileManager = environment.getCircuitFileManager();
// 	circuit_id_t circuitId = circuitFileManager.loadFromFile(path.string()).at(0);
// 	SharedCircuit circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
// 	return circuit->getBlockType();
// }

const BlockData* JunctionEvaluatorTest::getBlockData(BlockType type) {
	const BlockData* blockData = environment.getBackend().getBlockDataManager().getBlockData(type);
	return blockData;
}

void JunctionEvaluatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	circuit = environment.getBackend().getCircuit(circuitId);
	evaluator_id_t evalId = environment.getBackend().createEvaluator(circuitId).value();
	evaluator = environment.getBackend().getEvaluator(evalId);
	ASSERT_TRUE(evaluator->isPause());
}

void JunctionEvaluatorTest::TearDown() {
	circuit.reset();
	evaluator.reset();
}

TEST_F(JunctionEvaluatorTest, PlaceJunction) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, BlockType::JUNCTION));
	EXPECT_EQ(evaluator->getState(blockPos), Z);
}

TEST_F(JunctionEvaluatorTest, PlacePulldown) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, BlockType::JUNCTION_L));
	EXPECT_EQ(evaluator->getState(blockPos), L);
}

TEST_F(JunctionEvaluatorTest, PlacePullup) {
	Position blockPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, BlockType::JUNCTION_H));
	EXPECT_EQ(evaluator->getState(blockPos), H);
}

TEST_F(JunctionEvaluatorTest, PlacePullupAndPulldown) {
	Position pullUpPos(0, 0);
	Position pullDownPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(pullUpPos, 0, BlockType::JUNCTION_H));
	ASSERT_TRUE(circuit->tryInsertBlock(pullDownPos, 0, BlockType::JUNCTION_L));
	EXPECT_EQ(evaluator->getState(pullUpPos), H);
	EXPECT_EQ(evaluator->getState(pullDownPos), L);

	Position pullUpPortPos = pullUpPos + *(getBlockData(BlockType::JUNCTION_H)->getConnectionVector(0));
	Position pullDownPortPos = pullDownPos + *(getBlockData(BlockType::JUNCTION_L)->getConnectionVector(0));
	ASSERT_TRUE(circuit->tryCreateConnection(pullUpPortPos, pullDownPortPos));
	EXPECT_EQ(evaluator->getState(pullUpPos), X);
	EXPECT_EQ(evaluator->getState(pullDownPos), X);
}

TEST_F(JunctionEvaluatorTest, JunctionsPropagateThroughOutputs) {
	Position switch1Pos(0, 0);
	Position switch2Pos(1, 0);
	Position junction1Pos(2, 0);
	Position junction2Pos(3, 0);

	// switch 1 -> junction 1
	// switch 2 -> junction 1
	// switch 2 -> junction 2

	// this should mean that switch 1 is wired to junction 2 through junction 1 and the output of switch 2

	ASSERT_TRUE(circuit->tryInsertBlock(switch1Pos, 0, BlockType::SWITCH));
	ASSERT_TRUE(circuit->tryInsertBlock(switch2Pos, 0, BlockType::SWITCH));
	ASSERT_TRUE(circuit->tryInsertBlock(junction1Pos, 0, BlockType::JUNCTION));
	ASSERT_TRUE(circuit->tryInsertBlock(junction2Pos, 0, BlockType::JUNCTION));

	ASSERT_TRUE(circuit->tryCreateConnection(switch1Pos, junction1Pos));
	ASSERT_TRUE(circuit->tryCreateConnection(switch2Pos, junction1Pos));
	ASSERT_TRUE(circuit->tryCreateConnection(switch2Pos, junction2Pos));

	EXPECT_EQ(evaluator->getState(junction1Pos), L);
	EXPECT_EQ(evaluator->getState(junction2Pos), L);
	evaluator->setState(switch1Pos, H);
	EXPECT_EQ(evaluator->getState(junction1Pos), X);
	EXPECT_EQ(evaluator->getState(junction2Pos), X);
	evaluator->setState(switch2Pos, Z);
	EXPECT_EQ(evaluator->getState(junction1Pos), H);
	EXPECT_EQ(evaluator->getState(junction2Pos), H);
}

TEST_F(JunctionEvaluatorTest, JunctionsMultiwireIntoGate) {
	Position switchPos(0, 0);
	Position junction1Pos(1, 0);
	Position junction2Pos(2, 0);
	Position junction3Pos(3, 0);
	Position andGatePos2(4, 0);
	Position xorGatePos2(5, 0);
	Position xnorGatePos2(6, 0);
	Position andGatePos3(7, 0);
	Position xorGatePos3(8, 0);
	Position xnorGatePos3(9, 0);

	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));
	ASSERT_TRUE(circuit->tryInsertBlock(junction1Pos, 0, BlockType::JUNCTION));
	ASSERT_TRUE(circuit->tryInsertBlock(junction2Pos, 0, BlockType::JUNCTION));
	ASSERT_TRUE(circuit->tryInsertBlock(junction3Pos, 0, BlockType::JUNCTION));
	ASSERT_TRUE(circuit->tryInsertBlock(andGatePos2, 0, BlockType::AND));
	ASSERT_TRUE(circuit->tryInsertBlock(xorGatePos2, 0, BlockType::XOR));
	ASSERT_TRUE(circuit->tryInsertBlock(xnorGatePos2, 0, BlockType::XNOR));
	ASSERT_TRUE(circuit->tryInsertBlock(andGatePos3, 0, BlockType::AND));
	ASSERT_TRUE(circuit->tryInsertBlock(xorGatePos3, 0, BlockType::XOR));
	ASSERT_TRUE(circuit->tryInsertBlock(xnorGatePos3, 0, BlockType::XNOR));

	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, junction1Pos));
	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, junction2Pos));
	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, junction3Pos));
	ASSERT_TRUE(circuit->tryCreateConnection(junction1Pos, andGatePos2));
	ASSERT_TRUE(circuit->tryCreateConnection(junction2Pos, andGatePos2));
	ASSERT_TRUE(circuit->tryCreateConnection(junction1Pos, xorGatePos2));
	ASSERT_TRUE(circuit->tryCreateConnection(junction2Pos, xorGatePos2));
	ASSERT_TRUE(circuit->tryCreateConnection(junction1Pos, xnorGatePos2));
	ASSERT_TRUE(circuit->tryCreateConnection(junction2Pos, xnorGatePos2));
	ASSERT_TRUE(circuit->tryCreateConnection(junction1Pos, andGatePos3));
	ASSERT_TRUE(circuit->tryCreateConnection(junction2Pos, andGatePos3));
	ASSERT_TRUE(circuit->tryCreateConnection(junction3Pos, andGatePos3));
	ASSERT_TRUE(circuit->tryCreateConnection(junction1Pos, xorGatePos3));
	ASSERT_TRUE(circuit->tryCreateConnection(junction2Pos, xorGatePos3));
	ASSERT_TRUE(circuit->tryCreateConnection(junction3Pos, xorGatePos3));
	ASSERT_TRUE(circuit->tryCreateConnection(junction1Pos, xnorGatePos3));
	ASSERT_TRUE(circuit->tryCreateConnection(junction2Pos, xnorGatePos3));
	ASSERT_TRUE(circuit->tryCreateConnection(junction3Pos, xnorGatePos3));

	evaluator->tickStep();

	EXPECT_EQ(evaluator->getState(andGatePos2), L);
	EXPECT_EQ(evaluator->getState(xorGatePos2), L);
	EXPECT_EQ(evaluator->getState(xnorGatePos2), H);
	EXPECT_EQ(evaluator->getState(andGatePos3), L);
	EXPECT_EQ(evaluator->getState(xorGatePos3), L);
	EXPECT_EQ(evaluator->getState(xnorGatePos3), H);

	evaluator->setState(switchPos, H);
	evaluator->tickStep();

	EXPECT_EQ(evaluator->getState(andGatePos2), H);
	EXPECT_EQ(evaluator->getState(xorGatePos2), L);
	EXPECT_EQ(evaluator->getState(xnorGatePos2), H);
	EXPECT_EQ(evaluator->getState(andGatePos3), H);
	EXPECT_EQ(evaluator->getState(xorGatePos3), H);
	EXPECT_EQ(evaluator->getState(xnorGatePos3), L);
}
