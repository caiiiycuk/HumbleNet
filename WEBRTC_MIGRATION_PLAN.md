# WebRTC Migration Plan

## Purpose

This file is shared project context for future agents working on the Linux WebRTC migration in HumbleNet.

The goal is to replace the current unreliable micro WebRTC implementation with a Linux-native backend based on Google's `libwebrtc`, using `introlab/webrtc-native-build` as the source of Linux build artifacts.

This is a migration plan and architectural context document. It is not an implementation spec yet.

## Current Status

This section tracks actual progress made in the repository.

Completed:

- migration context documents were added
- external provider fork was added as a submodule at `3rdparty/webrtc-native-build`
- top-level CMake options for external WebRTC were added
- helper targets were added to configure/build/install the provider from the main project
- external provider install layout autodetection was added
- external Linux WebRTC configuration can now be enabled without failing before provider artifacts exist
- a new modern backend file exists at `src/humblenet/webrtc/webrtc_native_linux.cpp`
- the new backend is no longer a trivial stub; it now contains a modern `PeerConnection` / `DataChannel` adapter skeleton
- the external backend path in `src/humblenet/CMakeLists.txt` was updated to use modern requirements such as `cxx_std_20` and `WEBRTC_POSIX`

Partially completed:

- provider bootstrap, `fetch webrtc`, and `gclient sync` have completed
- provider build/install required Linux-side fixes in the fork and those fixes were applied
- runtime validation is in progress against the installed provider artifacts

Not yet completed:

- successful end-to-end execution of `tests/test_webrtc.cpp`
- CI coverage for the external backend path

## Scope

- Target platform: Linux only
- Keep existing HumbleNet public API intact
- Keep current microstack backend as fallback
- Introduce a new external native WebRTC backend built around `libwebrtc`
- Use the existing dynamic loading model where HumbleNet attempts to load `libwebrtc.so` first and falls back to microstack if unavailable

## Key Existing Project Facts

### Existing ABI boundary

HumbleNet already has a shared internal ABI for WebRTC backends in:

- `src/humblenet/src/libwebrtc.h`

This ABI is the contract that both:

- the current microstack implementation
- the future native Google WebRTC implementation

must satisfy.

### Existing dynamic loading path

HumbleNet already contains runtime loading logic in:

- `src/humblenet/src/libwebrtc_dynamic.cpp`

Behavior:

- it tries to load `libwebrtc.so` on Linux
- if loading or symbol resolution fails, it falls back to the internal microstack implementation

This means the project is already architecturally prepared for a backend swap.

### Existing native backend code is obsolete

The repository contains an old Chromium WebRTC adapter in:

- `src/humblenet/webrtc/webrtc_native.cpp`
- `3rdparty/chromium_webrtc/CMakeLists.txt`

This code targets a very old WebRTC API and old include layout. It should not be treated as a modern integration path.

Recommended approach:

- do not try to revive the old adapter in-place
- instead, write a new Linux-native adapter against modern `libwebrtc`

## Migration Strategy

### High-level approach

1. Build or obtain Linux `libwebrtc` artifacts using `introlab/webrtc-native-build`
2. Implement a new Linux HumbleNet backend that satisfies `src/humblenet/src/libwebrtc.h`
3. Package that backend as `libwebrtc.so`
4. Let existing HumbleNet runtime loading pick it up automatically
5. Preserve microstack as fallback

### Important constraint

For native Linux applications, browser-integrated WebRTC is not directly usable as a drop-in replacement.

The browser's built-in WebRTC stack is exposed via browser APIs such as `RTCPeerConnection`, not as a native C/C++ Linux runtime library for arbitrary applications.

Therefore the correct replacement path is:

- native `libwebrtc`

not:

- "the browser's built-in WebRTC"

## Target End State

After migration:

- HumbleNet still exposes the same public API
- on Linux, HumbleNet first attempts to load external `libwebrtc.so`
- if present and compatible, HumbleNet uses the new Google WebRTC backend
- if not present, HumbleNet continues using microstack
- the Linux build remains optional and explicitly enabled

## Planned Work

## Phase 1. Lock the architecture

