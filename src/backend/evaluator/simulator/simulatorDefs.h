#ifndef simulatorDefs_h
#define simulatorDefs_h

#include "util/id.h"

DECLARE_ID_TYPE(r_vec_index, unsigned int);
DECLARE_ID_TYPE(w_vec_index, unsigned int);
DECLARE_ID_TYPE(simulator_id_t, unsigned int);
DECLARE_ID_TYPE(gate_group_id_t, unsigned int);

struct simulator_state_index_t {
    unsigned long long value;
    bool operator==(const simulator_state_index_t& other) const {
        return value == other.value;
    }
    bool operator!=(const simulator_state_index_t& other) const {
        return value != other.value;
    }
    gate_group_id_t getGateGroupId() const {
        return gate_group_id_t(value >> 32);
    }
    r_vec_index getRVecIndex() const {
        return r_vec_index((value >> 16) & 0xFFFF);
    }
};

typedef std::variant<simulator_state_index_t, std::vector<simulator_state_index_t>> SimulatorStateIndexVecVariant;

#endif /* simulatorDefs_h */