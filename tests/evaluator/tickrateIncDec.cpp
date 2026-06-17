#include <gtest/gtest.h>
#include "environment/environment.h"
#include "backend/evaluator/simulator/evalLogicSimulator.h"

class TickrateIncDecEvaluatorTest : public ::testing::TestWithParam<double> {
protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment {false};
	EvalLogicSimulator* simulator = nullptr;
	Circuit* circuit = nullptr;
};

void TickrateIncDecEvaluatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	circuit = environment.getBackend().getCircuitManager().getCircuit(circuitId);
	simulator_id_t simulatorId = environment.getBackend().createSimulator(circuitId).value();
	simulator = environment.getBackend().getSimulator(simulatorId);
}

void TickrateIncDecEvaluatorTest::TearDown() {
	circuit = nullptr;
	simulator = nullptr;
}

TEST_P(TickrateIncDecEvaluatorTest, IncreaseTickrate) {
	double tickrate = GetParam();
	simulator->setTickrate(tickrate);
	EXPECT_DOUBLE_EQ(simulator->getTickrate(), tickrate);
	simulator->increaseTickrateSeq();
	EXPECT_GT(simulator->getTickrate(), tickrate);
}

TEST_P(TickrateIncDecEvaluatorTest, DecreaseTickrate) {
	double tickrate = GetParam();
	simulator->setTickrate(tickrate);
	EXPECT_DOUBLE_EQ(simulator->getTickrate(), tickrate);
	simulator->decreaseTickrateSeq();
	if (tickrate > EvalLogicSimulator::MIN_TICKRATE_DECREASABLE) {
		EXPECT_LT(simulator->getTickrate(), tickrate);
	} else {
		EXPECT_DOUBLE_EQ(simulator->getTickrate(), EvalLogicSimulator::MIN_TICKRATE_DECREASABLE);
	}
}

INSTANTIATE_TEST_SUITE_P(
	SmallValues,
	TickrateIncDecEvaluatorTest,
	::testing::Range(0.05, 1.05, 0.05)
);

INSTANTIATE_TEST_SUITE_P(
	NormalValues,
	TickrateIncDecEvaluatorTest,
	::testing::Values(0.5, 0.9, 1.0, 1.1, 2.0, 5.0, 10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 60.0, 70.0, 80.0, 90.0, 100.0, 120.0, 200.0, 240.0, 480.0, 500.0, 1000.0, 2000.0)
);

TEST_F(TickrateIncDecEvaluatorTest, ExactSequenceRounding) {
	struct {
		double start;
		double expectedNext;
		double expectedPrev;
	} cases[] = {
		{ 0.9, 1.0, 0.8 },
		{ 1.0, 2.0, 0.9 },
		{ 1.9, 2.0, 1.0 },
		{ 2.0, 3.0, 1.0 },
		{ 9.0, 10.0, 8.0 },
		{ 9.1, 10.0, 9.0 },
		{ 10.0, 20.0, 9.0 },
		{ 19.0, 20.0, 10.0 },
		{ 90.0, 100.0, 80.0 },
		{ 99.9, 100.0, 90.0 },
		{ 100.0, 200.0, 90.0 }
	};

	for (const auto& c : cases) {
		simulator->setTickrate(c.start);
		simulator->increaseTickrateSeq();
		EXPECT_DOUBLE_EQ(simulator->getTickrate(), c.expectedNext);

		simulator->setTickrate(c.start);
		simulator->decreaseTickrateSeq();
		EXPECT_DOUBLE_EQ(simulator->getTickrate(), c.expectedPrev);
	}
}
