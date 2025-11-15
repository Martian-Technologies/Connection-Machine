const cm = @import("cm.zig");

const UUID_: [*:0]const u8 = "abd7f5c1-2e89-4630-bf12-d596c812dde8";
const Name_: [*:0]const u8 = "Edge Detector";
const DefaultParams_: [*:0]const u8 = "(\"pulse width\": 3, \"{1:R,2:F,3:B}\": 2)";

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
    var pulse: i32 = cm.getParameter("pulse width");
    const kind = cm.getParameter("{1:R,2:F,3:B}");
    if (pulse < 1) {
        cm.logError("Pulse width should not be less than 1.");
        return false;
    }
    pulse += 1;

    const AND = cm.getPrimitiveType("AND");
    const NOR = cm.getPrimitiveType("NOR");
    const XNOR = cm.getPrimitiveType("XNOR");
    const SWITCH = cm.getPrimitiveType("SWITCH");
    const LIGHT = cm.getPrimitiveType("LIGHT");

    cm.setSize(1, 1);

    const endGate: cm.block_id_t = switch (kind) {
        1 => cm.createBlockAtPosition(pulse + 2, 0, 0, AND),
        2 => cm.createBlockAtPosition(pulse + 2, 0, 0, NOR),
        3 => cm.createBlockAtPosition(pulse + 2, 0, 0, XNOR),
        else => {
            cm.logError("{1:R,2:F,3:B} should only be 1, 2, or 3 and nothing else!");
            return false;
        },
    };

    const input = cm.createBlockAtPosition(0, 0, 0, SWITCH);
    cm.addConnectionInput(0, 0, input, 0);
    cm.createConnection(input, 0, endGate, 0);

    var wait = cm.createBlockAtPosition(1, 1, 0, NOR);
    cm.createConnection(input, 0, wait, 0);

    var i: i32 = 0;
    while (i < pulse) : (i += 1) {
        const tmp = cm.createBlockAtPosition(1, 1 + i, 0, AND);
        cm.createConnection(wait, 1, tmp, 0);
        wait = tmp;
    }

    cm.createConnection(wait, 1, endGate, 0);

    const output = cm.createBlockAtPosition(pulse + 3, 0, 0, LIGHT);
    cm.createConnection(endGate, 1, output, 0);
    cm.addConnectionOutput(0, 0, output, 0);

    return true;
}