Decisions:

- Do not change the HumbleNet public API
- Do not make external Google WebRTC mandatory for all builds
- Do not embed `introlab/webrtc-native-build` as a required always-built dependency in the main project path
- Keep the backend replaceable through the existing shared ABI
- Treat Linux as the only target for this migration phase

Expected result:

- clean separation between HumbleNet core and external WebRTC provider

## Phase 2. Establish external `libwebrtc` supply

Use `introlab/webrtc-native-build` as the source of Linux-native WebRTC artifacts.

Tasks:

- choose and pin a specific upstream revision
- document exact Linux distro/toolchain used to produce artifacts
- define expected output layout:
  - include directories
  - static or shared WebRTC libraries
  - any required bundled dependencies
- produce reproducible Linux x86_64 artifacts

Expected result:

- a reproducible set of Linux `libwebrtc` build artifacts

Status:

- partially done
- provider source bootstrap is underway
- final provider artifacts are not available yet

## Phase 3. Replace the obsolete native adapter

Do not evolve the current old file as the main implementation target:

- `src/humblenet/webrtc/webrtc_native.cpp`

Instead create a new Linux-native adapter, for example:

- `src/humblenet/webrtc/webrtc_native_linux.cpp`

Responsibilities of the new adapter:

- implement all functions declared in `src/humblenet/src/libwebrtc.h`
- wrap modern `libwebrtc`
- preserve HumbleNet callback semantics
- preserve HumbleNet connection/channel lifecycle expectations

Required ABI surface:

- `libwebrtc_create_context`
- `libwebrtc_destroy_context`
- `libwebrtc_set_stun_servers`
- `libwebrtc_add_turn_server`
- `libwebrtc_create_connection_extended`
- `libwebrtc_create_channel`
- `libwebrtc_create_offer`
- `libwebrtc_set_offer`
- `libwebrtc_set_answer`
- `libwebrtc_add_ice_candidate`
- `libwebrtc_write`
- `libwebrtc_close_channel`
- `libwebrtc_close_connection`

Expected result:

- a new native backend compiled as `libwebrtc.so`

Status:

- partially done
- `src/humblenet/webrtc/webrtc_native_linux.cpp` exists and implements a modern backend skeleton
- the adapter already covers context, connection, data channel, SDP, ICE, and close/send entrypoints
- it still needs full real-build validation and runtime testing against installed provider libraries

## Phase 4. CMake integration

The current `3rdparty/chromium_webrtc/CMakeLists.txt` is tied to an outdated workflow and should not be the basis for the new path.

Recommended new configuration model:

- `HUMBLENET_EXTERNAL_WEBRTC=ON`
- `HUMBLENET_EXTERNAL_WEBRTC_INCLUDE_DIR`
- `HUMBLENET_EXTERNAL_WEBRTC_LIB_DIR`

Build rules should:

- compile the new Linux adapter
- link it against prebuilt artifacts from `introlab/webrtc-native-build`
- keep the default HumbleNet build path unchanged unless explicitly enabled

Expected result:

- optional Linux external-WebRTC build path

Status:

- done for build and link integration
- external configuration flags and provider helper targets exist
- provider install layout autodetection exists
- the external path is intentionally allowed to configure before provider artifacts are installed
- HumbleNet `libwebrtc.so` now links against the installed provider artifacts
- remaining work is runtime stabilization rather than build-system wiring

## Phase 5. Runtime integration

Use existing runtime behavior in `src/humblenet/src/libwebrtc_dynamic.cpp`.

Requirements:

- preserve target library name `libwebrtc.so`
- ensure runtime search path is deterministic
- improve diagnostics for:
  - external backend loaded successfully
  - backend not found
  - missing symbol
  - incompatible backend build

Expected result:

- predictable runtime selection between external backend and microstack fallback

## Phase 6. Functional verification

Minimum verification matrix:

- peer-to-peer datachannel connection
- STUN-only path
- TURN-enabled path
- ICE candidate exchange
- open/send/receive/close channel flow
- connection shutdown flow
- repeated connect/disconnect cycles
- repeated execution of `tests/test_webrtc.cpp`

