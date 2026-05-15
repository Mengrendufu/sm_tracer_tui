# AGENTS.md — termbox

A terminal-based HSM (Hierarchical State Machine) visualizer/demo using **notcurses** for rendering and **Super-Simple Tasker (SST)** as the Active Object kernel. Written in C99, built with CMake + Ninja.

## Project structure

```
.
├── application/          # Application code
│   ├── main.c            # Entry: notcurses init, SST thread spawn, UI loop
│   ├── aos/              # Active Object (AO) registry + Blinky AO
│   │   ├── aos.c         #   SST_start() — constructs & starts all AOs
│   │   ├── blinky.c      #   Blinky HSM — idle/active states, timer-driven
│   │   └── blinky.h
│   └── ui/               # UI module (main-thread event loop)
│       ├── ui.c          #   UI_prepare, UI_loop, input routing, rendering
│       ├── ui.h
│       ├── ui_evt.c      #   Thread-safe event queue (eventfd + ring buffer)
│       ├── ui_evt.h      #   UI_Evt, UI_AppEvt, signal enum, cross-thread post
│       ├── sm_ui.c       #   SM_UI HSM — showMain / showMenu states
│       ├── sm_ui.h
│       ├── sm_ui_key.c   #   SM_UI_Key HSM — placeholder, idle-only
│       └── sm_ui_key.h
├── ports/                 # Desktop port layer
│   ├── sm_port.h         #   offsetof/containerof macros
│   ├── sst_port.c        #   SST desktop port: pthread per AO, sem_wait, critical section
│   ├── sst_port.h        #   SST port macros: SRP locking, event pools, tick rate
│   └── static_pool_port.h#   (trivial — just includes stdint.h)
├── bsp/                   # Board Support Package (desktop)
│   ├── bsp.c             #   Tick idle loop, tick handlers, DBC/SM_onAssert fault handler
│   └── bsp.h
├── 3rd_party/             # Git submodules
│   ├── Super-Simple-Tasker/  # SST kernel (https://github.com/QuantumLeaps/Super-Simple-Tasker)
│   ├── sm_hsm/              # HSM engine (https://github.com/Mengrendufu/sm_hsm)
│   └── common_c/            # Common C lib (static_pool, etc.)
├── docs/qm/                  # QM model files (State Machine modeling tool)
│   └── blinky.qm
├── CMakeLists.txt
├── CMakePresets.json        # Ninja Multi-Config presets
└── toolchain_gcc.cmake      # gcc, -Wall -Wextra -Wpedantic
```

## Two-thread architecture

| Thread | Role | Runs |
|--------|------|------|
| **Main** | UI loop, input, notcurses rendering | `UI_loop()` — poll-based event loop |
| **SST** | AO kernel, all Active Objects | `SST_Task_run()` — each AO in its own pthread |

**Event flow** (cross-thread):
```
BSP idle loop → SST_TimeEvt_tick() → BSP_onTick() → UI_onTick_()
  → UI_postSignal(UI_TIMER_SIG) → enqueue → write(eventfd)
  → main thread poll() wakes → UI_evtDequeue() → SM_UI.dispatch()
```

**UI event queue** is a thread-safe ring buffer with `head`/`tail` that both **decrement** (reversed direction: head wraps from 0 to `UI_QLEN_ - 1`).

AOs (Blinky) post text to UI via `UI_postText()` → same queue → main thread dequeues → dispatches to SM_UI HSM.

## Essential commands

```sh
# Configure (one-time)
cmake --preset configure

# Build (after configure)
cmake --build --preset build-debug

# Run
cmake --build --preset run-debug

# Clean
cmake --build --preset clean-debug

# Explicit ninja commands (after configure)
cd build && ninja -f build-Debug.ninja

# Rebuild after changing CMakeLists.txt
cmake --preset configure && cmake --build --preset build-debug
```

## HSM patterns

Every HSM actor follows this pattern:

1. **Forward-declare** state handlers with `SM_HSM_RETT` attribute:
   ```c
   static SM_StatePtr myState_init_(SM_Hsm *me) SM_HSM_RETT;
   static void        myState_entry_(SM_Hsm *me) SM_HSM_RETT;
   static void        myState_exit_(SM_Hsm *me) SM_HSM_RETT;
   static SM_RetState myState_(SM_Hsm *me, EvtType const *e) SM_HSM_RETT;
   ```

