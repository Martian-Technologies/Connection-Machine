//! Trusted circuit IR only. Guest Wasm must use Wasmtime instead. Code owns its
//! executable memory; calls borrow that owner and cannot outlive it.
use super::Instruction;
use cranelift_codegen::ir::{
    condcodes::IntCC, types, AbiParam, InstBuilder, MemFlags, Type, Value,
};
use cranelift_codegen::settings::{self, Configurable};
#[cfg(feature = "sparse-jit")]
use cranelift_frontend::Switch;
use cranelift_frontend::{FunctionBuilder, FunctionBuilderContext};
use cranelift_jit::{JITBuilder, JITModule};
use cranelift_module::{default_libcall_names, Linkage, Module};
use rayon::prelude::*;
use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{mpsc, Arc};
use std::thread::JoinHandle;

pub(super) const CHUNK: usize = 256;
const WARM_TICKS: u64 = 64;
const MAX_INPUTS: usize = 4096;

struct Snapshot {
    ops: Vec<Instruction>,
    inputs: Vec<u32>,
    states_len: usize,
    realistic: bool,
}

impl Snapshot {
    fn new(
        ops: &[Instruction],
        inputs: &[u32],
        states_len: usize,
        realistic: bool,
    ) -> Option<Self> {
        if states_len > i32::MAX as usize || ops.iter().map(|i| i.count).sum::<usize>() > MAX_INPUTS
        {
            return None;
        }
        let mut flat = Vec::new();
        let ops = ops
            .iter()
            .map(|op| {
                let mut op = *op;
                let start = flat.len();
                flat.extend_from_slice(&inputs[op.start..op.start + op.count]);
                op.start = start;
                op
            })
            .collect();
        Some(Self {
            ops,
            inputs: flat,
            states_len,
            realistic,
        })
    }
}

