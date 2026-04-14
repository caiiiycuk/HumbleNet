# WebRTC Native Linux Design

## Purpose

This file is implementation-oriented context for future agents working on the Linux native WebRTC backend for HumbleNet.

It builds on:

- `WEBRTC_MIGRATION_PLAN.md`

This document describes the intended technical design for a new Linux `libwebrtc` backend, integration boundaries, repository structure decisions, and implementation details that should guide code changes.

## Implementation Status

This section records what has already been implemented in the repository.

Completed:

- the provider fork has been added as a submodule at `3rdparty/webrtc-native-build`
- HumbleNet has a dedicated external Linux WebRTC configuration path
- helper targets exist to configure/build/install the provider from the root project
- autodetection of the provider install layout under `dist/<BuildType>/webrtc-native-build-<version>` exists
- `src/humblenet/webrtc/webrtc_native_linux.cpp` now contains a modern backend skeleton instead of a placeholder stub
- the external backend path in `src/humblenet/CMakeLists.txt` now requests modern language/platform requirements for upstream WebRTC

In progress:

- provider bootstrap through `fetch webrtc` and `gclient sync`
- real integration validation against provider-built libraries

Pending:

- final provider build/install
- successful link of HumbleNet external backend against installed provider artifacts
- end-to-end runtime verification with HumbleNet tests

## Core Decisions

### External provider source

The project will use this fork as the editable upstream for Linux WebRTC artifacts:

- `git@github.com:caiiiycuk/webrtc-native-build.git`

Reason:

- the team needs a source that can be patched as integration issues are discovered
- relying on a third-party repository without write access would make the migration slower and less reproducible

### Repository integration model

The external provider will be added to this repository as a git submodule.

Planned submodule path:

- `3rdparty/webrtc-native-build`

Reason:

- pinned revisions
- reproducible checkouts
- ability to patch and version the provider alongside HumbleNet
- clearer CI and local build behavior

### Runtime model

HumbleNet will continue using its existing runtime backend selection model:

- try external `libwebrtc.so`
- if unavailable or incompatible, fall back to microstack

This behavior already exists in:

- `src/humblenet/src/libwebrtc_dynamic.cpp`

## Non-Goals

The following are not goals for the first implementation phase:

- Windows support
- macOS support
- browser-side API changes
- public HumbleNet API changes
- removal of microstack fallback
- embedding Chromium or browser runtimes

## Planned Repository Layout

Expected additions and changes:

- `3rdparty/webrtc-native-build`
- new Linux-native backend source, likely:
  - `src/humblenet/webrtc/webrtc_native_linux.cpp`
- optional supporting headers, likely under:
  - `src/humblenet/webrtc/`
- CMake wiring for external WebRTC paths
- CI updates for Linux-only external backend path

## Backend Contract

The Linux native backend must implement the ABI defined in:

- `src/humblenet/src/libwebrtc.h`

That ABI is the stable bridge between:

- HumbleNet core
- microstack backend
- external native Google WebRTC backend

The implementation must export all required symbols expected by:

- `src/humblenet/src/libwebrtc_dynamic.cpp`

## Design Principles

### Principle 1. Preserve the ABI

Do not change `src/humblenet/src/libwebrtc.h` unless there is a hard blocker.

Reason:

- HumbleNet core is already written against that ABI
- dynamic loading depends on those exported symbols
- preserving the ABI allows safe fallback to microstack

### Principle 2. Replace the old adapter, do not patch around it

Do not treat:

- `src/humblenet/webrtc/webrtc_native.cpp`

as the main implementation base.

That code is useful only as historical reference for callback semantics and expected behavior. It should not define the new architecture.

### Principle 3. Keep external build concerns isolated

The `webrtc-native-build` fork should remain the place where upstream `libwebrtc` build concerns are solved.

HumbleNet should not absorb the full complexity of building upstream WebRTC from source.

### Principle 4. Prefer deterministic runtime behavior

When `libwebrtc.so` is found, HumbleNet should clearly use it.

When it is missing or incompatible, HumbleNet should clearly log fallback to microstack.

Silent ambiguous behavior is not acceptable.

## Submodule Strategy

The fork:

- `git@github.com:caiiiycuk/webrtc-native-build.git`

should be added as a submodule at:

- `3rdparty/webrtc-native-build`

Expected workflow:

1. Pin a known good commit in the submodule
2. Build provider artifacts from that pinned commit
3. If integration issues are found, patch the fork
4. Advance the submodule pointer in HumbleNet intentionally

This is preferred over:

- downloading release archives ad hoc
- pointing to arbitrary local checkouts
- depending on unpinned external branches

Current status:

- implemented
- the submodule is present in the repository and points to `git@github.com:caiiiycuk/webrtc-native-build.git`

## Build and Artifact Model

## Provider side

The forked `webrtc-native-build` submodule is responsible for producing the Linux WebRTC artifacts required by HumbleNet.

Expected outputs:

