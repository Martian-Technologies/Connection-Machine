use cm_simulator::{Engine, FLOATING as Z, HIGH as H, LOW as L, UNDEFINED as X};

// Independent truth-table oracle, expressed as counts rather than the engine's
// short-circuit kernels. Empty-input behavior is part of the app's existing contract.
fn reference(kind: u16, inputs: &[u8]) -> u8 {
    let lows = inputs.iter().filter(|&&s| s == L).count();
    let highs = inputs.iter().filter(|&&s| s == H).count();
    let unknown = inputs.iter().any(|&s| s >= Z);
    if inputs.is_empty() {
        return match kind {
            7 | 8 | 12 => X,
            9 => Z,
            11 => H,
            _ => L,
        };
    }
    match kind {
        1 | 4 => {
            let s = if lows > 0 {
                L
            } else if unknown {
                X
            } else {
                H
            };
            if kind == 4 && s != X {
                s ^ 1
            } else {
                s
            }
        }
        2 | 5 => {
            let s = if highs > 0 {
                H
            } else if unknown {
                X
            } else {
                L
            };
            if kind == 5 && s != X {
                s ^ 1
            } else {
                s
            }
        }
        3 | 6 => {
            if unknown {
                X
            } else {
                (highs % 2) as u8 ^ u8::from(kind == 6)
            }
        }
        7 | 8 => {
            if inputs.contains(&X) || (lows > 0 && highs > 0) || (lows == 0 && highs == 0) {
                X
            } else {
                u8::from(highs > 0) ^ u8::from(kind == 8)
            }
        }
        9..=12 => {
            if inputs.contains(&X) || (lows > 0 && highs > 0) {
                X
            } else if lows > 0 {
                L
            } else if highs > 0 {
                H
            } else {
                match kind {
                    10 => L,
                    11 => H,
                    12 => X,
                    _ => Z,
                }
            }
        }
        _ => panic!("invalid reference gate"),
    }
}

#[test]
fn exhaustive_four_state_truth_tables() {
    let mut cases = 0;
    for realistic in [false, true] {
        for kind in 1..=12 {
            for n in 0..=3 {
                for code in 0..4u32.pow(n) {
                    let mut e = Engine::new();
                    e.set_realistic(realistic);
                    let out = e.add_gate(kind);
                    let net = if (kind == 7 || kind == 8) && n > 1 {
                        Some(e.add_gate(9))
                    } else {
                        None
                    };
                    if let Some(net) = net {
                        assert!(e.connect(net, out, 0, 1));
                    }
                    let mut inputs = Vec::new();
                    for i in 0..n {
                        let switch = e.add_gate(16);
                        let s = ((code >> (i * 2)) & 3) as u8;
                        assert!(e.connect(switch, net.unwrap_or(out), 0, 1));
                        e.set_state(switch, s);
                        inputs.push(s);
                    }
                    e.tick();
                    e.tick();
                    assert_eq!(
                        e.state(out),
                        reference(kind, &inputs),
                        "kind={kind} inputs={inputs:?} realistic={realistic}"
                    );
                    cases += 1;
                }
            }
        }
    }
    assert_eq!(cases, 2040);
}

#[test]
fn tristate_and_weighted_connections() {
    for data in 0..4 {
        for enable in 0..4 {
            let mut e = Engine::new();
            let d = e.add_gate(16);
            let c = e.add_gate(16);
            let out = e.add_gate(13);
            assert!(e.connect(d, out, 0, 2));
            assert!(e.connect(c, out, 1, 2));
            e.set_state(d, data);
            e.set_state(c, enable);
            e.tick();
            let expected = match enable {
                L => Z,
                H if data < Z => data,
                _ => X,
            };
            assert_eq!(e.state(out), expected);
            assert!(e.connect(d, out, 0, -1));
            assert!(e.connect(c, out, 1, -1));
            e.tick();
            assert_eq!(
                e.state(out),
                expected,
                "one weighted removal must retain each input"
            );
        }
    }
    let mut e = Engine::new();
    let a = e.add_gate(16);
    let out = e.add_gate(3);
    assert!(e.connect(a, out, 0, 2));
    e.set_state(a, H);
    e.tick();
    assert_eq!(e.state(out), L);
    assert!(e.connect(a, out, 0, -1));
    e.tick();
    assert_eq!(e.state(out), H);
}

