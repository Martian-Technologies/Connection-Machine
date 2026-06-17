#ifndef circuitTests_h
#define circuitTests_h

#include "environment/environment.h"
#include <gtest/gtest.h>

class CircuitTest : public ::testing::Test {
protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment{ false };
	Circuit* circuit = nullptr;
	int i;
};

#endif /* circuitTests_h */
