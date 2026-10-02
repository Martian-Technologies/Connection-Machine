//! Tick-synchronous four-state simulation. Workers only read the committed state;
//! their results are committed together, then zero-delay junction nets are resolved.
//! The editor graph, execution program, live state and replay view have distinct owners.

use rayon::prelude::*;
use std::collections::{BTreeMap, BTreeSet, VecDeque};
use std::sync::Arc;

pub const LOW: u8 = 0;
pub const HIGH: u8 = 1;
pub const FLOATING: u8 = 2;
pub const UNDEFINED: u8 = 3;
const ABSENT: usize = usize::MAX;
const HISTORY_TICKS: usize = 1024;
const HISTORY_BYTES: usize = 64 * 1024 * 1024;
const PARALLEL_THRESHOLD: usize = 16_384;

#[cfg(feature = "jit")]
mod jit;

// Values match BlockType in the C++ adapter. Keep the ABI independent of Rust enums.
fn junction(kind: u16) -> bool {
    (9..=12).contains(&kind)
}
fn held(kind: u16) -> bool {
    kind == 14 || kind == 16 || (17..=20).contains(&kind)
}
fn initial(kind: u16, realistic: bool) -> u8 {
    match kind {
        9 | 19 => FLOATING,
        10 | 17 => LOW,
        11 | 18 => HIGH,
        12 | 20 => UNDEFINED,
        13 => {
            if realistic {
                UNDEFINED
            } else {
                FLOATING
            }
        }
        1..=8 => {
            if realistic {
                UNDEFINED
            } else {
                LOW
            }
        }
        _ => LOW,
    }
}

#[derive(Clone)]
struct Gate {
    kind: u16,
    inputs: BTreeMap<(u32, u32), u32>,
}

#[derive(Clone, Copy)]
struct Instruction {
    kind: u16,
    output: u32,
    start: usize,
    count: usize,
    data: u32,
    enable: u32,
}

impl Instruction {
    #[inline]
    fn evaluate(&self, states: &[u8], inputs: &[u32], realistic: bool) -> u8 {
        let value = match self.kind {
            7 | 8 => {
                let s = states[self.data as usize];
                (s ^ u8::from(self.kind == 8)) | (s >> 1)
            }
            13 => match states[self.enable as usize] {
                LOW => FLOATING,
                HIGH => {
                    if states[self.data as usize] >= FLOATING {
                        UNDEFINED
                    } else {
                        states[self.data as usize]
                    }
                }
                _ => UNDEFINED,
            },
            15 => LOW,
            1..=6 if self.count == 0 => LOW,
            1 | 2 | 4 | 5 => {
                let or = self.kind == 2 || self.kind == 5;
                let decisive = u8::from(or);
                let invert = self.kind == 4 || self.kind == 5;
                let mut result = decisive ^ 1;
                for &id in &inputs[self.start..self.start + self.count] {
                    let s = states[id as usize];
                    if s == decisive {
                        result = decisive;
                        break;
                    }
                    if s >= FLOATING {
                        result = UNDEFINED;
                    }
                }
                if result >= FLOATING {
                    UNDEFINED
                } else {
                    result ^ u8::from(invert)
                }
            }
            3 | 6 => {
                let mut result = u8::from(self.kind == 6);
                for &id in &inputs[self.start..self.start + self.count] {
                    let s = states[id as usize];
                    if s >= FLOATING {
                        result = UNDEFINED;
                        break;
                    }
                    result ^= s;
                }
                result
            }
            _ => UNDEFINED,
        };
        let old = states[self.output as usize];
        if realistic && self.kind != 15 && old != UNDEFINED && old != value {
            UNDEFINED
        } else {
            value
        }
    }
}

struct KernelGroup {
    kind: u16,
    start: usize,
    end: usize,
}

struct Net {
    members: Vec<u32>,
    drivers: Vec<u32>,
    fallback: u8,
}
impl Net {
    fn evaluate(&self, states: &[u8]) -> u8 {
        let mut result = FLOATING;
        for &driver in &self.drivers {
            let s = states[driver as usize];
            if s == FLOATING {
                continue;
            }
            if s == UNDEFINED || (result != FLOATING && result != s) {
                return UNDEFINED;
            }
            result = s;
        }
        if result == FLOATING {
            self.fallback
        } else {
            result
        }
    }
}

