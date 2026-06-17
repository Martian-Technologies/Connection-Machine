#include <gtest/gtest.h>
#include "environment/environment.h"
#include "backend/evaluator/simulator/evalLogicSimulator.h"

class NoEditSimulatorTest : public ::testing::Test {
protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment {false};
	EvalLogicSimulator* simulator = nullptr;
	Circuit* circuit = nullptr;
};

void NoEditSimulatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
	simulator_id_t simulatorId = environment.getBackend().createSimulator(circuitId).value();
	simulator = environment.getBackend().getSimulator(simulatorId);
}

void NoEditSimulatorTest::TearDown() {
	circuit = nullptr;
	simulator = nullptr;
}

TEST_F(NoEditSimulatorTest, PauseUnpause) {
	simulator->setPause(true);
	EXPECT_TRUE(simulator->isPause());
	EXPECT_DOUBLE_EQ(simulator->getRealTickrate(), 0.0);
	simulator->setPause(false);
	EXPECT_FALSE(simulator->isPause());
	// wait up to 200 ms for the background thread to update the tickrate
	auto start = std::chrono::steady_clock::now();
	bool tickPositive = false;
	while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(200)) {
		if (simulator->getRealTickrate() > 0.0) { tickPositive = true; break; }
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	EXPECT_TRUE(tickPositive);
}

TEST_F(NoEditSimulatorTest, TickStep) {
	simulator->setPause(true);
	EXPECT_TRUE(simulator->isPause());
	simulator->tickStep();
	EXPECT_TRUE(simulator->isPause());
	EXPECT_DOUBLE_EQ(simulator->getRealTickrate(), 0.0);
	EXPECT_FALSE(simulator->isViewingReplay());
}

TEST_F(NoEditSimulatorTest, RealisticMode) {
	EXPECT_FALSE(simulator->isRealistic());
	simulator->setRealistic(true);
	EXPECT_TRUE(simulator->isRealistic());
	simulator->setRealistic(false);
	EXPECT_FALSE(simulator->isRealistic());
}

TEST_F(NoEditSimulatorTest, TickrateSetting) {
	simulator->setTickrate(20.0);
	EXPECT_DOUBLE_EQ(simulator->getTickrate(), 20.0);
	simulator->setTickrate(100.0);
	EXPECT_DOUBLE_EQ(simulator->getTickrate(), 100.0);
}

TEST_F(NoEditSimulatorTest, UseTickrateSetting) {
	EXPECT_TRUE(simulator->getUseTickrate());
	simulator->setUseTickrate(false);
	EXPECT_FALSE(simulator->getUseTickrate());
	simulator->setUseTickrate(true);
	EXPECT_TRUE(simulator->getUseTickrate());
}

TEST_F(NoEditSimulatorTest, EvalName) {
	std::string expectedName = "Sim " + std::to_string(simulator->getSimulatorId()) + " (" + circuit->getCircuitNameNumber() + ")";
	EXPECT_EQ(simulator->getSimulatorName(), expectedName);
}