- WebRTC headers
- static libraries and/or shared libraries needed for linking
- any packaged support dependencies required by the provider build

The exact artifact set depends on what the fork emits most reliably on Linux.

## HumbleNet side

HumbleNet should build a backend shared library:

- `libwebrtc.so`

This shared library should:

- implement HumbleNet's backend ABI
- link against the fork-produced WebRTC artifacts
- be runtime-loadable by `libwebrtc_dynamic.cpp`

Important:

- the provider artifact output and the HumbleNet backend output are different things
- the provider builds WebRTC libraries
- HumbleNet builds the ABI adapter library that exposes the expected symbols

## CMake Integration Model

Recommended controls:

- `HUMBLENET_EXTERNAL_WEBRTC=ON`
- `HUMBLENET_EXTERNAL_WEBRTC_PROVIDER_ROOT`
- `HUMBLENET_EXTERNAL_WEBRTC_INCLUDE_DIR`
- `HUMBLENET_EXTERNAL_WEBRTC_LIB_DIR`

Recommended default convention when the submodule exists:

- `HUMBLENET_EXTERNAL_WEBRTC_PROVIDER_ROOT=3rdparty/webrtc-native-build`

The build should support:

- automatic use of the submodule path when present
- manual override of include/lib directories if needed

The default HumbleNet build must remain unchanged unless external WebRTC is explicitly enabled.

Current status:

- implemented for the bootstrap phase
- current root CMake includes:
  - `HUMBLENET_EXTERNAL_WEBRTC`
  - `HUMBLENET_EXTERNAL_WEBRTC_PROVIDER_ROOT`
  - `HUMBLENET_EXTERNAL_WEBRTC_INCLUDE_DIR`
  - `HUMBLENET_EXTERNAL_WEBRTC_LIB_DIR`
  - `HUMBLENET_EXTERNAL_WEBRTC_LIBRARIES`
- helper targets exist:
  - `humblenet_external_webrtc_provider_configure`
  - `humblenet_external_webrtc_provider_build`
  - `humblenet_external_webrtc_provider_install`
- the external path is intentionally non-fatal before provider artifacts are installed

## Technical Design

## Main objects

The new backend should preserve the conceptual model already used by HumbleNet:

- context
- connection
- data channel

Suggested object mapping:

- `libwebrtc_context`
  - owns WebRTC threads
  - owns `PeerConnectionFactory`
  - stores STUN/TURN configuration
  - owns callback entrypoint reference
- `libwebrtc_connection`
  - owns `PeerConnection`
  - implements `PeerConnectionObserver`
  - maps WebRTC state transitions to HumbleNet callbacks
- `libwebrtc_data_channel`
  - owns or references a `DataChannelInterface`
  - implements `DataChannelObserver`
  - maps message and close events to HumbleNet callbacks

Current status:

- partially implemented in `src/humblenet/webrtc/webrtc_native_linux.cpp`
- all three object types exist
- observer classes for local/remote description handling also exist

## Thread model

The thread model must be explicit and stable.

Planned approach:

- create dedicated WebRTC threads required by the modern API
- ensure callback dispatch behavior is understood and documented
- serialize HumbleNet-facing lifecycle transitions carefully

Must verify:

- where PeerConnection callbacks arrive
- where DataChannel callbacks arrive
- whether shutdown can race with pending callbacks

Shutdown behavior must be deliberate. Destruction should not rely on implicit ordering.

Current status:

- partially implemented
- the current backend skeleton creates dedicated network, worker, and signaling threads
- shutdown behavior still needs runtime validation against the real linked provider

## Callback mapping

The adapter must preserve the HumbleNet callback contract defined by `libwebrtc.h`.

Required mappings:

- local SDP creation -> `LWRTC_CALLBACK_LOCAL_DESCRIPTION`
- ICE candidate discovery -> `LWRTC_CALLBACK_ICE_CANDIDATE`
- successful connection establishment -> `LWRTC_CALLBACK_ESTABLISHED`
- disconnect/close -> `LWRTC_CALLBACK_DISCONNECTED`
- inbound channel ready -> `LWRTC_CALLBACK_CHANNEL_ACCEPTED`
- outbound channel ready -> `LWRTC_CALLBACK_CHANNEL_CONNECTED`
- message receive -> `LWRTC_CALLBACK_CHANNEL_RECEIVE`
- channel close -> `LWRTC_CALLBACK_CHANNEL_CLOSED`
- fatal negotiation/runtime error -> `LWRTC_CALLBACK_ERROR`
- final connection destruction -> `LWRTC_CALLBACK_DESTROY`

Important:

- callback order matters
- `DESTROY` must happen once and only after the object is actually done from HumbleNet's perspective

Current status:

- partially implemented
- local description, ICE candidate, channel receive, channel open/close, established, disconnected, and destroy paths are already represented in the new backend skeleton
- callback ordering still requires runtime verification

## SDP handling

The adapter should:

- generate offers and answers using modern WebRTC APIs
- serialize SDP to text for HumbleNet callbacks
- parse remote SDP text provided by HumbleNet