2. **Define state table** with `SM_HSM_ROM` (maps to `const`):
   ```c
   SM_HsmState SM_HSM_ROM MyState = {
       &SUPER_STATE,
       (SM_InitHandler)&myState_init_,
       (SM_ActionHandler)&myState_entry_,
       (SM_ActionHandler)&myState_exit_,
       (SM_StateHandler)&myState_
   };
   ```

3. **Return macros**: `_SM_HANDLED()`, `_SM_SUPER()`, `_SM_TRAN(&target)`, `_SM_INIT(&target)`

4. **HSM init** calls `SM_Hsm_init_()` in the AO init handler:
   ```c
   SM_Hsm_init_(&me->hsm, (SM_InitHandler)TOP_initial);
   ```

5. **containerof** extracts the enclosing AO struct:
   ```c
   MyAO *ao = containerof(me, MyAO, hsm);
   ```

**Two event types** coexist:
- **SST_Evt** — used in Blinky (AO receives `SST_Evt const *`)
- **UI_Evt** — used in SM_UI and SM_UI_Key (HSM receives `UI_Evt const *`)

## Design by Contract (DBC)

Every `.c` file starts with `DBC_MODULE_NAME("name")`. Assertions use **numeric labels** (not line numbers) to stay stable across edits:

```c
DBC_REQUIRE(100, me != (SM_UI *)0);
DBC_ENSURE(200, e != (SST_Evt const *)0);
```

Labels must be **unique within a module** (file). Convention: each file uses a distinct label range (100s, 200s, 300s...).

DBC is compiled out via `-DDBC_DISABLE`. The fault handler (`DBC_fault_handler` in `bsp.c`) writes to `/dev/tty` (bypasses notcurses alt screen) then `abort()`.

## Virtual function table pattern

AOs use function pointers for `init` and `dispatch`:

```c
typedef void (*VC_Handler)(void * const me, void const * const e) SM_HSM_RETT;

// In AO struct:
VC_Handler init;     // → SM_Hsm_init_
VC_Handler dispatch;  // → SM_Hsm_dispatch_

// Constructor sets them:
me->init     = (VC_Handler)MyAO_init;
me->dispatch = (VC_Handler)MyAO_dispatch;
```

## SST desktop port (gotchas)

- **Critical sections**: ref-counted non-recursive mutex with `__thread` nesting counter. `enterCriticalSection_()` asserts `l_critSectNest == 0`. Cannot nest.
- **AO thread loop**: executes `ao_thread()` — `sem_wait` → dequeue (in critical section) → dispatch → `SST_GC(e)`. Never returns.
- **`SST_GC`** skips GC for signals below `SST_USER_SIG` (standard SST time events are not pool-allocated).
- **Event pools**: multi-size pools checked at allocation time; `SST_Evt_new` uses first pool with `blockSize >= requested`.
- **SST_Task_lock/unlock** are no-ops (SRP locking delegated to `SST_PORT_CRIT_*`).

## UI event system

- **128-entry ring buffer** (`UI_QLEN_ = 128`), mutex-guarded, with eventfd for poll-based wake-up.
- **UI_NULL_SIG** is reserved/invalid: `UI_postSignal` asserts `sig > UI_NULL_SIG`.
- **UI_AppEvt** extends `UI_Evt` via `super` member (not pointer). Text payload is allocated inline: `sizeof(UI_AppEvt) + len + 1`, with `pld.msg.text` pointing past the struct.
- **Signals** (in order): `UI_NULL_SIG`, `UI_KEY_ESC_SIG`, `UI_KEY_CTRL_SLASH_SIG`, `UI_KEY_UP_SIG`, `UI_KEY_DOWN_SIG`, `UI_KEY_ENTER_SIG`, `UI_KEY_J_SIG`, `UI_KEY_K_SIG`, `UI_KEY_PGUP_SIG`, `UI_KEY_PGDN_SIG`, `UI_KEY_DEBUG_SIG`, `UI_TIMER_SIG`, `UI_BLINKY_TEXT_SIG`, `UI_RESIZE_SIG`.
- `UI_evtFree` just does `free()` — no ref counting.
- Quit is **only** accessible via the menu's "Quit" item. No key shortcut.

## Interactive menu

Opened by `Ctrl+/` (toggles open/close). Menu items are rendered in a centered `ncplane` with a solid background fill and a manual `|-----|` border.

| Key | Action |
|-----|--------|
| `Ctrl+/` | Open / close menu |
| `↑` `k` | Navigate up (wraps) |
| `↓` `j` | Navigate down (wraps) |
| `Enter` | Select highlighted item |