struct OwnedCode {
    module: Option<JITModule>,
    tick: unsafe extern "C" fn(*const u8, *mut u8),
    #[cfg(feature = "sparse-jit")]
    evaluate: unsafe extern "C" fn(*const u8, *const usize, *mut u8, usize),
    states_len: usize,
    first_output: usize,
    end_output: usize,
    pub bytes: usize,
}
impl Drop for OwnedCode {
    fn drop(&mut self) {
        if let Some(module) = self.module.take() {
            // SAFETY: the compiler removes an allocation only when the last
            // code lease is released, or after its cache stops all execution.
            unsafe {
                module.free_memory();
            }
        }
    }
}
impl OwnedCode {
    fn compile(s: Snapshot) -> Result<Self, String> {
        // Validate every baked offset before generating native loads/stores.
        if s.ops.iter().any(|i| {
            i.output as usize >= s.states_len
                || i.data as usize >= s.states_len
                || i.enable as usize >= s.states_len
        }) || s.inputs.iter().any(|&i| i as usize >= s.states_len)
        {
            return Err("invalid circuit offsets".into());
        }
        let mut flags = settings::builder();
        let first_output = s.ops.iter().map(|i| i.output as usize).min().unwrap();
        let end_output = s.ops.iter().map(|i| i.output as usize).max().unwrap() + 1;
        flags.set("opt_level", "speed").map_err(|e| e.to_string())?;
        flags.set("is_pic", "false").map_err(|e| e.to_string())?;
        let isa = cranelift_native::builder()
            .map_err(|e| e.to_string())?
            .finish(settings::Flags::new(flags))
            .map_err(|e| e.to_string())?;
        let mut module = JITModule::new(JITBuilder::with_isa(isa, default_libcall_names()));
        let mut ctx = module.make_context();
        let ptr = module.target_config().pointer_type();
        ctx.func
            .signature
            .params
            .extend([AbiParam::new(ptr), AbiParam::new(ptr)]);
        let id = module
            .declare_function("tick", Linkage::Local, &ctx.func.signature)
            .map_err(|e| e.to_string())?;
        let mut fb = FunctionBuilderContext::new();
        {
            let mut b = FunctionBuilder::new(&mut ctx.func, &mut fb);
            let block = b.create_block();
            b.switch_to_block(block);
            b.append_block_params_for_function_params(block);
            let old = b.block_params(block)[0];
            let next = b.block_params(block)[1];
            let mut memo = HashMap::new();
            let mut pos = 0;
            while pos < s.ops.len() {
                let vector =
                    pos + 16 <= s.ops.len() && vectorizable(&s.ops[pos..pos + 16], &s.inputs);
                let lanes = if vector { 16 } else { 1 };
                let ty = if vector { types::I8X16 } else { types::I8 };
                let i = s.ops[pos];
                let sources: Vec<u32> = match i.kind {
                    7 | 8 => vec![i.data],
                    13 => vec![i.data, i.enable],
                    _ => s.inputs[i.start..i.start + i.count].to_vec(),
                };
                let mut values = Vec::with_capacity(sources.len());
                for &source in &sources {
                    let v = *memo.entry((source, lanes)).or_insert_with(|| {
                        b.ins().load(
                            ty,
                            MemFlags::new().with_notrap().with_readonly(),
                            old,
                            source as i32,
                        )
                    });
                    values.push(v);
                }
                let value = emit_gate(&mut b, ty, i.kind, &values);
                let value = if s.realistic && i.kind != 15 {
                    let previous =
                        b.ins()
                            .load(ty, MemFlags::new().with_notrap(), old, i.output as i32);
                    let changed = b.ins().icmp(IntCC::NotEqual, previous, value);
                    let x = constant(&mut b, ty, 3);
                    let known = b.ins().icmp(IntCC::NotEqual, previous, x);
                    let transition = b.ins().band(changed, known);
                    select(&mut b, ty, transition, x, value)
                } else {
                    value
                };
                b.ins().store(
                    MemFlags::new().with_notrap(),
                    value,
                    next,
                    (i.output as usize - first_output) as i32,
                );
                pos += lanes;
            }
            b.ins().return_(&[]);
            b.seal_all_blocks();
            b.finalize();
        }
        if let Err(e) = module.define_function(id, &mut ctx) {
            // No pointer has been published, including on an error path.
            unsafe {
                module.free_memory();
            }
            return Err(e.to_string());
        }
        let dense_bytes = ctx.compiled_code().unwrap().code_buffer().len();
        #[cfg(feature = "sparse-jit")]
        let (eval_id, bytes) = {
            module.clear_context(&mut ctx);
            ctx.func.signature.params.extend([
                AbiParam::new(ptr),
                AbiParam::new(ptr),
                AbiParam::new(ptr),
                AbiParam::new(ptr),
            ]);
            let eval_id = module
                .declare_function("evaluate", Linkage::Local, &ctx.func.signature)
                .map_err(|e| e.to_string())?;
            {
                let mut b = FunctionBuilder::new(&mut ctx.func, &mut fb);
                let entry = b.create_block();
                b.switch_to_block(entry);
                b.append_block_params_for_function_params(entry);
                let old = b.block_params(entry)[0];
                let indices = b.block_params(entry)[1];
                let results = b.block_params(entry)[2];
                let count = b.block_params(entry)[3];
                let head = b.create_block();
                b.append_block_param(head, ptr);
                let dispatch = b.create_block();
                let finish = b.create_block();
                b.append_block_param(finish, types::I8);
                let exit = b.create_block();
                let zero = b.ins().iconst(ptr, 0);
                b.ins().jump(head, &[zero.into()]);
                b.switch_to_block(head);
                let position = b.block_params(head)[0];
                let more = b.ins().icmp(IntCC::UnsignedLessThan, position, count);
                b.ins().brif(more, dispatch, &[], exit, &[]);
                b.switch_to_block(exit);
                b.ins().return_(&[]);
                b.switch_to_block(dispatch);
                let offset = b
                    .ins()
                    .imul_imm(position, std::mem::size_of::<usize>() as i64);
                let address = b.ins().iadd(indices, offset);
                let global = b.ins().load(ptr, MemFlags::new().with_notrap(), address, 0);
                let index = b.ins().band_imm(global, (CHUNK - 1) as i64);
                let default = b.create_block();
                // Cheap unary gates keep the inline interpreter on the sparse
                // path. Avoid generating an unused dispatch body for them.
                let cases: Vec<_> = s
                    .ops
                    .iter()
                    .enumerate()
                    .filter(|_| s.ops.iter().any(|i| !matches!(i.kind, 7 | 8 | 15)))
                    .map(|(i, _)| (i, b.create_block()))
                    .collect();
                let mut switch = Switch::new();
                for &(i, block) in &cases {
                    switch.set_entry(i as u128, block);
                }
                switch.emit(&mut b, index, default);
                b.switch_to_block(default);
                let x = b.ins().iconst(types::I8, 3);
                b.ins().jump(finish, &[x.into()]);
                for &(index, block) in &cases {
                    let i = &s.ops[index];
                    b.switch_to_block(block);
                    let sources: Vec<u32> = match i.kind {
                        7 | 8 => vec![i.data],
                        13 => vec![i.data, i.enable],
                        _ => s.inputs[i.start..i.start + i.count].to_vec(),
                    };
                    let short_circuit = matches!(i.kind, 1 | 2 | 4 | 5) && i.count > 1;
                    let values: Vec<_> = sources
                        .iter()
                        .filter(|_| !short_circuit)
                        .map(|&source| {
                            b.ins().load(
                                types::I8,
                                MemFlags::new().with_notrap(),
                                old,
                                source as i32,
                            )
                        })
                        .collect();
                    let value = if short_circuit {
                        emit_short_circuit(&mut b, old, i.kind, &sources)
                    } else {
                        emit_gate(&mut b, types::I8, i.kind, &values)
                    };
                    let value = if s.realistic && i.kind != 15 {
                        let previous = b.ins().load(
                            types::I8,
                            MemFlags::new().with_notrap(),
                            old,
                            i.output as i32,
                        );
                        let changed = b.ins().icmp(IntCC::NotEqual, previous, value);
                        let x = b.ins().iconst(types::I8, 3);
                        let known = b.ins().icmp(IntCC::NotEqual, previous, x);
                        let transition = b.ins().band(changed, known);
                        b.ins().select(transition, x, value)
                    } else {
                        value
                    };
                    b.ins().jump(finish, &[value.into()]);
                }
                b.switch_to_block(finish);
                let value = b.block_params(finish)[0];
                let address = b.ins().iadd(results, position);
                b.ins()
                    .store(MemFlags::new().with_notrap(), value, address, 0);
                let position = b.ins().iadd_imm(position, 1);
                b.ins().jump(head, &[position.into()]);
                b.seal_all_blocks();
                b.finalize();
            }
            if let Err(e) = module.define_function(eval_id, &mut ctx) {
                unsafe {
                    module.free_memory();
                }
                return Err(e.to_string());
            }
            (
                eval_id,
                dense_bytes + ctx.compiled_code().unwrap().code_buffer().len(),
            )
        };
        #[cfg(not(feature = "sparse-jit"))]
        let bytes = dense_bytes;
        if let Err(e) = module.finalize_definitions() {
            unsafe {
                module.free_memory();
            }
            return Err(e.to_string());
        }
        // SAFETY: this is the exact generated C ABI. The compiler retains the
        // owning module while a published lease exists. run validates buffers.
        let tick = unsafe {
            std::mem::transmute::<*const u8, unsafe extern "C" fn(*const u8, *mut u8)>(
                module.get_finalized_function(id),
            )
        };
        // SAFETY: matches the generated pointer/index -> byte C ABI.
        #[cfg(feature = "sparse-jit")]
        let evaluate = unsafe {
            std::mem::transmute::<
                *const u8,
                unsafe extern "C" fn(*const u8, *const usize, *mut u8, usize),
            >(module.get_finalized_function(eval_id))
        };
        Ok(Self {
            module: Some(module),
            tick,
            #[cfg(feature = "sparse-jit")]
            evaluate,
            states_len: s.states_len,
            first_output,
            end_output,
            bytes,
        })
    }
}

