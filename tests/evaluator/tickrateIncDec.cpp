#include <gtest/gtest.h>
#include "environment/environment.h"
#include "backend/evaluator/evaluator.h"

class TickrateIncDecEvaluatorTest : public ::testing::TestWithParam<double> {
protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment {false};
	SharedEvaluator evaluator = nullptr;
	SharedCircuit circuit = nullptr;
};

void TickrateIncDecEvaluatorTest::SetUp() {
	circuit_id_t circuitId = environment.getBackend().getCircuitManager().createNewCircuit(false);
	circuit = environment.getBackend().getCircuit(circuitId);
	evaluator_id_t evalId = environment.getBackend().createEvaluator(circuitId).value();
	evaluator = environment.getBackend().getEvaluator(evalId);
}

void TickrateIncDecEvaluatorTest::TearDown() {
	circuit.reset();
	evaluator.reset();
}

TEST_P(TickrateIncDecEvaluatorTest, IncreaseTickrate) {
	double tickrate = GetParam();
	evaluator->setTickrate(tickrate);
	EXPECT_DOUBLE_EQ(evaluator->getTickrate(), tickrate);
	evaluator->increaseTickrateSeq();
	EXPECT_GT(evaluator->getTickrate(), tickrate);
}

TEST_P(TickrateIncDecEvaluatorTest, DecreaseTickrate) {
	double tickrate = GetParam();
	evaluator->setTickrate(tickrate);
	EXPECT_DOUBLE_EQ(evaluator->getTickrate(), tickrate);
	evaluator->decreaseTickrateSeq();
	if (tickrate > Evaluator::MIN_TICKRATE_DECREASABLE) {
		EXPECT_LT(evaluator->getTickrate(), tickrate);
	} else {
		EXPECT_DOUBLE_EQ(evaluator->getTickrate(), Evaluator::MIN_TICKRATE_DECREASABLE);
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
		evaluator->setTickrate(c.start);
		evaluator->increaseTickrateSeq();
		EXPECT_DOUBLE_EQ(evaluator->getTickrate(), c.expectedNext);

		evaluator->setTickrate(c.start);
		evaluator->decreaseTickrateSeq();
		EXPECT_DOUBLE_EQ(evaluator->getTickrate(), c.expectedPrev);
	}
}
