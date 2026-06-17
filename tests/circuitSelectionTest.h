#ifndef circuitSelectionTests_h
#define circuitSelectionTests_h

#include <gtest/gtest.h>
#include "environment/environment.h"

class CircuitSelectionTest : public ::testing::Test {
protected:
	void SetUp() override;
	void TearDown() override;
	Environment environment {false};
	Circuit* circuit;
	int i;
};

#endif /* circuitSelectionTests_h */