// Native allocations stay on the compiler thread. The lease prevents freeing
// a published function until its last borrowing owner is gone.
struct Lease {
    id: u64,
    sender: mpsc::Sender<Command>,
}
impl Drop for Lease {
    fn drop(&mut self) {
        let _ = self.sender.send(Command::Free(self.id));
    }
}
pub(super) struct Code {
    _lease: Lease,
    tick: unsafe extern "C" fn(*const u8, *mut u8),
    #[cfg(feature = "sparse-jit")]
    evaluate: unsafe extern "C" fn(*const u8, *const usize, *mut u8, usize),
    states_len: usize,
    first_output: usize,
    end_output: usize,
    pub bytes: usize,
}
impl Code {
    pub fn run(&self, old: &[u8], next: &mut [u8]) {
        assert_eq!(old.len(), self.states_len);
        assert_eq!(next.len(), self.states_len);
        self.run_partition(old, &mut next[self.first_output..self.end_output]);
    }
    fn run_partition(&self, old: &[u8], next: &mut [u8]) {
        assert_eq!(old.len(), self.states_len);
        assert_eq!(next.len(), self.end_output - self.first_output);
        // SAFETY: validated offsets, disjoint borrowed slices, and live owner.
        unsafe {
            (self.tick)(old.as_ptr(), next.as_mut_ptr());
        }
    }
    #[cfg(feature = "sparse-jit")]
    fn evaluate(&self, old: &[u8], instructions: &[usize], results: &mut [u8]) {
        assert_eq!(old.len(), self.states_len);
        assert_eq!(instructions.len(), results.len());
        // SAFETY: checked IR reads, bounded borrowed index/result slices,
        // disjoint buffers, and the same live lease as run. Native indices are
        // masked into this chunk; the engine groups them by actual chunk.
        unsafe {
            (self.evaluate)(
                old.as_ptr(),
                instructions.as_ptr(),
                results.as_mut_ptr(),
                instructions.len(),
            );
        }
    }
}

