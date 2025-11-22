#ifndef GetNonExistingObjects_h
#define GetNonExistingObjects_h

#include <gtest/gtest.h>
#include "backend/evaluator/simulator/logicState.h"

class GetNonExistingObjects : public ::testing::Test {
protected:
    logic_state_t L = logic_state_t::LOW;
    logic_state_t H = logic_state_t::HIGH;
    logic_state_t Z = logic_state_t::FLOATING;
    logic_state_t X = logic_state_t::UNDEFINED;
};

#endif /* GetNonExistingObjects_h */
