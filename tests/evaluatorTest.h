#ifndef evaulatorTests_h
#define evaulatorTests_h

#include <gtest/gtest.h>
#include "backend/backend.h"

class EvaluatorTest : public ::testing::Test {
public:
	EvaluatorTest() : backend(nullptr) {
		backend.getCircuitManager().getBlockDataManager()->initializeDefaults();
	}
	void changeState(const Address& addr);
	void readState(const Address& addr) const;

protected:
	void SetUp() override;
	void TearDown() override;
	Backend backend;
	SharedCircuit circuit;
	SharedEvaluator evaluator;
	int i;
};

#endif /* evaulatorTests_h */