fn vectorizable(ops: &[Instruction], inputs: &[u32]) -> bool {
    let first = ops[0];
    ops.iter().enumerate().all(|(lane, op)| {
        if op.kind != first.kind
            || op.count != first.count
            || op.output != first.output + lane as u32
        {
            return false;
        }
        match first.kind {
            7 | 8 => op.data == first.data + lane as u32,
            13 => op.data == first.data + lane as u32 && op.enable == first.enable + lane as u32,
            _ => (0..first.count)
                .all(|pin| inputs[op.start + pin] == inputs[first.start + pin] + lane as u32),
        }
    })
}
fn constant(b: &mut FunctionBuilder<'_>, ty: Type, n: i64) -> Value {
    let v = b.ins().iconst(types::I8, n);
    if ty.is_vector() {
        b.ins().splat(ty, v)
    } else {
        v
    }
}
#[cfg(feature = "sparse-jit")]
fn emit_short_circuit(
    b: &mut FunctionBuilder<'_>,
    old: Value,
    kind: u16,
    sources: &[u32],
) -> Value {
    let or = kind == 2 || kind == 5;
    let invert = kind == 4 || kind == 5;
    let decisive = b.ins().iconst(types::I8, i64::from(or));
    let answer = b.ins().iconst(types::I8, i64::from(or ^ invert));
    let default = b.ins().iconst(types::I8, i64::from(!or ^ invert));
    let x = b.ins().iconst(types::I8, 3);
    let join = b.create_block();
    b.append_block_param(join, types::I8);
    let mut unknown = b.ins().iconst(types::I8, 0);
    for &source in sources {
        let s = b
            .ins()
            .load(types::I8, MemFlags::new().with_notrap(), old, source as i32);
        let found = b.ins().icmp(IntCC::Equal, s, decisive);
        unknown = b.ins().bor(unknown, s);
        let next = b.create_block();
        b.ins().brif(found, join, &[answer.into()], next, &[]);
        b.switch_to_block(next);
    }
    let flag = b.ins().band_imm(unknown, 2);
    let value = b.ins().select(flag, x, default);
    b.ins().jump(join, &[value.into()]);
    b.switch_to_block(join);
    b.block_params(join)[0]
}
fn select(b: &mut FunctionBuilder<'_>, ty: Type, condition: Value, yes: Value, no: Value) -> Value {
    if ty.is_vector() {
        b.ins().bitselect(condition, yes, no)
    } else {
        b.ins().select(condition, yes, no)
    }
}
fn emit_gate(b: &mut FunctionBuilder<'_>, ty: Type, kind: u16, pins: &[Value]) -> Value {
    let zero = constant(b, ty, 0);
    let one = constant(b, ty, 1);
    let x = constant(b, ty, 3);
    match kind {
        7 | 8 => {
            let s = pins[0];
            let low = if kind == 8 { b.ins().bxor(s, one) } else { s };
            let high = b.ins().ushr_imm(s, 1);
            b.ins().bor(low, high)
        }
        13 => {
            let z = constant(b, ty, 2);
            let known = b.ins().icmp(IntCC::UnsignedLessThan, pins[0], z);
            let data = select(b, ty, known, pins[0], x);
            let enabled = b.ins().icmp(IntCC::Equal, pins[1], one);
            let value = select(b, ty, enabled, data, x);
            let disabled = b.ins().icmp(IntCC::Equal, pins[1], zero);
            select(b, ty, disabled, z, value)
        }
        15 => zero,
        1..=6 if pins.is_empty() => zero,
        1 | 2 | 4 | 5 => {
            let or = kind == 2 || kind == 5;
            let decisive = if or { one } else { zero };
            let default = if or { zero } else { one };
            let mut has_decisive = b.ins().icmp(IntCC::NotEqual, zero, zero);
            let mut unknown = has_decisive;
            let z = constant(b, ty, 2);
            for &s in pins {
                let d = b.ins().icmp(IntCC::Equal, s, decisive);
                has_decisive = b.ins().bor(has_decisive, d);
                let u = b.ins().icmp(IntCC::UnsignedGreaterThanOrEqual, s, z);
                unknown = b.ins().bor(unknown, u);
            }
            let result = select(b, ty, unknown, x, default);
            let result = select(b, ty, has_decisive, decisive, result);
            if kind == 4 || kind == 5 {
                let inv = b.ins().bxor(result, one);
                let known = b.ins().icmp(IntCC::NotEqual, result, x);
                select(b, ty, known, inv, x)
            } else {
                result
            }
        }
        3 | 6 => {
            let mut result = if kind == 6 { one } else { zero };
            let mut unknown = b.ins().icmp(IntCC::NotEqual, zero, zero);
            let z = constant(b, ty, 2);
            for &s in pins {
                result = b.ins().bxor(result, s);
                let u = b.ins().icmp(IntCC::UnsignedGreaterThanOrEqual, s, z);
                unknown = b.ins().bor(unknown, u);
            }
            select(b, ty, unknown, x, result)
        }
        _ => x,
    }
}

