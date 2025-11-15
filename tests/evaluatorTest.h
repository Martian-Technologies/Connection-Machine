#ifndef evaulatorTests_h
#define evaulatorTests_h

#include <gtest/gtest.h>
#include "backend/address.h"
#include "environment/environment.h"

class EvaluatorTest : public ::testing::Test {
public:
	void changeState(const Address& addr);
	void readState(const Address& addr) const;

protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment {false};
	SharedEvaluator evaluator = nullptr;
	SharedCircuit circuit = nullptr;
	int i;
};

#endif /* evaulatorTests_h */
