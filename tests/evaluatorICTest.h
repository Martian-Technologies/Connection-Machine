#ifndef evaluatorICTest_h
#define evaluatorICTest_h

#include <gtest/gtest.h>
#include "environment/environment.h"

class EvaluatorICTest : public ::testing::Test {
protected:
    void SetUp() override;
    void TearDown() override;
	Environment environment {false};
	SharedCircuit parentCircuit = nullptr;
	SharedEvaluator evaluator = nullptr;
    int idx;

    circuit_id_t createPassThroughIC(const std::string& name);
    inline BlockType getICBlockType(circuit_id_t cid) {
        auto* cbd = environment.getBackend().getCircuitManager().getCircuitBlockDataManager().getCircuitBlockData(cid);
        return cbd ? cbd->getBlockType() : BlockType::NONE;
    }
};

#endif /* evaluatorICTest_h */