#[test]
fn tick_boundaries_feedback_and_realistic_transitions() {
    let mut e = Engine::new();
    let a = e.add_gate(16);
    let b = e.add_gate(7);
    let c = e.add_gate(7);
    e.connect(a, b, 0, 1);
    e.connect(b, c, 0, 1);
    e.compile();
    e.set_state(a, H);
    assert_eq!(e.state(c), L);
    e.tick();
    assert_eq!(e.state(b), H);
    assert_eq!(e.state(c), L);
    e.tick();
    assert_eq!(e.state(c), H);
    e.set_realistic(true);
    e.reset();
    e.tick();
    e.tick();
    e.set_state(a, H);
    e.tick();
    assert_eq!(e.state(b), X);
    e.tick();
    assert_eq!(e.state(b), H);
    let mut e = Engine::new();
    let a = e.add_gate(8);
    e.connect(a, a, 0, 1);
    for i in 0..50 {
        e.tick();
        assert_eq!(e.state(a), if i % 2 == 0 { H } else { L });
    }
}

#[test]
fn junction_nets_resolve_without_adding_ticks() {
    let mut e = Engine::new();
    let a = e.add_gate(16);
    let b = e.add_gate(9);
    let c = e.add_gate(11);
    let d = e.add_gate(7);
    e.connect(a, b, 0, 1);
    e.connect(b, c, 0, 1);
    e.connect(c, d, 0, 1);
    e.compile();
    e.set_state(a, H);
    assert_eq!(e.state(b), H);
    assert_eq!(e.state(c), H);
    e.tick();
    assert_eq!(e.state(d), H);
    e.set_state(a, Z);
    assert_eq!(e.state(b), H);
    assert_eq!(e.state(c), H);
    let pull_down = e.add_gate(10);
    e.connect(pull_down, b, 0, 1);
    e.compile();
    assert_eq!(e.state(b), X);
    assert_eq!(e.state(c), X);
}

#[test]
fn editing_and_replay_never_restore_stale_live_state() {
    let mut e = Engine::new();
    let a = e.add_gate(16);
    let b = e.add_gate(8);
    e.connect(a, b, 0, 1);
    e.set_state(a, H);
    e.tick();
    assert_eq!(e.state(b), L);
    let unrelated = e.add_gate(16);
    e.compile();
    assert_eq!(e.state(b), L);
    e.set_state(a, L);
    e.tick();
    assert_eq!(e.state(b), H);
    e.set_state(a, H);
    e.tick();
    assert_eq!(e.state(b), L);
    assert!(e.step_back());
    assert!(e.step_back());
    assert_eq!(e.state(a), L);
    e.add_gate(16);
    e.compile();
    assert!(!e.is_viewing_replay());
    assert_eq!(e.state(a), H);
    e.set_state(unrelated, H);
    assert_eq!(e.state(a), H);
    assert_eq!(e.state(b), L);
}

#[test]
fn replay_roundtrip_input_events_wrap_and_id_reuse() {
    let mut e = Engine::new();
    let a = e.add_gate(16);
    e.set_state(a, H);
    e.tick();
    e.set_state(a, L);
    assert!(e.step_back());
    assert_eq!(e.state(a), H);
    e.set_state(a, L);
    assert!(!e.is_viewing_replay());
    assert_eq!(e.state(a), L);
    assert!(e.step_back());
    assert_eq!(e.state(a), H);
    assert!(e.step_forward());
    assert_eq!(e.state(a), L);
    e.reset();
    let oscillator = e.add_gate(8);
    e.connect(oscillator, oscillator, 0, 1);
    for _ in 0..1100 {
        e.tick();
    }
    for _ in 0..1024 {
        assert!(e.step_back());
    }
    assert!(!e.step_back());
    for _ in 0..1024 {
        assert!(e.step_forward());
    }
    assert!(!e.is_viewing_replay());
    assert_eq!(e.state(oscillator), L);
    assert!(e.remove_gate(oscillator));
    let replacement = e.add_gate(16);
    assert_eq!(replacement, oscillator);
    assert_eq!(e.state(replacement), L);
    assert!(!e.step_back());
}

#[test]
fn settled_circuits_do_no_gate_work() {
    let mut e = Engine::new();
    let a = e.add_gate(7);
    e.connect(a, a, 0, 1);
    e.tick();
    let evaluations = e.evaluations;
    for _ in 0..10_000 {
        e.tick();
    }
    assert_eq!(e.evaluations, evaluations);
    assert_eq!(e.replay_bytes(), 0);

    // Recycled delta capacity must not be charged to later empty frames.
    let low = e.add_gate(17);
    let oscillators: Vec<_> = (0..8).map(|_| e.add_gate(8)).collect();
    for &id in &oscillators {
        e.connect(id, id, 0, 1);
    }
    for _ in 0..1000 {
        let id = e.add_gate(7);
        e.connect(id, id, 0, 1);
    }
    for _ in 0..1100 {
        e.tick();
    }
    for id in oscillators {
        e.connect(id, id, 0, -1);
        e.connect(low, id, 0, 1);
    }
    for _ in 0..1100 {
        e.tick();
    }
    assert_eq!(e.replay_bytes(), 0);
}

