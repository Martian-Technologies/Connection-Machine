#ifndef simulatorDefs_h
#define simulatorDefs_h

#include "util/id.h"

DECLARE_ID_TYPE(w_vec_index, unsigned int);
DECLARE_ID_TYPE(simulator_id_t, unsigned int);
DECLARE_ID_TYPE(gate_group_id_t, unsigned int);
DECLARE_ID_TYPE(simulator_state_reference, unsigned int);

typedef std::variant<simulator_state_reference, std::vector<simulator_state_reference>> SimulatorStateIndexVecVariant;

#endif /* simulatorDefs_h */