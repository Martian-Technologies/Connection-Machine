#ifndef compiledGateGroup_h
#define compiledGateGroup_h

#include "simulatorDefs.h"

class CompiledGateGroup {
public:
	using FetchGroup = std::pair<gate_group_id_t, std::vector<std::pair<r_vec_index, w_vec_index>>>;
	using Junction = std::pair<r_vec_index, std::vector<r_vec_index>>;
	using Gate = std::tuple<w_vec_index, BlockType, std::vector<r_vec_index>>;

	CompiledGateGroup(
		std::vector<FetchGroup> fetchGroups,
		std::vector<Junction> junctions,
		std::vector<Gate> gates
	) : fetchGroups(std::move(fetchGroups)),
		junctions(std::move(junctions)),
		gates(std::move(gates)),
		totalRVecSize(fetchGroups.size() + junctions.size()),
		totalWVecSize(gates.size())
	{}
	~CompiledGateGroup() = default;
	bool operator==(const CompiledGateGroup& o) const = default;

private:
	std::vector<FetchGroup> fetchGroups;
	std::vector<Junction> junctions;
	std::vector<Gate> gates;
	unsigned int totalRVecSize;
	unsigned int totalWVecSize;
};

#endif /* compiledGateGroup_h */