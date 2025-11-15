version_0
Test: "Tri State Buffer Test"
Ports: "Input", "Enable", "Output"
Group "0 0 Z":
    >Set:
        "Input":0
        "Enable":0
    >Step 10
    >Check:
        "Output":2
Group "0 1 0":
    >Set:
        "Input":1
        "Enable":0
    >Step 10
    >Check:
        "Output":0
Group "0 Z X":
    >Set:
        "Input":2
        "Enable":0
    >Step 10
    >Check:
        "Output":3
Group "0 X X":
    >Set:
        "Input":3
        "Enable":0
    >Step 10
    >Check:
        "Output":3
Group "1 0 Z":
    >Set:
        "Input":0
        "Enable":1
    >Step 10
    >Check:
        "Output":2
Group "1 1 1":
    >Set:
        "Input":1
        "Enable":1
    >Step 10
    >Check:
        "Output":1
Group "1 Z X":
    >Set:
        "Input":2
        "Enable":1
    >Step 10
    >Check:
        "Output":3
Group "1 X X":
    >Set:
        "Input":3
        "Enable":1
    >Step 10
    >Check:
        "Output":3
Group "Z 0 Z":
    >Set:
        "Input":0
        "Enable":2
    >Step 10
    >Check:
        "Output":2
Group "Z 1 X":
    >Set:
        "Input":1
        "Enable":2
    >Step 10
    >Check:
        "Output":3
Group "Z Z X":
    >Set:
        "Input":2
        "Enable":2
    >Step 10
    >Check:
        "Output":3
Group "Z X X":
    >Set:
        "Input":3
        "Enable":2
    >Step 10
    >Check:
        "Output":3
Group "X 0 Z":
    >Set:
        "Input":0
        "Enable":3
    >Step 10
    >Check:
        "Output":2
Group "X 1 X":
    >Set:
        "Input":1
        "Enable":3
    >Step 10
    >Check:
        "Output":3
Group "X Z X":
    >Set:
        "Input":2
        "Enable":3
    >Step 10
    >Check:
        "Output":3
Group "X X X":
    >Set:
        "Input":3
        "Enable":3
    >Step 10
    >Check:
        "Output":3