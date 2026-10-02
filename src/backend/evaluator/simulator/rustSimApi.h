#ifndef rustSimApi_h
#define rustSimApi_h
#include <cstddef>
#include <cstdint>

// Serialize calls per engine. Buffers belong to the caller; their pointers are
// not retained. Stop the control thread before freeing the engine.
struct CmSimulator;
extern "C" {
	CmSimulator* cm_sim_new();
	void cm_sim_free(CmSimulator*);
	std::uint32_t cm_sim_add_gate(CmSimulator*, std::uint16_t kind);
	bool cm_sim_remove_gate(CmSimulator*, std::uint32_t id);
	bool cm_sim_connect(CmSimulator*, std::uint32_t source, std::uint32_t destination, std::uint32_t port, std::int32_t delta);
	void cm_sim_compile(CmSimulator*);
	void cm_sim_reset(CmSimulator*);
	void cm_sim_set_realistic(CmSimulator*, bool realistic);
	void cm_sim_set_threads(CmSimulator*, std::size_t threads);
	void cm_sim_set_state(CmSimulator*, std::uint32_t id, std::uint8_t value);
	std::uint8_t cm_sim_state(const CmSimulator*, std::uint32_t id);
	void cm_sim_states(const CmSimulator*, const std::uint32_t* ids, std::uint8_t* output, std::size_t count);
	void cm_sim_tick(CmSimulator*, std::uint32_t count);
	bool cm_sim_step_back(CmSimulator*);
	bool cm_sim_step_forward(CmSimulator*);
	bool cm_sim_skip_back(CmSimulator*);
	bool cm_sim_skip_forward(CmSimulator*);
	void cm_sim_return_to_live(CmSimulator*);
	bool cm_sim_is_replay(const CmSimulator*);
	std::uint64_t cm_sim_tick_index(const CmSimulator*);
	std::size_t cm_sim_replay_bytes(const CmSimulator*);
	void cm_sim_set_jit_enabled(CmSimulator*, bool enabled);
	// Four counters: gate evaluations, compiled groups, code bytes, JIT calls.
	void cm_sim_diagnostics(const CmSimulator*, std::uint64_t* output);
}
#endif /* rustSimApi_h */
