#ifndef circuitTests_h
#define circuitTests_h

#include <gtest/gtest.h>
#include "environment/environment.h"

class CircuitTest : public ::testing::Test {
protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment {false};
	SharedCircuit circuit = nullptr;
	int i;
};

#endif /* circuitTests_h */
