#ifndef circuitSelectionTests_h
#define circuitSelectionTests_h

#include <gtest/gtest.h>
#include "backend/circuit/circuitManager.h"
#include "backend/evaluator/evaluatorManager.h"

class CircuitSelectionTest : public ::testing::Test {
public:
	CircuitSelectionTest() : evaluatorManager(&dataUpdateEventManager), circuitManager(&dataUpdateEventManager, &evaluatorManager, nullptr) {
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

#endif /* circuitSelectionTests_h */
