#ifndef compiledGateGroup_h
#define compiledGateGroup_h

DECLARE_ID_TYPE(gate_group_id_t, unsigned int);
DECLARE_ID_TYPE(gate_index_in_group_t, unsigned int);

class CompiledGateGroup {
public:
	CompiledGateGroup() {}
	~CompiledGateGroup() = default;
	bool operator==(const CompiledGateGroup& o) const = default;
private:

};

#endif /* compiledGateGroup_h */