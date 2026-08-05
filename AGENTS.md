# AGENTS.md - sm_tracer TUI

A C99 terminal serial-tracing application built with notcurses, `sm_hsm`,
and the `sm_sst` Active Object kernel. CMake and Ninja drive the desktop
build.

## Project structure

```text
.
|-- application/
|   |-- main.c                 # Process startup and main-thread UI loop
|   |-- app_sig.h              # Application-wide SST signal registry
|   |-- aos/
|   |   |-- aos.c/.h           # AO registry and startup
|   |   |-- blinky/            # Timer-driven sample AO
|   |   `-- sp_mngr/           # Serial-port manager AO
|   |-- sp_thread/
|   |   |-- sp_thread.h        # Public serial-thread lifecycle API
|   |   |-- thread/            # Native serial-thread runtime
|   |   `-- hsm/               # Serial-thread HSM
|   `-- ui/
|       |-- ui.h               # Public UI lifecycle API
|       |-- ui_evt.h           # Public cross-thread UI ingress
|       |-- ui_input.h         # Normalized terminal-input payload
|       |-- thread/            # UI runtime, inbox, router, wake set
|       |-- hsm/               # SM_UI and SM_InputCmpsMngr
|       `-- widgets/           # Passive notcurses UI components
|-- bsp/                       # Tick, SST runtime init, fault handling
|-- ports/
|   |-- sm/                    # sm_hsm desktop adaptation
|   `-- sst/                   # SST pthread and event-pool adaptation
|-- 3rd_party/                 # Third-party dependencies
|   |-- libserialport/         # Vendored Linux/Windows serial library
|   |-- sm_sst/                # Git submodule
|   |-- sm_hsm/                # Git submodule
|   `-- common_c/              # Git submodule
|-- tests/                     # Contract and focused behavior tests
|-- CMakeLists.txt
|-- CMakePresets.json
`-- toolchain_gcc.cmake
```

## Runtime architecture

| Thread category | Role | Entry |
|-----------------|------|-------|
| Main | Terminal input, UI event drain, frame scheduling, rendering | `UI_run()` |
| Serial port | Event/serial wake, HSM dispatch, RX packet assembly | `SpThread_run_()` |
| SST kernel | Starts AOs, ticks timers, runs idle callback | `SST_Task_run()` |
| SST AO workers | One pthread and queue per started AO | `ao_thread()` |

These are categories, not a fixed count. The current process has five threads:
main, serial, SST kernel, Blinky worker, and SpMngr worker.

Launch-call order is significant:

```text
UI_init -> SpThread_start -> SST_init/SST_Task_run -> UI_run
```

UI infrastructure must exist before the serial HSM and SST AOs initialize,
because their initial transitions can call `UI_postText()`.
`SpThread_start()` and the SST launch create threads asynchronously. Before
entering `UI_run()`, main waits until `SST_onStart()` confirms that all AOs
have completed synchronous construction, queue setup, and initial transition.

The serial-thread HSM has an `active` parent with `disconnected` and
`connected` leaf states. Port refresh is handled by `active`; open and close
successes transition between the leaves. Open failure retains `disconnected`;
close failure or connection loss performs best-effort cleanup and returns to
`disconnected`. `SerialPortRuntime` owns the opened `sp_port` handle.

The serial receive path is `SerialPortRuntime -> RxPacketAssembler ->
SpMngrRxPacketEvt -> SpMngr HSM -> HdlcParser -> UI_postText`. The transport
packet owns a heap payload until SpMngr synchronously feeds every byte to its
embedded parser and releases the payload. Parser state survives transport
packet boundaries. Complete checksum-valid frames are currently rendered as
hexadecimal debug text; protocol-file and RecID mapping are not connected yet.

## UI boundaries

The terminal-input path is:

```text
notcurses_getvec
  -> UI_Input
  -> UIInputRouter
  -> UIEventInbox
  -> SM_UI
  -> SM_InputCmpsMngr
  -> InputComposer