struct Chunk {
    code: Option<Code>,
    token: Option<Arc<AtomicBool>>,
    submitted: bool,
    executions: u64,
}
struct Job {
    chunk: usize,
    token: Arc<AtomicBool>,
    snapshot: Snapshot,
}
struct Completed {
    chunk: usize,
    token: Arc<AtomicBool>,
    code: Option<Code>,
}
enum Command {
    Compile(Job),
    Free(u64),
    Shutdown,
}

pub(super) struct Cache {
    chunks: Vec<Chunk>,
    sender: Option<mpsc::Sender<Command>>,
    receiver: Option<mpsc::Receiver<Completed>>,
    worker: Option<JoinHandle<()>>,
    dense_ticks: u64,
    cursor: usize,
    pub hits: u64,
    pub enabled: bool,
    compiled: usize,
    pending: usize,
}
impl Default for Cache {
    fn default() -> Self {
        Self {
            chunks: Vec::new(),
            sender: None,
            receiver: None,
            worker: None,
            dense_ticks: 0,
            cursor: 0,
            hits: 0,
            enabled: true,
            compiled: 0,
            pending: 0,
        }
    }
}
impl Drop for Cache {
    fn drop(&mut self) {
        self.cancel();
        self.chunks.clear();
        if let Some(sender) = self.sender.take() {
            let _ = sender.send(Command::Shutdown);
        }
        if let Some(worker) = self.worker.take() {
            let _ = worker.join();
        }
    }
}
impl Cache {
    fn cancel(&mut self) {
        for chunk in &self.chunks {
            if let Some(token) = &chunk.token {
                token.store(false, Ordering::Relaxed);
            }
        }
    }
    pub fn reset(&mut self, count: usize) {
        self.cancel();
        self.chunks = (0..count.div_ceil(CHUNK))
            .map(|_| Chunk {
                code: None,
                token: None,
                submitted: false,
                executions: 0,
            })
            .collect();
        self.dense_ticks = 0;
        self.cursor = 0;
        self.compiled = 0;
    }
    pub fn invalidate(&mut self, instruction: usize) {
        let c = &mut self.chunks[instruction / CHUNK];
        self.compiled -= usize::from(c.code.is_some());
        if let Some(token) = &c.token {
            token.store(false, Ordering::Relaxed);
        }
        *c = Chunk {
            code: None,
            token: None,
            submitted: false,
            executions: 0,
        };
        self.cursor = self.cursor.min(instruction / CHUNK);
    }
    pub fn code(&self, chunk: usize) -> Option<&Code> {
        self.chunks[chunk].code.as_ref().filter(|_| self.enabled)
    }
    pub fn has_code(&self) -> bool {
        self.enabled && self.compiled > 0
    }
    pub fn run_parallel(&mut self, pool: &rayon::ThreadPool, old: &[u8], next: &mut [u8]) -> bool {
        if !self.enabled || self.chunks.is_empty() || self.compiled != self.chunks.len() {
            return false;
        }
        let first = self.chunks[0].code.as_ref().unwrap().first_output;
        let end = self
            .chunks
            .last()
            .unwrap()
            .code
            .as_ref()
            .unwrap()
            .end_output;
        if !self.chunks.iter().enumerate().all(|(i, chunk)| {
            let code = chunk.code.as_ref().unwrap();
            code.first_output == first + i * CHUNK
                && code.end_output == (first + (i + 1) * CHUNK).min(end)
        }) {
            return false;
        }
        let chunks = &self.chunks;
        pool.install(|| {
            next[first..end]
                .par_chunks_mut(CHUNK)
                .zip(chunks.par_iter())
                .with_min_len(16)
                .for_each(|(outputs, chunk)| {
                    chunk.code.as_ref().unwrap().run_partition(old, outputs);
                })
        });
        self.hits += chunks.len() as u64;
        true
    }
    #[cfg(feature = "sparse-jit")]
    pub fn evaluate(
        &mut self,
        chunk: usize,
        old: &[u8],
        indices: &[usize],
        results: &mut [u8],
    ) -> bool {
        if !self.has_code() {
            return false;
        }
        let Some(code) = self.chunks[chunk].code.as_ref() else {
            return false;
        };
        code.evaluate(old, indices, results);
        self.hits += 1;
        true
    }
    pub fn stats(&self) -> (usize, usize, u64) {
        (
            self.chunks.iter().filter(|c| c.code.is_some()).count(),
            self.chunks
                .iter()
                .filter_map(|c| c.code.as_ref())
                .map(|c| c.bytes)
                .sum(),
            self.hits,
        )
    }
    fn start_worker(&mut self) -> bool {
        if self.sender.is_some() {
            return true;
        }
        let (tx, rx) = mpsc::channel::<Command>();
        let (done_tx, done_rx) = mpsc::channel();
        let lease_tx = tx.clone();
        let worker = std::thread::Builder::new()
            .name("circuit-jit".into())
            .spawn(move || {
                let mut allocations = HashMap::new();
                let mut id = 0u64;
                while let Ok(command) = rx.recv() {
                    match command {
                        Command::Free(id) => {
                            allocations.remove(&id);
                        }
                        Command::Shutdown => break,
                        Command::Compile(job) => {
                            let code = if job.token.load(Ordering::Relaxed) {
                                OwnedCode::compile(job.snapshot).ok().map(|owned| {
                                    id += 1;
                                    let code = Code {
                                        _lease: Lease {
                                            id,
                                            sender: lease_tx.clone(),
                                        },
                                        tick: owned.tick,
                                        #[cfg(feature = "sparse-jit")]
                                        evaluate: owned.evaluate,
                                        states_len: owned.states_len,
                                        first_output: owned.first_output,
                                        end_output: owned.end_output,
                                        bytes: owned.bytes,
                                    };
                                    allocations.insert(id, owned);
                                    code
                                })
                            } else {
                                None
                            };
                            if done_tx
                                .send(Completed {
                                    chunk: job.chunk,
                                    token: job.token,
                                    code,
                                })
                                .is_err()
                            {
                                break;
                            }
                        }
                    }
                }
                // OwnedCode drops explicitly free all executable allocations.
            });
        if let Ok(worker) = worker {
            self.sender = Some(tx);
            self.receiver = Some(done_rx);
            self.worker = Some(worker);
            true
        } else {
            self.enabled = false;
            false
        }
    }
    fn accept(&mut self, done: Completed) {
        self.pending -= 1;
        if let Some(chunk) = self.chunks.get_mut(done.chunk) {
            if chunk
                .token
                .as_ref()
                .is_some_and(|token| Arc::ptr_eq(&done.token, token))
            {
                self.compiled -= usize::from(chunk.code.is_some());
                self.compiled += usize::from(done.code.is_some());
                chunk.code = done.code;
            }
        }
    }
    pub fn prepare(&mut self, ops: &[Instruction], inputs: &[u32], len: usize, realistic: bool) {
        if !self.start_worker() {
            return;
        }
        for (i, ops) in ops.chunks(CHUNK).enumerate() {
            self.invalidate(i * CHUNK);
            let Some(snapshot) = Snapshot::new(ops, inputs, len, realistic) else {
                continue;
            };
            let token = Arc::new(AtomicBool::new(true));
            self.chunks[i].token = Some(Arc::clone(&token));
            if self
                .sender
                .as_ref()
                .unwrap()
                .send(Command::Compile(Job {
                    chunk: i,
                    token: token.clone(),
                    snapshot,
                }))
                .is_err()
            {
                return;
            }
            self.pending += 1;
            self.chunks[i].submitted = true;
            loop {
                let Ok(done) = self.receiver.as_ref().unwrap().recv() else {
                    return;
                };
                let finished = Arc::ptr_eq(&done.token, &token);
                self.accept(done);
                if finished {
                    break;
                }
            }
        }
    }
    pub fn poll(
        &mut self,
        ops: &[Instruction],
        inputs: &[u32],
        len: usize,
        realistic: bool,
        active: Option<&[usize]>,
    ) {
        if !self.enabled
            || self.chunks.is_empty()
            || self.compiled == self.chunks.len() && self.pending == 0
        {
            return;
        }
        if let Some(active) = active {
            if active.is_empty() {
                return;
            }
            for &index in active {
                if !matches!(ops[index].kind, 7 | 8 | 15) {
                    let chunk = &mut self.chunks[index / CHUNK];
                    if !chunk.submitted {
                        chunk.executions += 1;
                    }
                }
            }
        } else {
            for chunk in &mut self.chunks {
                if !chunk.submitted {
                    chunk.executions += CHUNK as u64;
                }
            }
        }
        self.dense_ticks += 1;
        if self.dense_ticks < WARM_TICKS || !self.dense_ticks.is_multiple_of(64) {
            return;
        }
        if self.sender.is_none()
            && !self
                .chunks
                .iter()
                .any(|c| c.executions >= 16_384 && !c.submitted)
        {
            return;
        }
        if !self.start_worker() {
            return;
        }
        while let Ok(done) = self.receiver.as_ref().unwrap().try_recv() {
            self.accept(done);
        }
        for _ in 0..self.chunks.len() {
            if self.pending >= 4 {
                break;
            }
            let i = self.cursor;
            self.cursor = (self.cursor + 1) % self.chunks.len();
            if self.chunks[i].submitted || self.chunks[i].executions < 16_384 {
                continue;
            }
            self.chunks[i].submitted = true;
            let Some(snapshot) = Snapshot::new(
                &ops[i * CHUNK..((i + 1) * CHUNK).min(ops.len())],
                inputs,
                len,
                realistic,
            ) else {
                continue;
            };
            let token = Arc::new(AtomicBool::new(true));
            self.chunks[i].token = Some(Arc::clone(&token));
            let job = Job {
                chunk: i,
                token,
                snapshot,
            };
            if self
                .sender
                .as_ref()
                .unwrap()
                .send(Command::Compile(job))
                .is_err()
            {
                self.enabled = false;
                break;
            }
            self.pending += 1;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parallel_native_partitions_match_scalar_and_reject_overlapping_ranges() {
        let n = 1025;
        let mut ops: Vec<_> = (0..n)
            .map(|i| Instruction {
                kind: 8,
                output: i as u32 + 4,
                data: ((i * 17 + 3) % n) as u32 + 4,
                start: 0,
                count: 0,
                enable: 0,
            })
            .collect();
        let old: Vec<_> = (0..n + 4).map(|i| (i % 4) as u8).collect();
        let mut scalar = old.clone();
        let mut parallel = old.clone();
        let mut cache = Cache::default();
        cache.reset(n);
        cache.prepare(&ops, &[], old.len(), false);
        assert_eq!(cache.stats().0, n.div_ceil(CHUNK));
        for i in 0..n.div_ceil(CHUNK) {
            cache.code(i).unwrap().run(&old, &mut scalar);
        }
        let pool = rayon::ThreadPoolBuilder::new()
            .num_threads(4)
            .build()
            .unwrap();
        assert!(cache.run_parallel(&pool, &old, &mut parallel));
        assert_eq!(scalar, parallel);
        for op in &ops {
            let s = old[op.data as usize];
            assert_eq!(parallel[op.output as usize], (s ^ 1) | (s >> 1));
        }
        ops.swap(200, 600);
        cache.reset(n);
        cache.prepare(&ops, &[], old.len(), false);
        parallel.clone_from(&old);
        assert!(!cache.run_parallel(&pool, &old, &mut parallel));
        assert_eq!(parallel, old);
    }
}