Requirements:

- robust parse failure handling
- meaningful logging on parse errors
- no silent partial setup

Current status:

- partially implemented
- offer creation, answer creation, remote offer application, and remote answer application are already wired in the new backend skeleton
- real negotiation testing is still pending

## ICE handling

The adapter should:

- gather local ICE candidates
- serialize candidates to the string form expected by HumbleNet
- accept remote ICE candidate strings from HumbleNet and inject them into the connection

Must verify with tests:

- STUN-only operation
- TURN-enabled operation
- candidate ordering behavior
- candidate delivery before and after remote description setup

Current status:

- partially implemented
- outbound candidate serialization and inbound candidate parsing/injection are present in the new backend skeleton
- real ICE behavior has not been validated yet

## Data channel behavior

The existing HumbleNet behavior assumes a default datachannel-based transport model.

The adapter must preserve these expectations:

- channels can be created explicitly
- offer creation may depend on a default channel existing
- inbound channels can arrive before user-level interaction completes
- channel open and close events must map cleanly into HumbleNet callbacks

Must verify:

- reliability flags and ordering flags are set intentionally
- `write` behavior matches HumbleNet assumptions
- channel close does not double-fire destroy paths

Current status:

- partially implemented
- bootstrap/default channel handling is present
- explicit channel creation is present
- send and close entrypoints are implemented
- actual state and sequencing need runtime validation

## Error handling

The adapter should treat the following as explicit error cases:

- provider initialization failure
- peer connection factory creation failure
- peer connection creation failure
- SDP parse failure
- local description creation failure
- invalid ICE candidate input
- datachannel send failure
- provider runtime failure requiring teardown

Errors should:

- log clearly
- trigger HumbleNet-visible error or close behavior
- avoid leaking partially alive objects

## Logging requirements

The Linux backend should emit enough logs to answer:

- was external backend loaded
- was provider initialization successful
- was a peer connection created
- was SDP generated and applied
- were ICE candidates exchanged
- did channel open
- did send fail
- why did connection close
- did fallback to microstack occur

The goal is integration debugging, not verbose production telemetry.

## Implementation Detail Expectations

## Provider patch policy

Because the project will use:

- `git@github.com:caiiiycuk/webrtc-native-build.git`

the implementation is allowed to patch the provider fork when needed.

Examples of acceptable provider-side patches:

- Linux build fixes
- export or packaging fixes
- include path cleanup
- linker or artifact layout adjustments
- deterministic output path improvements

Examples of changes that should remain on HumbleNet side instead:

- ABI adapter logic
- HumbleNet callback mapping
- HumbleNet runtime loading rules
- project-specific CMake options

Current status:

- no provider fork patch has been committed from this repository yet
- one provider-side issue was already discovered and worked around locally by initializing the nested `depot_tools` submodule

## Submodule update policy

When changing provider behavior:

- patch the fork first
- update the submodule pointer second
- keep HumbleNet changes compatible with the pinned provider revision

Avoid:

- depending on uncommitted provider changes
- undocumented local patches outside the submodule

## Testing Plan

Minimum tests for the new backend:

- build success with external provider enabled
- runtime loading of external `libwebrtc.so`
- successful negotiation between two local peers
- successful message exchange in both directions
- clean connection shutdown
- repeated negotiation cycles

Network path tests:

- STUN-only
- TURN-enabled

Failure-path tests:

- malformed SDP
- malformed ICE candidate
- send on closed channel
- provider unavailable at runtime

Fallback tests:

- remove or hide `libwebrtc.so`
- verify HumbleNet still works via microstack

## Expected Implementation Sequence

1. Add the fork as submodule under `3rdparty/webrtc-native-build`
2. Document provider build outputs and stable paths
3. Add new CMake options for external Linux WebRTC
4. Implement `webrtc_native_linux.cpp`
5. Build `libwebrtc.so`
6. Verify runtime loading through existing loader
7. Run `tests/test_webrtc.cpp`
8. Add diagnostics and cleanup fixes
9. Add CI

## Open Questions To Resolve During Implementation

- exact provider artifact layout produced by the fork
- whether HumbleNet backend should link mostly against provider static libs or shared libs
- exact Linux runtime search strategy for `libwebrtc.so`
- whether any ABI wrapper versioning should be added later
- whether the old obsolete native adapter should be kept as reference or removed after migration stabilizes

## Guidance For Future Agents

When implementing this design:

- assume the provider fork is editable
- assume the provider is integrated as a submodule, not an ad hoc external checkout
- prefer changes in the provider fork for provider build/package problems
- prefer changes in HumbleNet for ABI adaptation and runtime loading behavior
- keep Linux support isolated and explicit
- avoid broad refactors outside the migration path unless they directly unblock the work

The next useful engineering artifact after this document is:

- a concrete file-by-file implementation checklist

covering:

- submodule addition
- CMake changes
- new backend files
- runtime loading verification
- test execution order
