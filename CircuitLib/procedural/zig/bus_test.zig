const cm = @import("cm.zig");

const UUID_: [*:0]const u8 = "500abfd9-31b1-4694-8cff-9cf2ae640996";
const Name_: [*:0]const u8 = "Bus Test";
const DefaultParams_: [*:0]const u8 = "()";

export fn getUUID() [*:0]const u8 {
    return UUID_;
}
export fn getName() [*:0]const u8 {
    return Name_;
}
export fn getDefaultParameters() [*:0]const u8 {
    return DefaultParams_;
}

export fn generateCircuit() bool {
    const goofyBusBlockType = cm.getBusBlockAdvanced(4, 3, 3, 4);
    if (goofyBusBlockType == 0) {
        cm.logError("Failed to get bus block type.");
        return false;
    }
    _ = cm.createBlockAtPosition(0, 0, 0, goofyBusBlockType);

    const standardBusBlockType = cm.getBusBlock(8);
    if (standardBusBlockType == 0) {
        cm.logError("Failed to get standard bus block type.");
        return false;
    }
    _ = cm.createBlockAtPosition(2, 0, 0, standardBusBlockType);
    return true;
}
