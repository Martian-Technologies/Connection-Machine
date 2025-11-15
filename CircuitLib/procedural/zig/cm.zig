pub const BlockType = u32;
pub const connection_end_id_t = u32;
pub const block_size_t = u8;
pub const block_id_t = i32;
pub const coord_t = i32;
pub const Rotation = i8;

pub extern "env" fn importFile(path: [*:0]const u8) u32;
pub extern "env" fn getParameter(key: [*:0]const u8) i32;
pub extern "env" fn getPrimitiveType(name: [*:0]const u8) BlockType;
pub extern "env" fn getNonPrimitiveType(uuid: [*:0]const u8) BlockType;
pub extern "env" fn getProceduralCircuitType(uuid: [*:0]const u8, params: [*:0]const u8) BlockType;
pub extern "env" fn getBusBlock(bit_width: i32) BlockType;
pub extern "env" fn getBusBlockAdvanced(num_inputs: i32, num_outputs: i32, input_lane_width: i32, output_lane_width: i32) BlockType;

pub extern "env" fn createBlock(block_type: BlockType) block_id_t;
pub extern "env" fn createBlockAtPosition(x: coord_t, y: coord_t, rotation: Rotation, blockType: BlockType) block_id_t;

pub extern "env" fn createConnection(source_block_id: block_id_t, source_port_id: connection_end_id_t, destination_source_id: block_id_t, destination_port_id: connection_end_id_t) void;
pub extern "env" fn addConnectionInput(external_port_x: coord_t, external_port_y: coord_t, internal_block_id: block_id_t, internal_port_id: connection_end_id_t) void;
pub extern "env" fn addConnectionOutput(external_port_x: coord_t, external_port_y: coord_t, internal_block_id: block_id_t, internal_port_id: connection_end_id_t) void;

pub extern "env" fn setSize(width: coord_t, height: coord_t) void;
pub extern "env" fn logInfo(msg: [*:0]const u8) void;
pub extern "env" fn logError(msg: [*:0]const u8) void;
