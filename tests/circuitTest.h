#ifndef circuitTests_h
#define circuitTests_h

#include <gtest/gtest.h>
#include "backend/circuit/circuitManager.h"
#include "backend/evaluator/evaluatorManager.h"

class CircuitTest : public ::testing::Test {
public:
	CircuitTest() : evaluatorManager(&dataUpdateEventManager), circuitManager(&dataUpdateEventManager, &evaluatorManager, nullptr) {
		circuitManager.getBlockDataManager()->initializeDefaults();
	}

protected:
	void SetUp() override;
	void TearDown() override;
	DataUpdateEventManager dataUpdateEventManager;
	EvaluatorManager evaluatorManager;
	CircuitManager circuitManager;
	SharedCircuit circuit = nullptr;
	int i;
};

#endif /* circuitTests_h */
