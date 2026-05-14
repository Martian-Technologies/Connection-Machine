#include <gtest/gtest.h>
#include "environment/environment.h"
#include "backend/evaluator/simulator/evalLogicSimulator.h"

class ReplaysEvaluatorTest : public ::testing::Test {
protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment {false};
	EvalLogicSimulator* simulator = nullptr;
	SharedCircuit circuit = nullptr;
	logic_state_t L = logic_state_t::LOW;
	logic_state_t H = logic_state_t::HIGH;
	logic_state_t Z = logic_state_t::FLOATING;
	logic_state_t X = logic_state_t::UNDEFINED;
};

void ReplaysEvaluatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	circuit = environment.getBackend().getCircuit(circuitId);
	simulator_id_t simulatorId = environment.getBackend().createSimulator(circuitId).value();
	simulator = environment.getBackend().getSimulator(simulatorId);
	ASSERT_TRUE(simulator->isPause());
	ASSERT_FALSE(simulator->isViewingReplay());
}

void ReplaysEvaluatorTest::TearDown() {
	circuit.reset();
	simulator = nullptr;
}

TEST_F(ReplaysEvaluatorTest, StepBackEmptyHistory) {
	EXPECT_FALSE(simulator->stepBack());
	EXPECT_FALSE(simulator->isViewingReplay());
}

TEST_F(ReplaysEvaluatorTest, SkipBackEmptyHistory) {
	EXPECT_FALSE(simulator->skipBack());
	EXPECT_FALSE(simulator->isViewingReplay());
}

TEST_F(ReplaysEvaluatorTest, StepForward) {
	simulator->stepForward();
	EXPECT_FALSE(simulator->isViewingReplay());
	EXPECT_TRUE(simulator->isPause());
}

TEST_F(ReplaysEvaluatorTest, StepForwardThenBack) {
	simulator->stepForward();
	EXPECT_FALSE(simulator->isViewingReplay());
	EXPECT_TRUE(simulator->isPause());
	EXPECT_TRUE(simulator->stepBack());
	EXPECT_TRUE(simulator->isViewingReplay());
	EXPECT_TRUE(simulator->isPause());
}

TEST_F(ReplaysEvaluatorTest, SkipForwardWithoutReplay) {
	EXPECT_FALSE(simulator->skipForward());
	EXPECT_FALSE(simulator->isViewingReplay());
	EXPECT_TRUE(simulator->isPause());
}

TEST_F(ReplaysEvaluatorTest, SetStateWhileViewingReplayReturnsToLiveState) {
	Position switchPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));

	simulator->setState(switchPos, H);
	simulator->stepForward();
	simulator->setState(switchPos, L);

	ASSERT_TRUE(simulator->stepBack());
	ASSERT_TRUE(simulator->isViewingReplay());
	EXPECT_EQ(simulator->getState(switchPos), H);

	simulator->setState(switchPos, L);
	EXPECT_FALSE(simulator->isViewingReplay());
	EXPECT_EQ(simulator->getState(switchPos), L);

	ASSERT_TRUE(simulator->stepBack());
	EXPECT_TRUE(simulator->isViewingReplay());
	EXPECT_EQ(simulator->getState(switchPos), H);
}

TEST_F(ReplaysEvaluatorTest, SetStateWhileViewingReplayPreservesCurrentLiveState) {
	Position switch1Pos(0, 0);
	Position switch2Pos(1, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switch1Pos, 0, BlockType::SWITCH));
	ASSERT_TRUE(circuit->tryInsertBlock(switch2Pos, 0, BlockType::SWITCH));

	simulator->setState(switch1Pos, H);
	simulator->setState(switch2Pos, L);

	simulator->stepForward();
	simulator->setState(switch1Pos, L);
	simulator->setState(switch2Pos, H);

	ASSERT_TRUE(simulator->stepBack());
	ASSERT_TRUE(simulator->isViewingReplay());
	ASSERT_EQ(simulator->getState(switch1Pos), H);
	ASSERT_EQ(simulator->getState(switch2Pos), L);

	simulator->setState(switch1Pos, L);
	EXPECT_FALSE(simulator->isViewingReplay());
	EXPECT_EQ(simulator->getState(switch1Pos), L);
	EXPECT_EQ(simulator->getState(switch2Pos), H);
}

TEST_F(ReplaysEvaluatorTest, SkipUsesSetStateEvents) {
	Position switchPos(0, 0);
	ASSERT_TRUE(circuit->tryInsertBlock(switchPos, 0, BlockType::SWITCH));

	simulator->setState(switchPos, H);
	simulator->stepForward();
	simulator->setState(switchPos, L);
	for (int i = 0; i < 5; i++) {
		simulator->stepForward();
	}

	ASSERT_TRUE(simulator->skipBack());
	EXPECT_TRUE(simulator->isViewingReplay());
	EXPECT_EQ(simulator->getState(switchPos), L);

	ASSERT_TRUE(simulator->skipBack());
	EXPECT_TRUE(simulator->isViewingReplay());
	EXPECT_EQ(simulator->getState(switchPos), H);

	ASSERT_TRUE(simulator->skipForward());
	EXPECT_TRUE(simulator->isViewingReplay());
	EXPECT_EQ(simulator->getState(switchPos), L);

	ASSERT_TRUE(simulator->skipForward());
	EXPECT_FALSE(simulator->isViewingReplay());
	EXPECT_EQ(simulator->getState(switchPos), L);
}

TEST_F(ReplaysEvaluatorTest, StepForwardALotThenBack) {
	for (int i = 0; i < 20; i++) {
		simulator->stepForward();
	}
	EXPECT_FALSE(simulator->isViewingReplay());
	EXPECT_TRUE(simulator->isPause());
	for (int i = 0; i < 20; i++) {
		EXPECT_TRUE(simulator->stepBack());
		EXPECT_TRUE(simulator->isViewingReplay());
	}
	EXPECT_TRUE(simulator->isPause());

	EXPECT_FALSE(simulator->stepBack());
	EXPECT_TRUE(simulator->isViewingReplay());
	EXPECT_TRUE(simulator->isPause());

	EXPECT_TRUE(simulator->skipForward());
	EXPECT_FALSE(simulator->isViewingReplay());
	EXPECT_TRUE(simulator->isPause());
}
