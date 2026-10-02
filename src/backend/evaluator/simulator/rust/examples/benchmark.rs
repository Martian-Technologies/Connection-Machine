use cm_simulator::Engine;
use std::time::Instant;

fn main() {
    let args: Vec<_> = std::env::args().collect();
    let n: u32 = args.get(1).map_or(10_000, |s| s.parse().unwrap());
    let ticks: u32 = args.get(2).map_or(2_000, |s| s.parse().unwrap());
    let threads = args.get(3).map_or(1, |s| s.parse().unwrap());
    let oscillating = args.get(4).is_some_and(|s| s == "oscillating");
    let mut engine = Engine::new();
    engine.set_threads(threads);
    let start = Instant::now();
    for _ in 0..n {
        let id = engine.add_gate(if oscillating { 8 } else { 7 });
        engine.connect(id, id, 0, 1);
    }
    engine.compile();
    let build_ms = start.elapsed().as_secs_f64() * 1000.0;
    for _ in 0..10 {
        engine.tick();
    }
    let start = Instant::now();
    for _ in 0..ticks {
        engine.tick();
    }
    let ns = start.elapsed().as_secs_f64() * 1e9 / ticks as f64;
    println!("gates={n} ticks={ticks} threads={threads} oscillating={oscillating} build_ms={build_ms:.3} ns_per_tick={ns:.1} replay_bytes={} checksum={}", engine.replay_bytes(), engine.state(n + 3));
}
