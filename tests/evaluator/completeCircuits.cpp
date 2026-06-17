#include <gtest/gtest.h>
#include "environment/environment.h"
#include "backend/evaluator/simulator/evalLogicSimulator.h"
#include "computerAPI/directoryManager.h"
#include "backend/blockData/blockDataManager.h"

class CompleteCircuitsEvaluatorTest : public ::testing::Test {
protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment { false };
	EvalLogicSimulator* simulator = nullptr;
	Circuit* circuit = nullptr;
	logic_state_t L = logic_state_t::LOW;
	logic_state_t H = logic_state_t::HIGH;
	logic_state_t Z = logic_state_t::FLOATING;
	logic_state_t X = logic_state_t::UNDEFINED;
	BlockType loadCircuit(const std::filesystem::path& path);
	const BlockData* getBlockData(BlockType type);
};

BlockType CompleteCircuitsEvaluatorTest::loadCircuit(const std::filesystem::path& path) {
	CircuitFileManager& circuitFileManager = environment.getCircuitFileManager();
	circuit_id_t circuitId = circuitFileManager.loadFromFile(path.string()).at(0);
	Circuit* circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
	return circuit->getBlockType();
}

const BlockData* CompleteCircuitsEvaluatorTest::getBlockData(BlockType type) {
	const BlockData* blockData = environment.getBackend().getBlockDataManager().getBlockData(type);
	return blockData;
}

void CompleteCircuitsEvaluatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
	simulator_id_t simulatorId = environment.getBackend().createSimulator(circuitId).value();
	simulator = environment.getBackend().getSimulator(simulatorId);
	ASSERT_TRUE(simulator->isPause());
}

void CompleteCircuitsEvaluatorTest::TearDown() {
	circuit = nullptr;
	simulator = nullptr;
}

TEST_F(CompleteCircuitsEvaluatorTest, FullAdder) {
	BlockType FA = loadCircuit(DirectoryManager::getResourceDirectory() / "circuits" / "evaluator" / "full_adder.cir");
	Position aPos(0, 0);
	Position bPos(0, 1);
	Position cinPos(0, 2);
	Position sumPos(1, 0);
	Position coutPos(1, 1);
	ASSERT_TRUE(circuit->tryInsertBlock(aPos, 0, FA));

	Position switch1Pos(-1, 0);
	Position switch2Pos(-1, 1);
	Position switch3Pos(-1, 2);

	Position light1Pos(2, 0);
	Position light2Pos(2, 1);

	ASSERT_TRUE(circuit->tryInsertBlock(switch1Pos, 0, BlockType::SWITCH));
	ASSERT_TRUE(circuit->tryInsertBlock(switch2Pos, 0, BlockType::SWITCH));
	ASSERT_TRUE(circuit->tryInsertBlock(switch3Pos, 0, BlockType::SWITCH));
	ASSERT_TRUE(circuit->tryInsertBlock(light1Pos, 0, BlockType::LIGHT));
	ASSERT_TRUE(circuit->tryInsertBlock(light2Pos, 0, BlockType::LIGHT));

	ASSERT_TRUE(circuit->tryCreateConnection(switch1Pos, aPos));
	ASSERT_TRUE(circuit->tryCreateConnection(switch2Pos, bPos));
	ASSERT_TRUE(circuit->tryCreateConnection(switch3Pos, cinPos));
	ASSERT_TRUE(circuit->tryCreateConnection(sumPos, light1Pos));
	ASSERT_TRUE(circuit->tryCreateConnection(coutPos, light2Pos));

	simulator->tickStep(3);
	EXPECT_EQ(simulator->getState(light1Pos), L);
	EXPECT_EQ(simulator->getState(light2Pos), L);
	simulator->setState(switch1Pos, H);
	simulator->tickStep(3);
	EXPECT_EQ(simulator->getState(light1Pos), H);
	EXPECT_EQ(simulator->getState(light2Pos), L);
	simulator->setState(switch2Pos, H);
	simulator->tickStep(3);
	EXPECT_EQ(simulator->getState(light1Pos), L);
	EXPECT_EQ(simulator->getState(light2Pos), H);
	simulator->setState(switch1Pos, L);
	simulator->tickStep(3);
	EXPECT_EQ(simulator->getState(light1Pos), H);
	EXPECT_EQ(simulator->getState(light2Pos), L);
	simulator->setState(switch3Pos, H);
	simulator->tickStep(3);
	EXPECT_EQ(simulator->getState(light1Pos), L);
	EXPECT_EQ(simulator->getState(light2Pos), H);
	simulator->setState(switch2Pos, L);
	simulator->tickStep(3);
	EXPECT_EQ(simulator->getState(light1Pos), H);
	EXPECT_EQ(simulator->getState(light2Pos), L);
	simulator->setState(switch1Pos, H);
	simulator->tickStep(3);
	EXPECT_EQ(simulator->getState(light1Pos), L);
	EXPECT_EQ(simulator->getState(light2Pos), H);
	simulator->setState(switch2Pos, H);
	simulator->tickStep(3);
	EXPECT_EQ(simulator->getState(light1Pos), H);
	EXPECT_EQ(simulator->getState(light2Pos), H);
}