```

- `UIThreadRuntime` owns the notcurses root, frame clock, host state, and loop.
- `UIInputRouter` classifies normalized `UI_Input` into `UI_Signal` values.
- `UIEventInbox` owns queued events and the producer-wakeup `eventfd`.
- `UIThreadWake` borrows terminal/event FDs, owns `pollfd[2]`, and reports
  terminal, event, or render-timeout readiness without consuming input.
- `SM_UI` owns page state and the top-level widget graph. Its embedded manager
  directly owns the `InputComposer` and `CommandSuggestion` lifecycles.
- `SM_UI` also owns the latest successful packed serial-port list. The serial
  thread and SpMngr only transfer transient copies and retain no list state.
- `SM_UI_HostOps` is a required contract owned by `SM_UI`; the UI runtime
  injects `requestQuit`, `requestFrame`, and a host-owned context.
- `SM_InputCmpsMngr` owns canonical UTF-8 text, byte length, edit position,
  editing semantics, command candidates, and accepted Token spans.
- Its command sink emits a validated submission intent to `SM_UI`; `SM_UI`
  merges optional overrides into its authoritative selected serial
  configuration, updates `ConnectionStatusBar` as a projection, and posts a
  complete snapshot event to `AO_SpMngr`.
- `InputComposer` is a passive `ncplane` projection with a software cursor.
  It soft-wraps against the current TUI panel width, grows upward to four
  rows, and then keeps the editing cursor visible through a vertical viewport.
  A rejected capacity insertion changes its prompt to `! ` and gives the
  software cursor warning colors until Manager removes or clears input text.
  It does not consume UI events or own canonical input text.
- `CommandSuggestion` is a passive sibling `ncplane`; Manager HSM state owns
  its candidates, selection, and visibility semantics.

The manager supports incremental left/right movement, insertion, backspace,
`Ctrl-U`, `Ctrl-W`, `Home`, and `End`. Its `editPos` is a UTF-8 byte offset,
not a terminal column. A leading or space-delimited `/` opens sorted command
suggestions. `Tab` or `Enter` accepts one as an atomic `$command` Token;
multiple Tokens can coexist in the canonical buffer. Acceptance always inserts
one ordinary trailing space and projects the Token span in blue. Typed or
pasted `$command` text is not a Token because it has no accepted span.

Command arguments remain ordinary editable text. Moving into or editing an
argument recomputes its suggestions against the whole word. `$connect` borrows
the current packed port catalog from `SM_UI`; baudrate, data bits, stop bits,
parity, and flow control use built-in candidate lists. Baudrate and protocol
still accept free text. `Tab` accepts an argument candidate, while `Enter`
submits the command and therefore preserves parameterless `$connect`.

Submission accepts configuration-only updates or one `$connect` plus optional
configuration Tokens. `$disconnect` and `$refresh` are standalone. Duplicate
commands, mixed lifecycle actions, missing configuration values, and stray
text reject the whole submission without posting or clearing the input.

## UI event system

- The inbox is a mutex-guarded 512-entry ring buffer.
- Queue `head` and `tail` both decrement and wrap from zero to the last slot.
- `UI_NULL_SIG` is reserved and invalid for posting.
- `UI_AppEvt` stores text inline after the event object; `UI_PortListEvt` does
  the same for NUL-separated, double-NUL-terminated port names.
- `UI_InputEvt` copies the complete normalized `UI_Input` payload.
- `UI_evtFree()` uses `free()`; UI events are not reference counted.
- Terminal-originated events are already on the UI thread, so enqueueing them
  does not write the eventfd. Once terminal readiness is reported, the runtime
  drains notcurses input in bounded batches and dispatches each batch before
  reading the next one. Cross-thread posts enqueue and wake the loop.
- The public application ingress consists of `UI_postText()` and
  `UI_postPortList()`; allocation, queue, wake, and dequeue remain private to
  the UI thread/HSM.

Current `enum UIEventSignals` order is:

```text
UI_NULL_SIG,
UI_INPUT_SIG,
UI_KEY_ESC_SIG, UI_KEY_CTRL_SLASH_SIG,
UI_KEY_UP_SIG, UI_KEY_DOWN_SIG,
UI_KEY_LEFT_SIG, UI_KEY_RIGHT_SIG,
UI_KEY_CTRL_LEFT_SIG, UI_KEY_CTRL_RIGHT_SIG,
UI_KEY_BACKSPACE_SIG, UI_KEY_CTRL_U_SIG, UI_KEY_CTRL_W_SIG,
UI_KEY_HOME_SIG, UI_KEY_END_SIG, UI_KEY_TAB_SIG, UI_KEY_ENTER_SIG,
UI_KEY_J_SIG, UI_KEY_K_SIG,
UI_KEY_CTRL_N_SIG, UI_KEY_CTRL_P_SIG,
UI_KEY_PGUP_SIG, UI_KEY_PGDN_SIG, UI_RESIZE_SIG,
UI_TIMER_SIG, UI_TEXT_SIG, UI_REFRESHED_PORTS_SIG
```

## Rendering and widgets

- Rendering is demand-driven through `requestFrame` with a 16 ms minimum frame
  interval (`1000U / 60U`), an approximate upper bound of 62.5 Hz.
- `UI_onTick_()` posts `UI_TIMER_SIG` at 10 Hz from the 100 Hz BSP tick.
- `SM_UI_flush()` updates dirty widget projections before
  `notcurses_render()` commits the frame.
- `TextBufferView` owns its text-area and scrollbar coupling, including its
  component-local content dirty state.
- Title bar, connection status bar, text buffer, input composer, keybar, and
  menu are separate widgets with component-specific APIs.
- `SM_UI` keeps the keybar anchored, shrinks `TextBufferView` while the input
  composer grows, and recomputes wrapping whenever terminal width changes.
- Quit is selected through the menu. `SM_UI_teardown()` destroys
  lifecycle-sensitive widgets before `notcurses_stop()` tears down the
  remaining plane graph.

Menu keys:

| Key | Action |
|-----|--------|
| `Ctrl+/` | Open or close menu |
| Up / `k` | Select previous item, wrapping |
| Down / `j` | Select next item, wrapping |
| `Enter` | Execute selected `MenuAction` |

The menu returns `MENU_ACT_RESUME`, `MENU_ACT_CLEAR`, `MENU_ACT_ABOUT`, or
`MENU_ACT_QUIT` through `Menu_action()`. The SM_UI state handler owns the
resulting transition and host request.

## HSM patterns

Each HSM declaration block is a visual state index. Use the module divider,
mark `TOP-INIT`, and name every state before its declarations and table.
Compact HSM declarations may exceed the normal line-width limit. Forward
declarations and definitions both include `SM_HSM_RETT`:

```c
//============================================================================
//=== HSM states