#[test]
fn parallel_and_scalar_execution_are_identical() {
    let mut scalar = Engine::new();
    let mut parallel = Engine::new();
    parallel.set_threads(4);
    for _ in 0..20_000 {
        let a = scalar.add_gate(8);
        let b = parallel.add_gate(8);
        assert_eq!(a, b);
        scalar.connect(a, a, 0, 1);
        parallel.connect(b, b, 0, 1);
    }
    for _ in 0..8 {
        scalar.tick();
        parallel.tick();
        for id in 4..20_004 {
            assert_eq!(scalar.state(id), parallel.state(id));
        }
    }
}

#[test]
fn randomized_differential_sequences() {
    for realistic in [false, true] {
        for seed in 0..20u64 {
            let mut rng = seed + 1;
            let mut random = || {
                rng ^= rng << 13;
                rng ^= rng >> 7;
                rng ^= rng << 17;
                rng
            };
            let mut e = Engine::new();
            e.set_realistic(realistic);
            let switches: Vec<_> = (0..4).map(|_| e.add_gate(16)).collect();
            let kinds: Vec<_> = (0..36).map(|_| (random() % 8 + 1) as u16).collect();
            let ids: Vec<_> = kinds.iter().map(|&kind| e.add_gate(kind)).collect();
            let mut sources = Vec::new();
            for (&id, &kind) in ids.iter().zip(&kinds) {
                let n = if kind == 7 || kind == 8 {
                    1
                } else {
                    (random() % 4) as usize
                };
                let inputs: Vec<u32> = (0..n).map(|_| (random() % 40 + 4) as u32).collect();
                for &source in &inputs {
                    assert!(e.connect(source, id, 0, 1));
                }
                sources.push(inputs);
            }
            e.reset();
            let mut states = vec![L; 44];
            states[..4].copy_from_slice(&[L, H, Z, X]);
            if realistic {
                states[8..].fill(X);
            }
            for _ in 0..100 {
                let switch = switches[(random() % 4) as usize];
                let value = (random() % 4) as u8;
                states[switch as usize] = value;
                e.set_state(switch, value);
                let mut next = states.clone();
                for (i, &id) in ids.iter().enumerate() {
                    let inputs: Vec<_> = sources[i].iter().map(|&s| states[s as usize]).collect();
                    let target = reference(kinds[i], &inputs);
                    let old = states[id as usize];
                    next[id as usize] = if realistic && old != X && old != target {
                        X
                    } else {
                        target
                    };
                }
                e.tick();
                states = next;
                for id in 4..44 {
                    assert_eq!(
                        e.state(id),
                        states[id as usize],
                        "seed={seed} realistic={realistic} id={id}"
                    );
                }
            }
        }
    }
}

#[test]
fn wire_edits_only_schedule_the_affected_instruction() {
    let mut e = Engine::new();
    let a = e.add_gate(16);
    let b = e.add_gate(16);
    e.set_state(b, H);
    let mut outputs = Vec::new();
    for _ in 0..10_000 {
        let out = e.add_gate(7);
        e.connect(a, out, 0, 1);
        outputs.push(out);
    }
    e.tick();
    let before = e.evaluations;
    assert!(e.connect(a, outputs[0], 0, -1));
    assert!(e.connect(b, outputs[0], 0, 1));
    e.compile();
    e.tick();
    assert_eq!(e.evaluations - before, 1);
    assert_eq!(e.state(outputs[0]), H);
    assert_eq!(e.state(outputs[1]), L);
    // Reverse references must also describe edges changed since compilation.
    assert!(e.remove_gate(b));
    e.tick();
    assert_eq!(e.state(outputs[0]), X);
    assert_eq!(e.state(outputs[1]), L);
}