Additionally verify:

- callback ordering
- resource cleanup
- thread/lifecycle correctness
- behavior under failed negotiation

Expected result:

- confidence that the new backend matches HumbleNet expectations

Status:

- not done yet
- only syntax-level validation and repository integration work has been completed so far

## Phase 7. CI and packaging

Add a dedicated Linux job that:

- builds external WebRTC artifacts or consumes pinned ones
- builds the HumbleNet WebRTC backend
- builds HumbleNet with the standard path
- runs relevant tests
- publishes Linux artifacts if needed

Artifacts of interest:

- `libwebrtc.so`
- `libhumblenet.so`
- build metadata
- usage notes

## Phase 8. Documentation

Document:

- how to build Linux WebRTC artifacts from `introlab/webrtc-native-build`
- how to point HumbleNet CMake to those artifacts
- where `libwebrtc.so` must live at runtime
- how to confirm HumbleNet is using external WebRTC and not fallback microstack
- supported Linux environments

## Risks

### API drift

Modern `libwebrtc` differs substantially from the API expected by the old `webrtc_native.cpp`.

Impact:

- old code is not a safe drop-in basis

### Toolchain and ABI mismatches

Potential mismatches:

- compiler version
- standard library ABI
- linker behavior
- transitive dependency layout

Impact:

- runtime instability or link failures

### Threading and lifecycle mismatches

The HumbleNet callback model has expectations around connection, channel, and destroy events.

Impact:

- subtle crashes
- race conditions
- shutdown bugs

### STUN/TURN behavior differences

The new backend may negotiate differently from the microstack backend.

Impact:

- logic that previously relied on quirks may need adjustment

## Estimated Effort

Rough estimate for a careful Linux-only migration:

- external WebRTC artifact setup: 1-2 days
- new backend implementation: 3-6 days
- CMake and runtime integration: 1-2 days
- test stabilization and CI: 2-4 days

Total:

- approximately 1-2 weeks

## Recommended Execution Order

1. Produce reproducible Linux artifacts from `introlab/webrtc-native-build`
2. Implement a new `libwebrtc.so` backend against `src/humblenet/src/libwebrtc.h`
3. Add optional CMake wiring for external WebRTC
4. Validate runtime loading through existing dynamic loader
5. Run and stabilize `tests/test_webrtc.cpp`
6. Add CI and documentation

## Guidance For Future Agents

When working on this migration:

- do not assume the old Chromium adapter is salvageable with small edits
- do not remove microstack fallback unless explicitly instructed
- do not change HumbleNet public API unless there is a clear blocking reason
- prefer minimal integration with a strong runtime fallback story
- treat Linux as the only supported target unless the scope is explicitly expanded

If implementation begins, the next useful artifact should be:

- a technical design note for `webrtc_native_linux.cpp`

covering:

- object ownership
- thread model
- callback mapping
- SDP/ICE handling
- datachannel lifecycle

## Current Implementation Status

- `[x]` Linux provider submodule is added and pinned
- `[x]` external provider configure/build/install flow works from the main project
- `[x]` provider autodetection resolves installed `include` and `lib` paths from `dist/Release`
- `[x]` new `webrtc_native_linux.cpp` builds into `libwebrtc.so`
- `[x]` `humblenet_test_webrtc` now builds against the external backend path
- `[~]` provider fork contains Linux build fixes for `depot_tools` pathing and clang-based generation
- `[~]` provider fork now includes an upstream patch hook for the current WebRTC checkout compile break
- `[ ]` runtime behavior is not stabilized yet

Current runtime result:

- external `libwebrtc.so` is now actually loaded by the dynamic backend path
- modern SDP generation is confirmed in `humblenet_test_webrtc.bin.x86_64`
- Linux test execution in the current environment exposes a zero-network condition for `BasicNetworkManager`, which prevents normal ICE candidate gathering
- a test-only fallback investigation using fake/loopback allocation is in progress, but it is currently blocked on allocator thread-affinity and provider packaging gaps
- this means the migration is past build/link integration and now blocked on runtime connection establishment in the current environment