// TOP-INIT
static SM_StatePtr Class_TOP_initial_(SM_Hsm * const me) SM_HSM_RETT;

// state
static SM_StatePtr Class_state_init_(SM_Hsm * const me) SM_HSM_RETT;
static void        Class_state_entry_(SM_Hsm * const me) SM_HSM_RETT;
static void        Class_state_exit_(SM_Hsm * const me) SM_HSM_RETT;
static SM_RetState Class_state_(SM_Hsm * const me, EvtType const * const e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM Class_state = {
    SM_HSM_TOP,                              // super
    (SM_InitHandler)&Class_state_init_,      // init_
    (SM_ActionHandler)&Class_state_entry_,   // entry_
    (SM_ActionHandler)&Class_state_exit_,    // exit_
    (SM_StateHandler)&Class_state_           // handler
};
```

State tables use `SM_HSM_ROM`; handlers return `_SM_HANDLED()`, `_SM_SUPER()`,
`_SM_TRAN(&target)`, or `_SM_INIT(&target)`. Several signals that deliberately
share one action may be grouped as adjacent `case` labels.

Every state owns uniquely named `init`, `entry`, `exit`, and handler actions.
Do not bind one state's action directly into another state's table, even when
their behavior is currently identical. Put reusable implementation in an
ordinary helper and call it from separate state-specific actions.

Two event domains coexist:

- `SST_Evt` crosses AO queues and drives AO HSMs.
- `UI_Evt` is private to the UI thread/HSM boundary.

Use `containerof()` when an HSM handler needs its enclosing AO or subsystem
instance.

## SST desktop port

- The port uses a non-recursive mutex and a thread-local nesting guard;
  critical sections must not nest.
- Every AO worker waits on its semaphore, dequeues under the critical section,
  dispatches outside it, then calls `SST_GC()`.
- `SST_Task_setPrio()` records the priority, registers the task, initializes
  its semaphore, and launches its pthread.
- `SST_Task_lock()` and `SST_Task_unlock()` are no-ops in this port.
- The event-pool mechanism is compile-time optional through
  `SST_EVT_POOL_NUM`; this port currently fixes it to `3U`, while BSP currently
  initializes small, mid-size, and big event pools.
- The mid-size pool is sized for lightweight `SpMngrRxPacketEvt` and
  `SpMngrPortsEvt` traffic. The big pool independently holds the much larger
  `SpMngrConfigEvt` snapshots.
- When enabled, `SST_Evt_new()` selects the first fitting initialized pool and
  initializes `poolId` and `refCtr`. For dynamic events, `SST_Evt_gc()`
  decrements `refCtr` when it exceeds one and otherwise returns the event to
  its pool. Static events have `poolId == 0U` and are not reclaimed.

## Design by Contract

Each implementation module declares `DBC_MODULE_NAME("name")`. Numeric labels
must be stable and unique within that module:

```c
DBC_REQUIRE(100, me != (SM_UI *)0);
DBC_ENSURE(200, e != (SST_Evt const *)0);
```

Use requirements for caller obligations, invariants for persistent state, and
ensures for produced results. The desktop fault handler writes to `/dev/tty`
before `abort()` so failures remain visible outside the notcurses alternate
screen. `DBC_DISABLE` and `SM_DBC_DISABLE` remove their respective checks.

## Build and tests

Prerequisites are CMake 3.25 or newer, Ninja, GCC, and the notcurses-core
headers and library. Preset schema 6 is the reason for the CMake minimum.

```sh
cmake --preset debug
cmake --build --preset build-debug
ctest --test-dir build/Linux/debug --output-on-failure

cmake --preset release
cmake --build --preset build-release

cmake --build --preset run-debug
cmake --build --preset run-release

cmake --build --preset clean-debug
cmake --build --preset clean-release
```

Because application sources are collected with `GLOB_RECURSE` without
`CONFIGURE_DEPENDS`, rerun `cmake --preset debug` or `release` after adding or
removing a `.c` file.

CTest currently exercises:

- `ui_input_router`
- `ui_input_cmps_mngr`
- `sp_mngr_command`
- `hdlc_parser`
- `sp_thread_event_flow`
- `sp_thread_wake`
- `serial_port_runtime`
- `rx_packet_assembler`
- `ui_input_composer_lifecycle`
- `ui_command_suggestion_lifecycle`
- `scrollbar`
- `text_area`
- `ui_widget_boundary_contract`

Compile-only object targets additionally check the public UI event ingress,
application subsystem declarations, and the Manager-facing input composer API.

## External models

The checked-out repository currently contains no `docs/qm/` directory. QM
state-machine work is maintained externally under:

```text
/mnt/c/mengrendufu/workshop/qm/sm_tracer_tui
```

The C implementation is handwritten and follows the agreed QM/QP-style HSM
semantics; update the external model deliberately when state behavior changes.

The StarUML architecture model is maintained externally at:

```text
/mnt/c/mengrendufu/workshop/umls/staruml/sm_tracer/sm_tracer_tui/sm_tracer_tui.mdj
```

## Color scheme

| Element | Background | Foreground |
|---------|------------|------------|
| Terminal root | `(24, 27, 31)` | Widget-specific |
| Title bar | `(60, 60, 120)` | Muted lavender/white/mint |
| Connection status | `(42, 42, 44)` | Gray/mint |
| Text buffer | `(32, 32, 34)` | `(200, 220, 200)` |
| Input composer | `(38, 38, 42)` | Gray/mint |
| Menu | `(50, 50, 100)` | `(200, 200, 220)` |
| Selected menu item | `(80, 80, 160)` | `(255, 255, 255)` |
| Keybar | `(50, 50, 80)` | Gold/muted lavender |

## Conventions

- These C style rules apply to `application/`, `bsp/`, and `ports/`.
- Opening braces are inline for single-line function signatures.
- For multi-line parameter lists, put the opening brace on its own line.
- Keep code lines within 78 ASCII characters, except compact HSM declarations
  where the established model-oriented layout is clearer.
- Asterisk binds left: `char *p`, `SM_UI * const me`.
- Put `const` after the pointed-to type: `SST_Evt const *`.
- Private functions and instances use one trailing underscore.
- Unsigned literals use the `U` suffix.
- Indent with four spaces and no tabs.
- Copyright header: WTFPL v2, Sunny Matato 2026.
- Section separators use `//====` ruler blocks.
- Use `(void)e;` for intentionally unused parameters.
- UI IO functions take the narrowest component pointer available, not the
  enclosing `SM_UI`, unless orchestration genuinely requires it.
