# ChangeOver State Machine

Open `System_State_Machine.drawio` in [diagrams.net](https://app.diagrams.net).

Every state of the whole program (system, charger channel, faults, UI faces,
imbalance, dead-battery verdict, MCU power path, ESP link) is tabulated in
`System_State_Machine.xlsx` - regenerate it with
`python3 tools/make_state_machine_xlsx.py`.

```mermaid
stateDiagram-v2
    [*] --> BOOT
    BOOT --> BATTERY: valid && !input_present
    BOOT --> INPUT: valid && input_present
    BOOT --> BOOT: NULL/invalid
    BATTERY --> SAFE: v<20800 OR low-battery latch (set <21000, cleared >=21200), 3000ms
    INPUT --> SAFE: same cut condition, 3000ms
    SAFE --> INPUT: input_present && v>=21200, 3000ms
    BATTERY --> BATTERY: valid mapping
    INPUT --> INPUT: valid mapping
    SAFE --> SAFE: invalid/fault preserve
    BATTERY --> SAFE: imbalance latch + param 117 (immediate)
    INPUT --> SAFE: dead-battery verdict + param 127 (immediate)
    BOOT --> FAULT: valid && fault_mask != 0
    BATTERY --> FAULT: valid && fault_mask != 0
    INPUT --> FAULT: valid && fault_mask != 0
    SAFE --> FAULT: valid && fault_mask != 0
    FAULT --> INPUT: fault cleared + input_present
    FAULT --> BATTERY: fault cleared + !input_present
```
