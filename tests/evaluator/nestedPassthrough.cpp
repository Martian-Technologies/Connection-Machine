#include <gtest/gtest.h>
#include "environment/environment.h"
#include "backend/evaluator/simulator/evalLogicSimulator.h"
#include "computerAPI/directoryManager.h"

class NestedPassthroughEvaluatorTest : public ::testing::Test {
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
    Circuit* nestedPassthroughCircuit = nullptr;
    Circuit* nestedPassthroughBrokenCircuit = nullptr;
    BlockType NPT;
    BlockType NPTB;
};

void NestedPassthroughEvaluatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
	simulator_id_t simulatorId = environment.getBackend().createSimulator(circuitId).value();
	simulator = environment.getBackend().getSimulator(simulatorId);
	ASSERT_TRUE(simulator->isPause());

	CircuitFileManager& circuitFileManager = environment.getCircuitFileManager();
	circuit_id_t nestedPassthroughCircuitId = circuitFileManager.loadFromFile((DirectoryManager::getResourceDirectory() / "circuits" / "evaluator" / "passthrough.cir").string()).at(0);
	nestedPassthroughCircuit = environment.getBackend().getCircuitManager().getCircuit(nestedPassthroughCircuitId);
    NPT = nestedPassthroughCircuit->getBlockType();
    circuit_id_t nestedPassthroughBrokenCircuitId = circuitFileManager.loadFromFile((DirectoryManager::getResourceDirectory() / "circuits" / "evaluator" / "nested_passthrough_broken.cir").string()).at(0);
    nestedPassthroughBrokenCircuit = environment.getBackend().getCircuitManager().getCircuit(nestedPassthroughBrokenCircuitId);
    NPTB = nestedPassthroughBrokenCircuit->getBlockType();
}

void NestedPassthroughEvaluatorTest::TearDown() {
	circuit = nullptr;
	simulator = nullptr;
}

TEST_F(NestedPassthroughEvaluatorTest, PlaceNestedPassthrough) {
    Position blockPos(0, 0);
    ASSERT_TRUE(circuit->tryInsertBlock(blockPos, 0, NPT));
    EXPECT_EQ(simulator->getState(blockPos), L);
}