#[test]
fn randomized_wire_edits_match_a_tick_oracle_through_input_compaction() {
    for realistic in [false, true] {
        let mut e = Engine::new();
        e.set_realistic(realistic);
        let drivers: Vec<_> = (0..4)
            .map(|s| {
                let id = e.add_gate(16);
                e.set_state(id, s);
                id
            })
            .collect();
        let gates: Vec<_> = (1..=8).map(|kind| e.add_gate(kind)).collect();
        let mut weights = [[0i32; 4]; 8];
        let mut old = [if realistic { X } else { L }; 8];
        let mut random = 1234567u64;
        for _ in 0..12_000 {
            random = random.wrapping_mul(6364136223846793005).wrapping_add(1);
            let which = ((random >> 32) % 8) as usize;
            let source = ((random >> 40) % 4) as usize;
            if which >= 6 {
                for (i, weight) in weights[which].iter_mut().enumerate() {
                    if *weight > 0 {
                        assert!(e.connect(drivers[i], gates[which], 0, -*weight));
                        *weight = 0;
                    }
                }
            }
            let delta = if weights[which][source] > 0 { -1 } else { 1 };
            assert!(e.connect(drivers[source], gates[which], 0, delta));
            weights[which][source] += delta;
            e.compile();
            e.tick();
            for i in 0..8 {
                let inputs: Vec<_> = weights[i]
                    .iter()
                    .enumerate()
                    .flat_map(|(s, &weight)| std::iter::repeat_n(s as u8, weight as usize))
                    .collect();
                let target = reference(i as u16 + 1, &inputs);
                let expected = if realistic && old[i] != X && old[i] != target {
                    X
                } else {
                    target
                };
                assert_eq!(e.state(gates[i]), expected, "gate={i} inputs={inputs:?}");
                old[i] = expected;
            }
        }
    }
}

#[cfg(feature = "jit")]
#[test]
fn jit_scalar_and_simd_match_all_four_state_inputs_and_realistic_transitions() {
    for realistic in [false, true] {
        let mut e = Engine::new();
        e.set_realistic(realistic);
        let a: Vec<_> = (0..32).map(|_| e.add_gate(16)).collect();
        let b: Vec<_> = (0..32).map(|_| e.add_gate(16)).collect();
        let mut cases = Vec::new();
        for kind in (1..=8).chain([13, 15]) {
            for lane in 0..32 {
                let out = e.add_gate(kind);
                if kind != 15 {
                    assert!(e.connect(a[lane], out, 0, 1));
                    if kind <= 6 || kind == 13 {
                        assert!(e.connect(b[lane], out, u32::from(kind == 13), 1));
                    }
                }
                cases.push((kind, lane, out));
            }
        }
        e.compile();
        for lane in 0..32 {
            e.set_state(a[lane], (lane % 4) as u8);
            e.set_state(b[lane], ((lane / 4) % 4) as u8);
        }
        e.prepare_jit();
        assert_eq!(e.jit_stats().0, cases.len().div_ceil(256));
        for tick in 0..2 {
            let old: Vec<_> = cases.iter().map(|&(_, _, id)| e.state(id)).collect();
            e.tick();
            for (index, &(kind, lane, out)) in cases.iter().enumerate() {
                let (a, b) = ((lane % 4) as u8, ((lane / 4) % 4) as u8);
                let target = match kind {
                    13 => match b {
                        L => Z,
                        H if a < Z => a,
                        _ => X,
                    },
                    15 => L,
                    7 | 8 => reference(kind, &[a]),
                    _ => reference(kind, &[a, b]),
                };
                let expected = if realistic && kind != 15 && old[index] != X && old[index] != target
                {
                    X
                } else {
                    target
                };
                assert_eq!(
                    e.state(out),
                    expected,
                    "kind={kind} lane={lane} tick={tick} realistic={realistic}"
                );
            }
        }
        assert!(e.jit_stats().2 > 0);
    }
}

#[cfg(feature = "jit")]
#[test]
fn jit_edits_settings_and_replay_match_the_interpreter() {
    let mut native = Engine::new();
    let mut reference = Engine::new();
    reference.set_jit_enabled(false);
    for engine in [&mut native, &mut reference] {
        for i in 0..800 {
            let id = engine.add_gate(if i % 2 == 0 { 4 } else { 8 });
            engine.connect(id, id, 0, if i % 2 == 0 { 2 } else { 1 });
        }
        engine.compile();
    }
    native.prepare_jit();
    for tick in 0..120 {
        if tick == 40 {
            for engine in [&mut native, &mut reference] {
                engine.connect(4, 4, 0, -2);
                engine.connect(5, 4, 0, 2);
                engine.compile();
            }
            assert_eq!(
                native.jit_stats().0,
                3,
                "a local edit invalidates one chunk"
            );
        }
        if tick == 60 {
            for engine in [&mut native, &mut reference] {
                engine.set_realistic(true);
            }
            assert_eq!(native.jit_stats().0, 0);
            native.prepare_jit();
        }
        if tick == 80 {
            for engine in [&mut native, &mut reference] {
                engine.remove_gate(15);
                engine.add_gate(8);
                engine.compile();
            }
            native.prepare_jit();
        }
        native.tick();
        reference.tick();
        for id in 4..804 {
            assert_eq!(native.state(id), reference.state(id), "tick={tick} id={id}");
        }
    }
    for _ in 0..100 {
        assert_eq!(native.step_back(), reference.step_back());
        for id in 4..804 {
            assert_eq!(native.state(id), reference.state(id));
        }
    }
    assert!(native.jit_stats().2 > 0);
}

