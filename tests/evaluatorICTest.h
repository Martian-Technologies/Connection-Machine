#ifndef evaluatorICTest_h
#define evaluatorICTest_h

#include <gtest/gtest.h>
#include "backend/backend.h"

class EvaluatorICTest : public ::testing::Test {
public:
    EvaluatorICTest() : backend(nullptr) {
		backend.getCircuitManager().getBlockDataManager()->initializeDefaults();
	}

protected:
    void SetUp() override;
    void TearDown() override;
    Backend backend;
    SharedCircuit parentCircuit;
    SharedEvaluator evaluator;
    int idx;

    circuit_id_t createPassThroughIC(const std::string& name);
    inline BlockType getICBlockType(circuit_id_t cid) {
        auto* cbdm = backend.getCircuitManager().getCircuitBlockDataManager();
        auto* cbd = cbdm->getCircuitBlockData(cid);
        return cbd ? cbd->getBlockType() : BlockType::NONE;
    }
};

#endif /* evaluatorICTest_h */