**Menu items:** Resume, Clear screen, About, Quit.

Menu selection dispatch uses a `MenuAction` enum (`MENU_ACT_RESUME`, `MENU_ACT_CLEAR`, `MENU_ACT_ABOUT`, `MENU_ACT_QUIT`) returned by an IO function (`SM_UI_execMenuAction_`). The HSM layer switches on the returned enum for state transitions.

A **keybar** at the bottom of the screen shows context-sensitive hints:
- Menu closed: `ctrl+/ open menu`
- Menu open: `ctrl+/ close menu  j/k ↑↓ navigate  enter select`

The keybar helpers (`SM_UI_setKeybarClosed_`, `SM_UI_setKeybarOpen_`) take a `struct ncplane *` directly, not the enclosing `SM_UI`.

- `BSP_TICKS_PER_SEC = 100` — 100 Hz system tick.
- `SST_onIdle()` does `nanosleep(tickRateMs)` → `SST_TimeEvt_tick()` → `BSP_onTick()`.
- `BSP_onTick()` fanouts to registered tick handlers (`BSP_MAX_TICK_HANDLERS_ = 4`).
- UI timer: `UI_onTick_()` fires every `BSP_TICKS_PER_SEC / 10 = 10` ticks → 10 Hz.
- Render frame cap: 60 fps in `UI_render_()`.
- `mainPlane` background is filled solid via `ncplane_set_base_cell` (opaque). Colors: `(25, 25, 40)` bg, `(200, 220, 200)` fg.

## Code organization (sm_ui.c)

The file is organized into clear sections:

1. **Forward declarations** — HSM state handler signatures grouped together, followed by IO helper forward decls.
2. **State tables** — Each state's forward decls sit immediately above its `SM_HsmState` table.
3. **HSM implementations** — All state handlers, virtual functions (`init`/`dispatch`), and the AO constructor (`ctor`/`start`) in one contiguous block.
4. **IO implementations** — `drawMenuItem_`, `execMenuAction_`, `setKeybarClosed_`, `setKeybarOpen_` at the end. These take specific `ncplane *` pointers rather than the enclosing `SM_UI *`.

## QM modeling

The HSM state diagrams are modeled in `docs/qm/` using **QM** (https://www.state-machine.com/qm). The `.qm` file references QPC framework classes (`qpc::QActive`). All three models (Blinky, SM_UI, SM_UI_Key) are in `docs/qm/blinky.qm`.

Current implementation is handwritten (not QM-generated) but follows the QM model structure.

- `stdint.h` include added to `sm_ui.h` (was missing, caused clangd error).
- Several unused-include warnings from LSP; these are cosmetic.
- No test suite.

## Color scheme

| Element | Bg (RGB) | Fg (RGB) |
|---------|----------|----------|
| Outer box border | — | `(60, 60, 120)` / `(15, 15, 35)` |
| Title plane | `(60, 60, 120)` | `(230, 230, 255)` bold |
| Status plane | `(35, 35, 60)` | `(200, 200, 200)` |
| **Main plane** | `(25, 25, 40)` | `(200, 220, 200)` |
| Main plane border | — | `(60, 60, 120)` / `(15, 15, 35)` |
| Menu plane bg | `(50, 50, 100)` | `(200, 200, 220)` |
| Menu border / title | `(50, 50, 100)` | `(140, 140, 200)` |
| Menu selected item | `(80, 80, 160)` | `(255, 255, 255)` |
| Keybar bg | `(50, 50, 80)` | — |
| Keybar shortcut (gold) | — | `(230, 200, 100)` bold |
| Keybar description (muted) | — | `(160, 160, 180)` |

## Conventions

- Opening braces inline for single-line signatures; for multi-line parameter lists the `{` goes on its own line.
- Asterisk binds left on pointer types: `char *p`, `SM_UI * const me`
- `const` after `*`: `SST_Evt const *`, `struct ncplane * const`
- Single underscore suffix for private/static functions: `UI_onTick_`, `UI_routeInput_`
- `U` suffix for unsigned literals: `0U`, `1U`, `16U`
- Indentation: 4 spaces (no tabs)
- Copyright header: WTFPL v2, Sunny Matato 2026
- File comments section separator: `//====` ruler blocks
- Function names: `Module_action_qualifier` pattern (`SM_UI_active_init_`, `UI_evtDequeue`)
- `(void)e;` for unused parameters
- IO functions take specific `ncplane *` pointers, not the full enclosing struct
