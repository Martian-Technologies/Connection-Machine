#include <gtest/gtest.h>
#include "environment/environment.h"
#include "backend/evaluator/simulator/evalLogicSimulator.h"
#include "loggingTestSetup.h"

class SwitchAndLightSimulatorTest : public ::testing::Test {
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
};

void SwitchAndLightSimulatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
	simulator_id_t simulatorId = environment.getBackend().createSimulator(circuitId).value();
	simulator = environment.getBackend().getSimulator(simulatorId);
	ASSERT_TRUE(simulator->isPause());
}

void SwitchAndLightSimulatorTest::TearDown() {
	circuit = nullptr;
	simulator = nullptr;
}

TEST_F(SwitchAndLightSimulatorTest, SingleSwitch) {
	Position switchPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));
	EXPECT_EQ(simulator->getState(switchPos), L);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(switchPos), H);
	simulator->setState(switchPos, L);
	EXPECT_EQ(simulator->getState(switchPos), L);
}

TEST_F(SwitchAndLightSimulatorTest, InteractFail) {
	Position nothingPos(10, 10);
	EXPECT_EQ(simulator->getState(nothingPos), X);
	logging_test::setExpectedLogCounts(1, 0);
	simulator->setState(nothingPos, H); // should log an error
	EXPECT_EQ(simulator->getState(nothingPos), X);
}

TEST_F(SwitchAndLightSimulatorTest, SwitchAndLight) {
	Position switchPos(0, 0);
	Position lightPos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	// switch and light not connected yet
	EXPECT_EQ(simulator->getState(switchPos), L);
	EXPECT_EQ(simulator->getState(lightPos), Z);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(switchPos), H);
	EXPECT_EQ(simulator->getState(lightPos), Z);
	simulator->setState(switchPos, L);
	EXPECT_EQ(simulator->getState(switchPos), L);
	EXPECT_EQ(simulator->getState(lightPos), Z);

	// connect switch to light
	ASSERT_TRUE(circuit->tryCreateConnection(switchPos, lightPos));

	EXPECT_EQ(simulator->getState(switchPos), L);
	EXPECT_EQ(simulator->getState(lightPos), L);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(switchPos), H);
	EXPECT_EQ(simulator->getState(lightPos), H);
	simulator->setState(switchPos, L);
	EXPECT_EQ(simulator->getState(switchPos), L);
	EXPECT_EQ(simulator->getState(lightPos), L);

	// disconnect switch from light
	ASSERT_TRUE(circuit->tryRemoveConnection(switchPos, lightPos));
	EXPECT_EQ(simulator->getState(switchPos), L);
	EXPECT_EQ(simulator->getState(lightPos), Z);
	simulator->setState(switchPos, H);
	EXPECT_EQ(simulator->getState(switchPos), H);
	EXPECT_EQ(simulator->getState(lightPos), Z);
	simulator->setState(switchPos, L);
	EXPECT_EQ(simulator->getState(switchPos), L);
	EXPECT_EQ(simulator->getState(lightPos), Z);
}

TEST_F(SwitchAndLightSimulatorTest, TwoSwitchesAndLight) {
	Position switch1Pos(0, 0);
	Position switch2Pos(1, 0);
	Position lightPos(2, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switch1Pos, 0, BlockType::SWITCH));
	ASSERT_TRUE(circuit->tryInsertBlock(switch2Pos, 0, BlockType::SWITCH));
	ASSERT_TRUE(circuit->tryInsertBlock(lightPos, 0, BlockType::LIGHT));

	EXPECT_EQ(simulator->getState(switch1Pos), L);
	EXPECT_EQ(simulator->getState(switch2Pos), L);
	EXPECT_EQ(simulator->getState(lightPos), Z);

	// connect switches to light
	ASSERT_TRUE(circuit->tryCreateConnection(switch1Pos, lightPos));
	ASSERT_TRUE(circuit->tryCreateConnection(switch2Pos, lightPos));

	EXPECT_EQ(simulator->getState(switch1Pos), L);
	EXPECT_EQ(simulator->getState(switch2Pos), L);
	EXPECT_EQ(simulator->getState(lightPos), L);

	simulator->setState(switch1Pos, H);
	EXPECT_EQ(simulator->getState(switch1Pos), H);
	EXPECT_EQ(simulator->getState(switch2Pos), L);
	EXPECT_EQ(simulator->getState(lightPos), X); // contention

	simulator->setState(switch2Pos, H);
	EXPECT_EQ(simulator->getState(switch1Pos), H);
	EXPECT_EQ(simulator->getState(switch2Pos), H);
	EXPECT_EQ(simulator->getState(lightPos), H);

	simulator->setState(switch1Pos, L);
	EXPECT_EQ(simulator->getState(switch1Pos), L);
	EXPECT_EQ(simulator->getState(switch2Pos), H);
	EXPECT_EQ(simulator->getState(lightPos), X); // contention

	simulator->setState(switch2Pos, L);
	EXPECT_EQ(simulator->getState(switch1Pos), L);
	EXPECT_EQ(simulator->getState(switch2Pos), L);
	EXPECT_EQ(simulator->getState(lightPos), L);
}