#[cfg(feature = "jit")]
#[test]
fn background_jit_discards_stale_compilations_during_edits() {
    use std::time::{Duration, Instant};
    let mut native = Engine::new();
    let mut oracle = Engine::new();
    oracle.set_jit_enabled(false);
    for engine in [&mut native, &mut oracle] {
        for _ in 0..1600 {
            let id = engine.add_gate(8);
            engine.connect(id, id, 0, 1);
        }
    }
    for _ in 0..128 {
        native.tick();
        oracle.tick();
    }
    // There are outstanding jobs. Replace topology, then rewire again while
    // the next generation is compiling. Old native offsets must never publish.
    for engine in [&mut native, &mut oracle] {
        engine.remove_gate(4);
        let replacement = engine.add_gate(8);
        assert_eq!(replacement, 4);
        engine.connect(5, replacement, 0, 1);
        engine.compile();
    }
    let start = Instant::now();
    let mut ticks = 0;
    while native.jit_stats().0 < 7 {
        native.tick();
        oracle.tick();
        ticks += 1;
        if ticks == 128 {
            for engine in [&mut native, &mut oracle] {
                engine.connect(5, 4, 0, -1);
                engine.connect(6, 4, 0, 1);
                engine.compile();
            }
        }
        for id in [4, 5, 6, 259, 260, 1603] {
            assert_eq!(native.state(id), oracle.state(id));
        }
        assert!(
            start.elapsed() < Duration::from_secs(15),
            "background compilation did not finish"
        );
        std::thread::yield_now();
    }
    for _ in 0..100 {
        native.tick();
        oracle.tick();
        for id in 4..1604 {
            assert_eq!(native.state(id), oracle.state(id));
        }
    }
    assert!(native.jit_stats().2 > 0);
}

#[cfg(feature = "sparse-jit")]
#[test]
fn sparse_jit_dispatch_matches_the_interpreter_with_random_inputs_and_edits() {
    for realistic in [false, true] {
        let mut native = Engine::new();
        let mut oracle = Engine::new();
        oracle.set_jit_enabled(false);
        let mut rng = 12345u64;
        let mut random = || {
            rng = rng.wrapping_mul(6364136223846793005).wrapping_add(1);
            (rng >> 32) as u32
        };
        let kinds: Vec<_> = (0..640).map(|_| (random() % 6 + 1) as u16).collect();
        let inputs: Vec<Vec<u32>> = (0..640)
            .map(|_| (0..random() % 5).map(|_| random() % 320 + 4).collect())
            .collect();
        for engine in [&mut native, &mut oracle] {
            engine.set_realistic(realistic);
            for _ in 0..320 {
                engine.add_gate(16);
            }
            for (which, &kind) in kinds.iter().enumerate() {
                let id = engine.add_gate(kind);
                for &source in &inputs[which] {
                    assert!(engine.connect(source, id, 0, 1));
                }
            }
            engine.tick();
            engine.tick();
        }
        native.prepare_jit();
        assert_eq!(native.jit_stats().0, 3);
        for tick in 0..1200 {
            let source = random() % 320 + 4;
            let state = (random() % 4) as u8;
            if tick % 100 == 0 {
                let destination = random() % 640 + 324;
                for engine in [&mut native, &mut oracle] {
                    engine.connect(source, destination, 0, 1);
                    engine.compile();
                }
                native.prepare_jit();
            }
            let changes: Vec<_> = (0..8)
                .map(|_| (random() % 320 + 4, (random() % 4) as u8))
                .chain([(source, state)])
                .collect();
            for engine in [&mut native, &mut oracle] {
                for &(source, state) in &changes {
                    engine.set_state(source, state);
                }
                engine.tick();
            }
            for id in 324..964 {
                assert_eq!(
                    native.state(id),
                    oracle.state(id),
                    "tick={tick} id={id} realistic={realistic}"
                );
            }
        }
        assert!(
            native.jit_stats().2 > 1000,
            "exercise native sparse dispatch"
        );
    }
}
