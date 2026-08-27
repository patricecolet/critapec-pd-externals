# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository layout

This is a collection of independent Pure Data externals by Patrice Colet. Each external lives in its own top-level directory and is built independently — there is no top-level Makefile and no umbrella library. Sibling directories do not share code; any shared patterns are duplicated per external.

Current externals:
- `bytes2float`, `bytes2int`, `float2byte`, `uint32tobytes` — byte/number conversion between Pd floats and little-endian byte streams.
- `cb4tech_parse` — parses a 30-byte `sensors_values_t` struct from CB4Tech hardware into named controller messages (`encoder`, `adc`, `joystick`, `drum`, `button`, `selector`).
- `encoder2note` — maps encoder rotation to MIDI-note + cents based on a "gear" position.
- `gpioencoder` (class `rpi_encoder_step`) — quadrature encoder + switch via `libgpiod` on Raspberry Pi; a dedicated pthread polls GPIO events while a Pd `clock` forwards values from atomic counters.
- `midifile` — reads/writes SMF files (derived from Martin Peach's mrpeach external; GPL-2.0+).
- `xiao_serial` — bidirectional USB serial I/O with a XIAO ESP32S3 via `termios`; reader thread + circular buffer, Pd `clock` drains it.
- `pdjson` — **Lua external** (`.pd_lua`, requires `pd-lua` and `lunajson`), not compiled C. Loads/reads/writes JSON and can dump it as chunked binary frames.

## Build

Each C external uses [pd-lib-builder](https://github.com/pure-data/pd-lib-builder), included **once** at the repo root as a git submodule (`pd-lib-builder/`). Every external's Makefile points at it via `PDLIBBUILDER_DIR=../pd-lib-builder/`.

After cloning, initialize the submodule:

```
git clone --recursive <repo-url>
# or, on an existing clone:
git submodule update --init
```

Build from inside the external's directory:

```
cd <external>
make           # produces <name>.pd_darwin, .pd_linux, etc. for the host platform
make clean
```

There is no repo-wide build target — to rebuild everything, loop over the directories. Compiled artifacts (`*.pd_darwin`, `*.pd_linux`, `*.o`) are gitignored but are sometimes committed in the tree anyway; treat them as build output.

### Version string convention

Every C external's Makefile extracts `lib.version` by grepping a `VERSION` line out of `<name>-meta.pd`:

```make
lib.version := $(shell sed -n 's|^\#X text [0-9][0-9]* [0-9][0-9]* VERSION \(.*\);|\1|p' $(lib.name)-meta.pd)
```

When bumping a version, edit the `#X text ... VERSION x.y;` line in `<name>-meta.pd` — do **not** hardcode it in the Makefile or the `.c` file (the `.c` uses `-DVERSION='"$(lib.version)"'` as a fallback-protected `#define`).

### Platform-conditional builds

Two externals have platform-specific build logic driven by `uname -s` in their Makefile:

- `gpioencoder/Makefile` — on Linux adds `-lgpiod -lpthread`; on anything else defines `RPI_ENCODER_FORCE_STUB` so the external still compiles (useful on macOS for development) but outputs no GPIO data. The source guards GPIO code behind `#ifdef RPI_ENCODER_HAVE_GPIOD`.
- `xiao_serial/Makefile` — links `-lpthread` on both Linux and Darwin (termios is POSIX on both).

When touching these files, preserve both the Linux path and the stub path — they are the entire macOS-dev workflow.

## Architectural patterns worth knowing

### Threaded GPIO/serial + Pd clock handoff

Both `rpi_encoder_step` and `xiao_serial` use the same concurrency shape: a dedicated pthread blocks on an OS primitive (GPIO `poll(2)` / serial `read(2)`), and a Pd `clock_delay`-driven tick function runs on the Pd audio thread to drain accumulated state and emit outlets. This is the correct way to bridge blocking I/O into Pd — **never call `outlet_*` from the worker thread**. Communication between the threads uses `stdatomic` counters (encoder) or a mutex-protected circular buffer (serial).

The tick function reschedules itself via `clock_delay(x->x_clock, x->x_interval)` at the end — if you edit tick logic, make sure every exit path still reschedules, or the external will stop emitting.

### Encoder quantization

`rpi_encoder_step` accumulates quadrature edges in `x_pending_steps` (4 edges per mechanical detent). The tick function divides by 4 and carries the remainder in `x_partial_steps`, so the outlet only fires on full detents. This is intentional anti-jitter behavior — see `gpioencoder/README.md`. Don't "fix" the /4 divisor without understanding this.

### Invalid-input routing (bytes2float pattern)

`bytes2float` and `bytes2int` route malformed input (bytes outside 0–255, or extra bytes beyond what one value consumes) out a second "extra" outlet rather than dropping or clamping them. When adding new byte-parsing externals, follow this convention so users can chain them.

### pdjson index buffer

`pdjson.pd_lua` flattens nested JSON into a list of `[index_path..., value]` rows stored in `jsonFileBuffer`, which is rebuilt eagerly on `read` and `set`. The `dumpBinary` method frames the raw JSON string into `BINARY`-tagged chunks with 4-byte little-endian size + offset headers — this is the wire format the XIAO ESP32S3 firmware expects. Keep the chunk header layout in sync with the firmware if you change it.

## Testing

There is no automated test suite. The verification workflow is:

1. Build the external.
2. Open `<name>-help.pd` in Pure Data — every external ships one and they double as smoke tests.
3. For hardware-dependent externals (`gpioencoder`, `xiao_serial`), verification requires the physical device; the stub / macOS build only proves it compiles.

`xiao_serial/TEST_MACOS.md` documents manual macOS verification steps for that external specifically.