#[derive(Clone, Copy)]
struct Change {
    id: u32,
    before: u8,
    after: u8,
}
enum FrameData {
    Delta(Vec<Change>),
    Dense {
        before: Arc<Vec<u8>>,
        after: Arc<Vec<u8>>,
    },
}
struct Frame {
    data: FrameData,
    input: bool,
}
impl Frame {
    fn bytes(&self) -> usize {
        match &self.data {
            FrameData::Delta(d) => d.capacity() * std::mem::size_of::<Change>(),
            FrameData::Dense { before, after } => before.capacity() + after.capacity(),
        }
    }
    fn apply(&self, states: &mut Vec<u8>, forward: bool) {
        match &self.data {
            FrameData::Delta(d) if forward => {
                for c in d {
                    states[c.id as usize] = c.after;
                }
            }
            FrameData::Delta(d) => {
                for c in d.iter().rev() {
                    states[c.id as usize] = c.before;
                }
            }
            FrameData::Dense { before, after } => {
                states.clone_from(if forward { after } else { before })
            }
        }
    }
}

/// All mutation is serialized by the C++ control mutex. Rayon sees immutable
/// slices and disjoint output slices; it never mutates committed live state.
pub struct Engine {
    gates: Vec<Option<Gate>>,
    free_ids: Vec<u32>,
    dependents: Vec<BTreeSet<u32>>,
    pending_connections: BTreeMap<u32, BTreeSet<u32>>,
    live_input_count: usize,
    removed_consumer_marks: Vec<bool>,
    states: Arc<Vec<u8>>,
    spare_states: Vec<u8>,
    spare_changes: Vec<Change>,
    program: Vec<Instruction>,
    groups: Vec<KernelGroup>,
    unary: Vec<(u32, u32)>,
    inputs: Vec<u32>,
    instruction_of: Vec<usize>,
    nets: Vec<Net>,
    gate_dependencies: Vec<Vec<usize>>,
    net_dependencies: Vec<Vec<usize>>,
    active: Vec<usize>,
    spare_active: Vec<usize>,
    scheduled: Vec<bool>,
    dirty_nets: Vec<usize>,
    net_scheduled: Vec<bool>,
    results: Vec<u8>,
    dense_execution: bool,
    realistic: bool,
    needs_compile: bool,
    threads: usize,
    pool: Option<rayon::ThreadPool>,
    history: VecDeque<Frame>,
    history_bytes: usize,
    view: Option<(usize, Vec<u8>)>,
    pub ticks: u64,
    pub evaluations: u64,
    #[cfg(feature = "jit")]
    jit: jit::Cache,
}

