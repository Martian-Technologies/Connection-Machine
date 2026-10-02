//! The host must serialize calls on an engine, supply valid pointers and release
//! the engine only after its control thread stops. No Rust allocation crosses the ABI.
use super::Engine;

#[no_mangle]
pub extern "C" fn cm_sim_new() -> *mut Engine {
    Box::into_raw(Box::new(Engine::new()))
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_free(engine: *mut Engine) {
    if !engine.is_null() {
        drop(Box::from_raw(engine));
    }
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_add_gate(engine: &mut Engine, kind: u16) -> u32 {
    engine.add_gate(kind)
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_remove_gate(engine: &mut Engine, id: u32) -> bool {
    engine.remove_gate(id)
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_connect(
    engine: &mut Engine,
    source: u32,
    destination: u32,
    port: u32,
    delta: i32,
) -> bool {
    engine.connect(source, destination, port, delta)
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_compile(engine: &mut Engine) {
    engine.compile();
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_reset(engine: &mut Engine) {
    engine.reset();
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_set_realistic(engine: &mut Engine, realistic: bool) {
    engine.set_realistic(realistic);
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_set_threads(engine: &mut Engine, threads: usize) {
    engine.set_threads(threads);
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_set_state(engine: &mut Engine, id: u32, value: u8) {
    engine.set_state(id, value);
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_state(engine: &Engine, id: u32) -> u8 {
    engine.state(id)
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_states(
    engine: &Engine,
    ids: *const u32,
    output: *mut u8,
    count: usize,
) {
    if count == 0 {
        return;
    }
    let ids = std::slice::from_raw_parts(ids, count);
    let output = std::slice::from_raw_parts_mut(output, count);
    for (out, &id) in output.iter_mut().zip(ids) {
        *out = engine.state(id);
    }
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_tick(engine: &mut Engine, count: u32) {
    for _ in 0..count {
        engine.tick();
    }
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_step_back(engine: &mut Engine) -> bool {
    engine.step_back()
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_step_forward(engine: &mut Engine) -> bool {
    engine.step_forward()
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_skip_back(engine: &mut Engine) -> bool {
    engine.skip_back()
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_skip_forward(engine: &mut Engine) -> bool {
    engine.skip_forward()
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_return_to_live(engine: &mut Engine) {
    engine.return_to_live();
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_is_replay(engine: &Engine) -> bool {
    engine.is_viewing_replay()
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_tick_index(engine: &Engine) -> u64 {
    engine.ticks
}
#[no_mangle]
pub unsafe extern "C" fn cm_sim_replay_bytes(engine: &Engine) -> usize {
    engine.replay_bytes()
}
