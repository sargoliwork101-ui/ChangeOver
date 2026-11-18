# ESP_USER 64 KiB storage release
## Allocation
| Class | Committed payload ceiling |
|---|---:|
| Daily event files | 28 KiB |
| Chart snapshot plus journal | 10 KiB |
| Charge plus diagnostic/power history | 6 KiB |
| Credentials, Wi-Fi, parameters, clock, warning and theme | 4 KiB |
| Shared filesystem and staging allowance | 16 KiB |
| Total envelope | 64 KiB = 65536 bytes |
These are admission ceilings, not flash partitions or RAM sizes. Rounded physical
allocation takes precedence. Metadata and temporary copies consume space too,
so logs may be reclaimed before their payload ceiling. Temporary writes may
borrow the staging allowance; it is not always 16 KiB of unused flash.
## Retention and failure behavior
Oldest day files are reclaimed first, never passwords or settings. The active
writer's day is protected; a sole previous day may go to make room for today.
A day approaching 4096 bytes keeps its newest half through a verified atomic
rewrite. No extra logging or wake-up sampling was introduced. Existing bounded
charge/error histories and chart rings retain their existing counts and triggers.
Three months is a maximum file-count/range horizon, not guaranteed retention.
The current per-day file layout is unchanged; this release does not claim to
pack ninety daily files into 64 KiB. Unknown or physically damaged storage may
refuse a write; dirty/pending data then remain retryable, not falsely committed.
Snapshot writes stage, close, read back and atomically rename. A failed update
keeps the old committed file. Explicit history clears persist empty snapshots
before publishing empty RAM; all-class clears can partially succeed and return
an error. Chart sequence checkpoints prevent old journals restoring cleared data.
Login policy and current credentials are unchanged, including the pre-existing
missing/corrupt credential fallback. No password reset or STM32 edits are included.
## Validation workloads
Production functions were exercised on a 65536-byte virtual NOR using the actual
ESP8266 upstream LittleFS dependency at 6a53d76e90af33f0656333c1db09bd337fa75d23.
Geometry: 4096-byte erase blocks, 64-byte read/program/cache, block_cycles=16.
Each profile runs 129600 minute observations (90 days), then 1000 extra rows in
the final day to exercise compaction. Time and telemetry are injected; RF, board
hardware, flash endurance, sudden power cuts and target RAM use are not measured.
| Profile | Outage model | Daily files retained at end |
|---|---|---:|
| Steady | No outage | 4 |
| Normal example | One 2-hour outage per day and recharge | 2 |
| Busier example | Two 2-hour outages per day and recharge | 2 |
All three must complete with zero physical ENOSPC writes, zero blocked chart
samples, zero dropped RAM rows and unchanged administrator credential bytes.
These example profiles are not measurements of the owner's actual power usage.
## Reproduce
Run `NODE_PATH=/path/to/node_modules bash ESP_USER/Tester/run_simple_tests.sh`
with jsdom available for the DOM checks. Run real allocation tests separately:
`LITTLEFS_SOURCE=/path/to/pinned/littlefs bash ESP_USER/Tester/run_storage_littlefs.sh`.
No upstream dependency or generated flash image is shipped in firmware sources.
The downloadable release ZIP contains Arduino sketch sources, not a compiled
flashable binary. Select the actual target board and a 64 KiB LittleFS layout in
the Arduino build configuration. Do not format existing storage just to update.