impl Default for Engine {
    fn default() -> Self {
        Self::new()
    }
}
impl Engine {
    pub fn new() -> Self {
        Self {
            gates: vec![None; 4],
            free_ids: Vec::new(),
            dependents: vec![BTreeSet::new(); 4],
            pending_connections: BTreeMap::new(),
            live_input_count: 0,
            removed_consumer_marks: Vec::new(),
            states: Arc::new(vec![LOW, HIGH, FLOATING, UNDEFINED]),
            spare_states: Vec::new(),
            spare_changes: Vec::new(),
            program: Vec::new(),
            groups: Vec::new(),
            unary: Vec::new(),
            inputs: Vec::new(),
            instruction_of: Vec::new(),
            nets: Vec::new(),
            gate_dependencies: Vec::new(),
            net_dependencies: Vec::new(),
            active: Vec::new(),
            spare_active: Vec::new(),
            scheduled: Vec::new(),
            dirty_nets: Vec::new(),
            net_scheduled: Vec::new(),
            results: Vec::new(),
            dense_execution: false,
            realistic: false,
            needs_compile: false,
            threads: 1,
            pool: None,
            history: VecDeque::new(),
            history_bytes: 0,
            view: None,
            ticks: 0,
            evaluations: 0,
            #[cfg(feature = "jit")]
            jit: jit::Cache::default(),
        }
    }
    pub fn add_gate(&mut self, kind: u16) -> u32 {
        if !(1..=20).contains(&kind) {
            return 3;
        }
        self.invalidate_history();
        let id = self.free_ids.pop().unwrap_or(self.gates.len() as u32);
        if id as usize == self.gates.len() {
            self.gates.push(None);
            self.dependents.push(BTreeSet::new());
            Arc::make_mut(&mut self.states).push(UNDEFINED);
        }
        self.gates[id as usize] = Some(Gate {
            kind,
            inputs: BTreeMap::new(),
        });
        Arc::make_mut(&mut self.states)[id as usize] = initial(kind, self.realistic);
        self.needs_compile = true;
        id
    }
    pub fn remove_gate(&mut self, id: u32) -> bool {
        if id < 4
            || self
                .gates
                .get(id as usize)
                .and_then(Option::as_ref)
                .is_none()
        {
            return false;
        }
        self.invalidate_history();
        let removed = self.gates[id as usize].take().unwrap();
        for &(_, source) in removed.inputs.keys() {
            self.dependents[source as usize].remove(&id);
        }
        for destination in std::mem::take(&mut self.dependents[id as usize]) {
            if let Some(gate) = &mut self.gates[destination as usize] {
                gate.inputs.retain(|&(_, source), _| source != id);
            }
        }
        Arc::make_mut(&mut self.states)[id as usize] = UNDEFINED;
        self.free_ids.push(id);
        self.needs_compile = true;
        true
    }
    pub fn connect(&mut self, source: u32, destination: u32, port: u32, delta: i32) -> bool {
        if self
            .gates
            .get(source as usize)
            .and_then(Option::as_ref)
            .is_none()
        {
            return false;
        }
        let Some(gate) = self
            .gates
            .get_mut(destination as usize)
            .and_then(Option::as_mut)
        else {
            return false;
        };
        if held(gate.kind) || gate.kind == 15 {
            return false;
        }
        let old = gate.inputs.get(&(port, source)).copied().unwrap_or(0);
        let new = i64::from(old) + i64::from(delta);
        if !(0..=u32::MAX as i64).contains(&new) {
            return false;
        }
        if (gate.kind == 7 || gate.kind == 8 || gate.kind == 13)
            && new > 0
            && gate.inputs.keys().any(|&(p, s)| p == port && s != source)
        {
            return false;
        }
        // Preserve the old dependency set once per destination in this edit.
        // Gate/net membership changes still use the full compiler.
        if !self.needs_compile && !junction(gate.kind) {
            self.pending_connections
                .entry(destination)
                .or_insert_with(|| gate.inputs.keys().map(|&(_, source)| source).collect());
        } else {
            self.needs_compile = true;
        }
        if new == 0 {
            gate.inputs.remove(&(port, source));
            if !gate.inputs.keys().any(|&(_, s)| s == source) {
                self.dependents[source as usize].remove(&destination);
            }
        } else {
            gate.inputs.insert((port, source), new as u32);
            self.dependents[source as usize].insert(destination);
        }
        self.invalidate_history();
        true
    }
    fn invalidate_history(&mut self) {
        // Replay is a separate view: clearing it cannot roll back live state.
        self.view = None;
        self.history.clear();
        self.history_bytes = 0;
    }
    pub fn set_threads(&mut self, threads: usize) {
        let threads = threads.clamp(1, 64);
        if threads != self.threads {
            self.threads = threads;
            self.pool = None;
        }
    }
    pub fn set_realistic(&mut self, realistic: bool) {
        if realistic != self.realistic {
            self.realistic = realistic;
            #[cfg(feature = "jit")]
            self.jit.reset(self.program.len());
            self.invalidate_history();
            self.activate_all();
        }
    }
    fn activate_all(&mut self) {
        self.dense_execution = true;
        self.active.clear();
        self.scheduled.resize(self.program.len(), false);
        self.scheduled.fill(false);
    }
    pub fn compile(&mut self) {
        if !self.needs_compile {
            if self.pending_connections.is_empty() {
                return;
            }
            self.patch_connections();
            // Reclaim orphaned fan-in segments occasionally rather than
            // rebuilding the topology for every multi-input rewire.
            if self.inputs.len() <= self.live_input_count.saturating_mul(2) + 4096 {
                return;
            }
        }
        self.pending_connections.clear();
        self.program.clear();
        self.groups.clear();
        self.unary.clear();
        self.inputs.clear();
        self.nets.clear();
        self.instruction_of.resize(self.gates.len(), ABSENT);
        self.instruction_of.fill(ABSENT);
        // Merge only zero-delay junctions. Logic gates remain tick boundaries,
        // including gates within a group, so grouping cannot alter propagation delay.
        let mut parent: Vec<usize> = (0..self.gates.len()).collect();
        fn root(parent: &mut [usize], mut n: usize) -> usize {
            while parent[n] != n {
                parent[n] = parent[parent[n]];
                n = parent[n];
            }
            n
        }
        for (id, gate) in self.gates.iter().enumerate() {
            let Some(gate) = gate else {
                continue;
            };
            if !junction(gate.kind) {
                continue;
            }
            for &(_, source) in gate.inputs.keys() {
                if self.gates[source as usize]
                    .as_ref()
                    .is_some_and(|g| junction(g.kind))
                {
                    let a = root(&mut parent, id);
                    let b = root(&mut parent, source as usize);
                    parent[b] = a;
                }
            }
        }
        let mut net_members: BTreeMap<usize, Vec<u32>> = BTreeMap::new();
        let mut execution_order = Vec::new();
        for (id, gate) in self.gates.iter().enumerate() {
            let Some(gate) = gate else {
                continue;
            };
            if junction(gate.kind) {
                net_members
                    .entry(root(&mut parent, id))
                    .or_default()
                    .push(id as u32);
            } else if !held(gate.kind) {
                execution_order.push((gate.kind, id));
            }
        }
        execution_order.sort_unstable();
        for (kind, id) in execution_order {
            let instruction = self.build_instruction(id as u32, kind);
            self.instruction_of[id] = self.program.len();
            self.program.push(instruction);
        }
        let mut start = 0;
        while start < self.program.len() {
            let kind = self.program[start].kind;
            let mut end = start + 1;
            while end < self.program.len() && self.program[end].kind == kind {
                end += 1;
            }
            self.groups.push(KernelGroup { kind, start, end });
            start = end;
        }
        self.unary
            .extend(self.program.iter().map(|i| (i.output, i.data)));
        for members in net_members.into_values() {
            let mut drivers = BTreeSet::new();
            let mut fallback = FLOATING;
            for &id in &members {
                let gate = self.gates[id as usize].as_ref().unwrap();
                let pull = initial(gate.kind, false);
                if pull != FLOATING {
                    fallback = if fallback == FLOATING || fallback == pull {
                        pull
                    } else {
                        UNDEFINED
                    };
                }
                for &(_, source) in gate.inputs.keys() {
                    if !junction(self.gates[source as usize].as_ref().unwrap().kind) {
                        drivers.insert(source);
                    }
                }
            }
            self.nets.push(Net {
                members,
                drivers: drivers.into_iter().collect(),
                fallback,
            });
        }
        self.gate_dependencies
            .resize_with(self.gates.len(), Vec::new);
        self.net_dependencies
            .resize_with(self.gates.len(), Vec::new);
        for dependencies in &mut self.gate_dependencies {
            dependencies.clear();
        }
        for dependencies in &mut self.net_dependencies {
            dependencies.clear();
        }
        for (index, instruction) in self.program.iter().enumerate() {
            let gate = self.gates[instruction.output as usize].as_ref().unwrap();
            for &(_, source) in gate.inputs.keys() {
                self.gate_dependencies[source as usize].push(index);
            }
        }
        for (index, net) in self.nets.iter().enumerate() {
            for &driver in &net.drivers {
                self.net_dependencies[driver as usize].push(index);
            }
        }
        self.live_input_count = self.inputs.len();
        // Reuse small adjacency allocations, but release capacities left by
        // a removed large fanout. Interactive edits must not retain every peak.
        for dependencies in self
            .gate_dependencies
            .iter_mut()
            .chain(&mut self.net_dependencies)
        {
            if dependencies.capacity() > dependencies.len().saturating_mul(4).max(16) {
                dependencies.shrink_to_fit();
            }
        }
        self.removed_consumer_marks
            .resize(self.program.len(), false);
        self.removed_consumer_marks.fill(false);
        self.activate_all();
        self.dirty_nets = (0..self.nets.len()).collect();
        self.net_scheduled = vec![true; self.nets.len()];
        self.needs_compile = false;
        #[cfg(feature = "jit")]
        self.jit.reset(self.program.len());
        let mut ignored = Vec::new();
        self.resolve_nets(&mut ignored);
    }
    fn build_instruction(&mut self, id: u32, kind: u16) -> Instruction {
        let gate = self.gates[id as usize].as_ref().unwrap();
        let start = self.inputs.len();
        let mut data = if kind == 13 { 0 } else { 3 };
        let mut enable = 0;
        for (&(port, source), &weight) in &gate.inputs {
            if kind == 13 {
                if port == 0 {
                    data = source;
                } else {
                    enable = source;
                }
            } else if kind == 7 || kind == 8 {
                data = source;
            } else if kind != 15 {
                self.inputs
                    .extend(std::iter::repeat_n(source, weight as usize));
            }
        }
        Instruction {
            kind,
            output: id,
            start,
            count: self.inputs.len() - start,
            data,
            enable,
        }
    }
    fn patch_connections(&mut self) {
        let mut removed: BTreeMap<u32, Vec<usize>> = BTreeMap::new();
        let mut added: BTreeMap<u32, Vec<usize>> = BTreeMap::new();
        for (id, old_sources) in std::mem::take(&mut self.pending_connections) {
            let index = self.instruction_of[id as usize];
            #[cfg(feature = "jit")]
            self.jit.invalidate(index);
            let gate = self.gates[id as usize].as_ref().unwrap();
            let kind = gate.kind;
            let new_sources: BTreeSet<_> = gate.inputs.keys().map(|&(_, source)| source).collect();
            for &source in old_sources.difference(&new_sources) {
                removed.entry(source).or_default().push(index);
            }
            for &source in new_sources.difference(&old_sources) {
                added.entry(source).or_default().push(index);
            }
            let instruction = self.build_instruction(id, kind);
            self.live_input_count =
                self.live_input_count - self.program[index].count + instruction.count;
            self.program[index] = instruction;
            self.unary[index] = (id, instruction.data);
            self.schedule(index);
        }
        // Scan each edited driver's fanout once per transaction, even when
        // hundreds of its consumers were rewired together.
        for (source, consumers) in removed {
            if consumers.len() == 1 {
                self.gate_dependencies[source as usize].retain(|&index| index != consumers[0]);
            } else {
                for &index in &consumers {
                    self.removed_consumer_marks[index] = true;
                }
                let marks = &self.removed_consumer_marks;
                self.gate_dependencies[source as usize].retain(|&index| !marks[index]);
                for &index in &consumers {
                    self.removed_consumer_marks[index] = false;
                }
            }
        }
        for (source, consumers) in added {
            self.gate_dependencies[source as usize].extend(consumers);
        }
    }
    fn schedule(&mut self, instruction: usize) {
        if !self.scheduled[instruction] {
            self.scheduled[instruction] = true;
            self.active.push(instruction);
        }
    }
    fn changed(&mut self, id: usize) {
        for i in 0..self.gate_dependencies[id].len() {
            self.schedule(self.gate_dependencies[id][i]);
        }
        for &net in &self.net_dependencies[id] {
            if !self.net_scheduled[net] {
                self.net_scheduled[net] = true;
                self.dirty_nets.push(net);
            }
        }
    }
    fn commit(&mut self, id: u32, value: u8, changes: &mut Vec<Change>) {
        let before = self.states[id as usize];
        if before != value {
            changes.push(Change {
                id,
                before,
                after: value,
            });
            Arc::make_mut(&mut self.states)[id as usize] = value;
            self.changed(id as usize);
        }
    }
    fn resolve_nets(&mut self, changes: &mut Vec<Change>) {
        while let Some(index) = self.dirty_nets.pop() {
            self.net_scheduled[index] = false;
            let value = self.nets[index].evaluate(&self.states);
            for i in 0..self.nets[index].members.len() {
                let id = self.nets[index].members[i];
                self.commit(id, value, changes);
            }
        }
    }
    pub fn reset(&mut self) {
        self.compile();
        self.invalidate_history();
        self.ticks = 0;
        let states = Arc::make_mut(&mut self.states);
        for (id, gate) in self.gates.iter().enumerate() {
            if let Some(gate) = gate {
                states[id] = initial(gate.kind, self.realistic);
            }
        }
        self.activate_all();
        self.dirty_nets = (0..self.nets.len()).collect();
        self.net_scheduled.fill(true);
        let mut ignored = Vec::new();
        self.resolve_nets(&mut ignored);
    }
    pub fn set_state(&mut self, id: u32, value: u8) {
        self.compile();
        self.view = None;
        let Some(gate) = self.gates.get(id as usize).and_then(Option::as_ref) else {
            return;
        };
        if value > UNDEFINED || junction(gate.kind) || (17..=20).contains(&gate.kind) {
            return;
        }
        let mut changes = Vec::new();
        self.commit(id, value, &mut changes);
        let instruction = self.instruction_of[id as usize];
        if instruction != ABSENT {
            self.schedule(instruction);
        }
        self.resolve_nets(&mut changes);
        if let Some(frame) = self.history.back_mut() {
            self.history_bytes -= frame.bytes();
            frame.input = true;
            match &mut frame.data {
                FrameData::Delta(d) => d.extend(changes),
                FrameData::Dense { after, .. } => *after = Arc::clone(&self.states),
            }
            self.history_bytes += frame.bytes();
            self.trim_history();
        }
    }
    pub fn state(&self, id: u32) -> u8 {
        let states = self.view.as_ref().map_or(self.states.as_ref(), |(_, s)| s);
        states.get(id as usize).copied().unwrap_or(UNDEFINED)
    }
    pub fn tick(&mut self) {
        self.compile();
        self.view = None;
        if self.dense_execution || self.active.len() > self.program.len() / 4 {
            self.tick_dense();
            return;
        }
        if self.active.is_empty() && self.dirty_nets.is_empty() {
            self.history.push_back(Frame {
                data: FrameData::Delta(Vec::new()),
                input: false,
            });
            self.trim_history();
            self.ticks += 1;
            return;
        }
        let mut active = std::mem::take(&mut self.spare_active);
        std::mem::swap(&mut active, &mut self.active);
        #[cfg(feature = "sparse-jit")]
        self.jit.poll(
            &self.program,
            &self.inputs,
            self.states.len(),
            self.realistic,
            Some(&active),
        );
        for &index in &active {
            self.scheduled[index] = false;
        }
        self.evaluations += active.len() as u64;
        let dense = active.len() * std::mem::size_of::<Change>() >= self.states.len() * 2;
        let before = if dense {
            Some(Arc::clone(&self.states))
        } else {
            None
        };
        self.results.resize(active.len(), LOW);
        if active.len() >= PARALLEL_THRESHOLD && self.threads > 1 {
            if self.pool.is_none() {
                // Thread creation failures fall back to the scalar engine.
                self.pool = rayon::ThreadPoolBuilder::new()
                    .num_threads(self.threads)
                    .build()
                    .ok();
            }
            if let Some(pool) = &self.pool {
                let program = &self.program;
                let states = &self.states;
                let inputs = &self.inputs;
                let realistic = self.realistic;
                pool.install(|| {
                    self.results
                        .par_iter_mut()
                        .zip(active.par_iter())
                        .with_min_len(256)
                        .for_each(|(result, &index)| {
                            *result = program[index].evaluate(states, inputs, realistic)
                        })
                });
            } else {
                for (result, &index) in self.results.iter_mut().zip(&active) {
                    *result =
                        self.program[index].evaluate(&self.states, &self.inputs, self.realistic);
                }
            }
        } else {
            #[cfg(feature = "sparse-jit")]
            let native = self.jit.has_code() && active.len() >= 8;
            #[cfg(not(feature = "sparse-jit"))]
            let native = false;
            if native {
                active.sort_unstable();
                #[cfg(feature = "sparse-jit")]
                {
                    let mut start = 0;
                    while start < active.len() {
                        let chunk = active[start] / jit::CHUNK;
                        let mut end = start + 1;
                        while end < active.len() && active[end] / jit::CHUNK == chunk {
                            end += 1;
                        }
                        let complex = active[start..end]
                            .iter()
                            .any(|&i| !matches!(self.program[i].kind, 7 | 8 | 15));
                        if !complex
                            || !self.jit.evaluate(
                                chunk,
                                &self.states,
                                &active[start..end],
                                &mut self.results[start..end],
                            )
                        {
                            for (result, &index) in
                                self.results[start..end].iter_mut().zip(&active[start..end])
                            {
                                *result = self.program[index].evaluate(
                                    &self.states,
                                    &self.inputs,
                                    self.realistic,
                                );
                            }
                        }
                        start = end;
                    }
                }
            } else {
                for (result, &index) in self.results.iter_mut().zip(&active) {
                    *result =
                        self.program[index].evaluate(&self.states, &self.inputs, self.realistic);
                }
            }
        }
        let mut changes = std::mem::take(&mut self.spare_changes);
        changes.clear();
        if active
            .iter()
            .zip(&self.results)
            .any(|(&i, &value)| self.states[self.program[i].output as usize] != value)
        {
            // Acquire mutable state once per publication, rather than checking
            // Arc ownership for every changed gate. Evaluation is complete.
            let states = Arc::make_mut(&mut self.states);
            for (&index, &value) in active.iter().zip(&self.results) {
                let id = self.program[index].output;
                let before = states[id as usize];
                if before != value {
                    changes.push(Change {
                        id,
                        before,
                        after: value,
                    });
                    states[id as usize] = value;
                }
            }
            for change in &changes {
                self.changed(change.id as usize);
                if self.realistic && change.after == UNDEFINED {
                    self.schedule(self.instruction_of[change.id as usize]);
                }
            }
        }
        self.resolve_nets(&mut changes);
        // Reuse the active-list allocation after the next list has been built.
        self.spare_active = active;
        self.spare_active.clear();
        let data = if changes.is_empty() {
            self.spare_changes = changes;
            FrameData::Delta(Vec::new())
        } else if let Some(before) = before {
            FrameData::Dense {
                before,
                after: Arc::clone(&self.states),
            }
        } else {
            FrameData::Delta(changes)
        };
        let frame = Frame { data, input: false };
        self.history_bytes += frame.bytes();
        self.history.push_back(frame);
        self.trim_history();
        self.ticks += 1;
    }
    fn tick_dense(&mut self) {
        // Avoid maintaining a work queue when most gates change. All kernels
        // read the same state, and publication is still one atomic tick boundary.
        self.active.clear();
        self.scheduled.fill(false);
        self.dirty_nets.clear();
        self.net_scheduled.fill(false);
        self.evaluations += self.program.len() as u64;
        let before = Arc::clone(&self.states);
        let mut next = std::mem::take(&mut self.spare_states);
        next.clone_from(&self.states);
        #[cfg(feature = "jit")]
        self.jit.poll(
            &self.program,
            &self.inputs,
            self.states.len(),
            self.realistic,
            None,
        );
        // Dispatch overhead dominates inexpensive kernels below this size.
        #[cfg(feature = "jit")]
        let has_jit = self.jit.has_code();
        #[cfg(not(feature = "jit"))]
        let has_jit = false;
        let parallel =
            self.program.len() >= (if has_jit { 524_288 } else { 131_072 }) && self.threads > 1;
        if parallel && self.pool.is_none() {
            let count = self.threads.min(self.program.len() / 4096).max(1);
            self.pool = rayon::ThreadPoolBuilder::new()
                .num_threads(count)
                .build()
                .ok();
        }
        #[cfg(feature = "jit")]
        let native_parallel = parallel
            && has_jit
            && self
                .pool
                .as_ref()
                .is_some_and(|pool| self.jit.run_parallel(pool, &self.states, &mut next));
        #[cfg(not(feature = "jit"))]
        let native_parallel = false;
        if native_parallel {
            // Native kernels wrote only their verified disjoint partitions.
        } else if let Some(pool) = self.pool.as_ref().filter(|_| parallel && !has_jit) {
            let states = &self.states;
            let inputs = &self.inputs;
            let realistic = self.realistic;
            let program = &self.program;
            let instruction_of = &self.instruction_of;
            let unary = &self.unary;
            let unary_only = !realistic
                && self.groups.len() == 1
                && (self.groups[0].kind == 7 || self.groups[0].kind == 8);
            if unary_only {
                let invert = u8::from(self.groups[0].kind == 8);
                let first = unary[0].0 as usize;
                let contiguous = unary.last().unwrap().0 as usize - first + 1 == unary.len();
                if contiguous {
                    pool.install(|| {
                        next[first..first + unary.len()]
                            .par_iter_mut()
                            .zip(unary.par_iter())
                            .with_min_len(4096)
                            .for_each(|(output, &(_, input))| {
                                let s = states[input as usize];
                                *output = (s ^ invert) | (s >> 1);
                            })
                    });
                } else {
                    pool.install(|| {
                        next.par_chunks_mut(1024)
                            .enumerate()
                            .for_each(|(chunk, outputs)| {
                                for (i, output) in outputs.iter_mut().enumerate() {
                                    let index = instruction_of[chunk * 1024 + i];
                                    if index != ABSENT {
                                        let s = states[unary[index].1 as usize];
                                        *output = (s ^ invert) | (s >> 1);
                                    }
                                }
                            })
                    });
                }
            } else {
                pool.install(|| {
                    next.par_chunks_mut(1024)
                        .enumerate()
                        .for_each(|(chunk, outputs)| {
                            for (i, output) in outputs.iter_mut().enumerate() {
                                let index = instruction_of[chunk * 1024 + i];
                                if index != ABSENT {
                                    *output = program[index].evaluate(states, inputs, realistic);
                                }
                            }
                        })
                });
            }
        } else {
            if has_jit {
                #[cfg(feature = "jit")]
                for (chunk, ops) in self.program.chunks(jit::CHUNK).enumerate() {
                    if let Some(code) = self.jit.code(chunk) {
                        code.run(&self.states, &mut next);
                        self.jit.hits += 1;
                    } else {
                        if !self.realistic
                            && matches!(ops[0].kind, 7 | 8)
                            && ops[0].kind == ops.last().unwrap().kind
                        {
                            let invert = u8::from(ops[0].kind == 8);
                            for instruction in ops {
                                let s = self.states[instruction.data as usize];
                                next[instruction.output as usize] = (s ^ invert) | (s >> 1);
                            }
                        } else {
                            for instruction in ops {
                                next[instruction.output as usize] = instruction.evaluate(
                                    &self.states,
                                    &self.inputs,
                                    self.realistic,
                                );
                            }
                        }
                    }
                }
            } else {
                for group in &self.groups {
                    if !self.realistic && (group.kind == 7 || group.kind == 8) {
                        let invert = u8::from(group.kind == 8);
                        let ops = &self.unary[group.start..group.end];
                        let first = ops[0].0 as usize;
                        if ops.last().unwrap().0 as usize - first + 1 == ops.len() {
                            for (output, &(_, input)) in
                                next[first..first + ops.len()].iter_mut().zip(ops)
                            {
                                let s = self.states[input as usize];
                                *output = (s ^ invert) | (s >> 1);
                            }
                        } else {
                            for &(output, input) in ops {
                                let s = self.states[input as usize];
                                next[output as usize] = (s ^ invert) | (s >> 1);
                            }
                        }
                    } else {
                        for instruction in &self.program[group.start..group.end] {
                            next[instruction.output as usize] =
                                instruction.evaluate(&self.states, &self.inputs, self.realistic);
                        }
                    }
                }
            }
        }
        for net in &self.nets {
            let value = net.evaluate(&next);
            for &id in &net.members {
                next[id as usize] = value;
            }
        }
        self.states = Arc::new(next);
        // Only the density threshold and an empty/nonempty result are needed.
        // Stop scanning once dense execution is guaranteed for the next tick.
        let threshold = self.program.len() / 8;
        let mut changed = 0;
        for (old, new) in before.chunks(64).zip(self.states.chunks(64)) {
            let (a, a_tail) = old.as_chunks::<8>();
            let (b, b_tail) = new.as_chunks::<8>();
            for (&a, &b) in a.iter().zip(b) {
                // Four-state bytes have only two bits: fold both into a
                // single bit per byte, then count eight changes together.
                let diff = u64::from_le_bytes(a) ^ u64::from_le_bytes(b);
                changed += ((diff | (diff >> 1)) & 0x0101_0101_0101_0101).count_ones() as usize;
            }
            changed += a_tail.iter().zip(b_tail).filter(|(a, b)| a != b).count();
            if changed > threshold {
                break;
            }
        }
        self.dense_execution = changed > self.program.len() / 8;
        if !self.dense_execution {
            for id in 4..self.states.len() {
                if before[id] != self.states[id] {
                    self.changed(id);
                    if self.realistic && self.states[id] == UNDEFINED {
                        let instruction = self.instruction_of[id];
                        if instruction != ABSENT {
                            self.schedule(instruction);
                        }
                    }
                }
            }
        }
        // Zero-delay nets were resolved above; scheduling their consumer gates
        // is sufficient. No extra junction evaluation is needed next tick.
        self.dirty_nets.clear();
        self.net_scheduled.fill(false);
        let data = if changed == 0 {
            FrameData::Delta(Vec::new())
        } else {
            FrameData::Dense {
                before,
                after: Arc::clone(&self.states),
            }
        };
        let frame = Frame { data, input: false };
        self.history_bytes += frame.bytes();
        self.history.push_back(frame);
        self.trim_history();
        self.ticks += 1;
    }
    fn trim_history(&mut self) {
        while self.history.len() > HISTORY_TICKS || self.history_bytes > HISTORY_BYTES {
            let expired = self.history.pop_front().unwrap();
            self.history_bytes -= expired.bytes();
            match expired.data {
                FrameData::Delta(changes) => {
                    if changes.capacity() >= self.spare_changes.capacity() {
                        self.spare_changes = changes;
                    }
                }
                FrameData::Dense { before, .. } => {
                    // The oldest snapshot normally has a single owner. Recycle
                    // its buffer only after ownership proves replay no longer
                    // needs it; every recorded tick still has its actual state.
                    if let Ok(states) = Arc::try_unwrap(before) {
                        self.spare_states = states;
                    }
                }
            }
        }
    }
    pub fn step_back(&mut self) -> bool {
        let offset = self.view.as_ref().map_or(0, |(offset, _)| *offset);
        if offset >= self.history.len() {
            return false;
        }
        let (_, mut states) = self
            .view
            .take()
            .unwrap_or_else(|| (0, self.states.as_ref().clone()));
        self.history[self.history.len() - offset - 1].apply(&mut states, false);
        self.view = Some((offset + 1, states));
        true
    }
    pub fn step_forward(&mut self) -> bool {
        let Some((offset, mut states)) = self.view.take() else {
            return false;
        };
        if offset == 1 {
            return true;
        }
        self.history[self.history.len() - offset].apply(&mut states, true);
        self.view = Some((offset - 1, states));
        true
    }
    pub fn skip_back(&mut self) -> bool {
        let mut moved = false;
        while self.step_back() {
            moved = true;
            let offset = self.view.as_ref().unwrap().0;
            if offset == self.history.len() || self.history[self.history.len() - offset - 1].input {
                break;
            }
        }
        moved
    }
    pub fn skip_forward(&mut self) -> bool {
        let mut moved = false;
        while self.step_forward() {
            moved = true;
            let Some((offset, _)) = &self.view else {
                break;
            };
            if self.history[self.history.len() - offset - 1].input {
                break;
            }
        }
        moved
    }
    pub fn return_to_live(&mut self) {
        self.view = None;
    }
    pub fn is_viewing_replay(&self) -> bool {
        self.view.is_some()
    }
    pub fn replay_bytes(&self) -> usize {
        self.history_bytes
    }

    /// Disable native code for differential tests or interpreter-only callers.
    pub fn set_jit_enabled(&mut self, enabled: bool) {
        #[cfg(feature = "jit")]
        {
            self.jit.enabled = enabled;
        }
        #[cfg(not(feature = "jit"))]
        let _ = enabled;
    }

    /// Optional eager preparation for offline use. The app normally compiles
    /// hot dense groups in the background, without blocking edit publication.
    pub fn prepare_jit(&mut self) {
        self.compile();
        #[cfg(feature = "jit")]
        self.jit.prepare(
            &self.program,
            &self.inputs,
            self.states.len(),
            self.realistic,
        );
    }

    pub fn jit_stats(&self) -> (usize, usize, u64) {
        #[cfg(feature = "jit")]
        {
            self.jit.stats()
        }
        #[cfg(not(feature = "jit"))]
        {
            (0, 0, 0)
        }
    }
}

mod ffi;
