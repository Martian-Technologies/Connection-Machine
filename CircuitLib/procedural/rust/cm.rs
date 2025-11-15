#![allow(non_camel_case_types)]
#![allow(dead_code)]

pub type BlockType = u32;
pub type connection_end_id_t = u32;
pub type block_size_t = u8;
pub type block_id_t = i32;
pub type coord_t = i32;
pub type Rotation = i8;

extern "C" {
    // File & parameter functions
    pub fn importFile(path: *const u8) -> u32;
    pub fn getParameter(key: *const u8) -> i32;

    // Block type functions
    pub fn getPrimitiveType(name: *const u8) -> BlockType;
    pub fn getNonPrimitiveType(uuid: *const u8) -> BlockType;
    pub fn getProceduralCircuitType(uuid: *const u8, params: *const u8) -> BlockType;
    pub fn getBusBlock(bit_width: i32) -> BlockType;
    pub fn getBusBlockAdvanced(
        num_inputs: i32,
        num_outputs: i32,
        input_lane_width: i32,
        output_lane_width: i32
    ) -> BlockType;

    // Block creation
    pub fn createBlock(block_type: BlockType) -> block_id_t;
    pub fn createBlockAtPosition(x: coord_t, y: coord_t, rotation: Rotation, block_type: BlockType) -> block_id_t;

    // Connection functions
    pub fn createConnection(
        source_block_id: block_id_t,
        source_port_id: connection_end_id_t,
        destination_block_id: block_id_t,
        destination_port_id: connection_end_id_t,
    );

    pub fn addConnectionInput(
        external_port_x: coord_t,
        external_port_y: coord_t,
        internal_block_id: block_id_t,
        internal_port_id: connection_end_id_t,
    );

    pub fn addConnectionOutput(
        external_port_x: coord_t,
        external_port_y: coord_t,
        internal_block_id: block_id_t,
        internal_port_id: connection_end_id_t,
    );

    // Misc
    pub fn setSize(width: coord_t, height: coord_t);
    pub fn logInfo(msg: *const u8);
    pub fn logError(msg: *const u8);
}
