# WebRTC Native Linux Checklist

## Purpose

This is the execution checklist for implementing the Linux native WebRTC backend in HumbleNet.

Related context:

- `WEBRTC_MIGRATION_PLAN.md`
- `WEBRTC_NATIVE_LINUX_DESIGN.md`

This document is intentionally task-oriented and file-oriented.

## Progress Snapshot

Legend:

- `[x]` done
- `[~]` in progress or partially done
- `[ ]` not done

Current snapshot:

- `[x]` planning and design documents added
- `[x]` provider fork added as submodule
- `[x]` external Linux WebRTC CMake options added
- `[x]` helper provider configure/build/install targets added
- `[x]` provider install layout autodetection added
- `[x]` external configure path made bootstrap-safe before provider install exists
- `[~]` modern Linux backend skeleton implemented in `src/humblenet/webrtc/webrtc_native_linux.cpp`
- `[~]` provider bootstrap started, including nested `depot_tools` init and `fetch webrtc`
- `[ ]` provider build
- `[ ]` provider install
- `[ ]` final external backend link validation
- `[ ]` `tests/test_webrtc.cpp` on the external backend
- `[ ]` CI coverage

## Success Criteria

The migration is considered minimally successful when all of the following are true:

- the provider fork exists as a pinned submodule
- HumbleNet can build a Linux native `libwebrtc.so` backend
- HumbleNet loads that backend through the existing dynamic loader
- `tests/test_webrtc.cpp` works with the external backend
- fallback to microstack still works when external backend is absent

## Phase 0. Repository Preparation

### 0.1 Add provider submodule

Target path:

- `3rdparty/webrtc-native-build`

Provider repo:

- `git@github.com:caiiiycuk/webrtc-native-build.git`

Tasks:

- add the repository as a git submodule
- pin it to a known commit
- verify a fresh clone with submodules can initialize it correctly

Expected files affected:

- `.gitmodules`
- git submodule metadata

Done when:

- submodule path exists
- pinned commit is committed in HumbleNet

Status:

- `[x]` done

### 0.2 Add provider usage notes

Tasks:

- document expected local build flow for the provider
- record where its Linux output artifacts will be found

Preferred location:

- either append to `WEBRTC_NATIVE_LINUX_DESIGN.md`
- or add provider-specific notes under a new Markdown file later if needed

Done when:

- another agent can build provider artifacts without rediscovering paths

Status:

- `[x]` done at the planning/design document level
- `[~]` still worth extending later with exact build commands after first successful provider install

## Phase 1. Inspect Provider Outputs

### 1.1 Identify provider artifact layout

Submodule path:

- `3rdparty/webrtc-native-build`

Tasks:

- inspect provider build scripts
- determine include directory path
- determine output library directory path
- determine whether outputs are static, shared, or mixed
- determine any required runtime dependencies

Record:

- exact include path
- exact library path
- exact artifact names

Done when:

- HumbleNet integration can refer to concrete provider output paths

Status:

- `[x]` done
- identified provider install layout under:
  - `3rdparty/webrtc-native-build/dist/<BuildType>/webrtc-native-build-<version>/include`
  - `3rdparty/webrtc-native-build/dist/<BuildType>/webrtc-native-build-<version>/lib`
- identified default Linux artifacts:
  - `libwebrtc.a`
  - `libboringssl.a`
  - `libyuv_internal.a`
  - system link deps such as `dl`, `X11`, `expat`

### 1.2 Decide HumbleNet linkage model

Tasks:

- decide whether HumbleNet adapter links against provider static libs or provider shared libs
- decide how to produce final `libwebrtc.so`

Recommendation:

- prefer the simplest reliable Linux path with the fewest runtime surprises

Done when:

- there is a clear link strategy for the new adapter library

Status:

- `[x]` done at the initial design level
- current plan is to build HumbleNet `libwebrtc.so` as the ABI adapter and link it against provider-installed static libraries

## Phase 2. CMake Wiring

### 2.1 Add new top-level options

File:

- `CMakeLists.txt`

Tasks:

- add an option to enable external Linux WebRTC
- add variables for provider root/include/lib directories
- keep the default build path unchanged when the option is off

Suggested options:

- `HUMBLENET_EXTERNAL_WEBRTC`
- `HUMBLENET_EXTERNAL_WEBRTC_PROVIDER_ROOT`
- `HUMBLENET_EXTERNAL_WEBRTC_INCLUDE_DIR`
- `HUMBLENET_EXTERNAL_WEBRTC_LIB_DIR`

Done when:

- configuration can be switched on explicitly without affecting default builds

Status:

- `[x]` done

### 2.2 Add provider-aware build logic

Files:

- `CMakeLists.txt`
- `src/humblenet/CMakeLists.txt`

Tasks:

- teach CMake to locate provider headers and libraries
- add a Linux-only path to build the new backend adapter
- keep current microstack path intact
- avoid relying on obsolete `3rdparty/chromium_webrtc/CMakeLists.txt`

Done when:

- CMake can configure a Linux build that includes the new backend target

Status:

- `[x]` done for bootstrap and path selection
- external configuration path now works before provider artifacts are installed
- final link validation still depends on provider install completing

### 2.3 Decide fate of old Chromium WebRTC CMake

Files:

- `3rdparty/chromium_webrtc/CMakeLists.txt`
- `src/humblenet/CMakeLists.txt`

Tasks:

- decide whether to leave old logic untouched but unused
- or explicitly deprecate and bypass it in the new path

Recommendation:

- avoid deleting old logic early
- isolate the new path first

Done when:

- the new path does not depend on the obsolete path

Status:

- `[x]` done
- the new external path does not rely on the obsolete Chromium integration
- old path is still left in-tree as reference/fallback logic for future cleanup decisions

## Phase 3. New Linux Backend Implementation

### 3.1 Create new adapter source

New file:

- `src/humblenet/webrtc/webrtc_native_linux.cpp`

Optional new files:

- `src/humblenet/webrtc/webrtc_native_linux.h`
- helper headers under `src/humblenet/webrtc/`

Tasks:

- implement a modern `libwebrtc` adapter
- do not base core architecture on the obsolete `webrtc_native.cpp`

Done when:

- all backend ABI functions are implemented in the new Linux file set

Status:

- `[~]` in progress
- the file exists and implements the full ABI surface structurally
- real runtime validation against linked provider libs is still pending

### 3.2 Implement context object

Primary file:

- `src/humblenet/webrtc/webrtc_native_linux.cpp`

Tasks:

- implement `libwebrtc_context`
- store callback pointer
- store STUN/TURN configuration
- create and own WebRTC threads
- create and own `PeerConnectionFactory`

Done when:

- context creation and destruction work reliably

Status:

- `[~]` partially done
- context object, thread ownership, and factory creation are implemented
- reliability still needs linked-runtime testing

### 3.3 Implement connection object

Primary file:

- `src/humblenet/webrtc/webrtc_native_linux.cpp`

Tasks:

- implement `libwebrtc_connection`
- implement `PeerConnectionObserver`
- map connection state changes to HumbleNet callbacks
- support offer/answer application
- support ICE candidate injection

Done when:

- connections can be created, negotiated, and closed cleanly

Status:

- `[~]` partially done
- connection object and peer connection observer are implemented
- negotiation and close paths exist
- real end-to-end negotiation is still unverified

### 3.4 Implement data channel object

Primary file:

- `src/humblenet/webrtc/webrtc_native_linux.cpp`

Tasks:

- implement `libwebrtc_data_channel`
- implement `DataChannelObserver`
- map open, message, and close events to HumbleNet callbacks
- implement send logic

Done when:

- data can be exchanged bidirectionally over channels

Status:

- `[~]` partially done
- channel observer, send, receive, open, and close paths are implemented
- bidirectional exchange is not validated yet

### 3.5 Implement full ABI surface

Contract file:

- `src/humblenet/src/libwebrtc.h`

Functions to implement:

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

Done when:

- dynamic symbol loading can resolve the full expected interface

Status:

- `[~]` structurally done
- functions are present in the new backend source
- final exported-library validation remains pending

## Phase 4. Exported Library

### 4.1 Build HumbleNet backend shared library

Files:

- `src/humblenet/CMakeLists.txt`
- possibly new CMake helper logic if needed

Tasks:

- build final adapter library named `libwebrtc.so`
- ensure required symbols are exported
- ensure Linux linker settings are correct

Done when:

- the produced library can be loaded by HumbleNet at runtime

Status:

- `[ ]` not done yet
- blocked on provider build/install completing

### 4.2 Verify symbol visibility

Files:

- `src/humblenet/webrtc/exported_linux.map`
- `src/humblenet/CMakeLists.txt`

Tasks:

- confirm exported symbol list includes the full ABI surface
- update export map if necessary

Done when:

- `dlopen` and `dlsym` can resolve all required symbols

Status:

- `[ ]` not done yet

## Phase 5. Runtime Loading

### 5.1 Validate current loader assumptions

File:

- `src/humblenet/src/libwebrtc_dynamic.cpp`

Tasks:

- confirm expected library name on Linux is `libwebrtc.so`
- confirm search behavior is acceptable
- confirm fallback to microstack still works

Done when:

- runtime behavior is understood and reproduced intentionally

Status:

- `[x]` done at code-reading level
- loader behavior in `src/humblenet/src/libwebrtc_dynamic.cpp` remains the expected runtime model

### 5.2 Improve diagnostics if needed

File:

- `src/humblenet/src/libwebrtc_dynamic.cpp`

Tasks:

- add or refine logs for:
  - external backend load success
  - backend not found
  - missing symbol
  - fallback to microstack

Done when:

- logs make backend selection unambiguous during testing

Status:

- `[~]` partially done
- backend-side diagnostics were improved
- loader-specific diagnostic refinement is still open if runtime testing shows ambiguity

## Phase 6. Functional Tests

### 6.1 Run core WebRTC test

Test file:

- `tests/test_webrtc.cpp`

Tasks:

- build the test with external backend enabled
- run the test against the new backend
- verify stable bidirectional message exchange

Done when:

- the test passes reliably

Status:

- `[ ]` not done yet

### 6.2 Validate fallback mode

Relevant files:

- `src/humblenet/src/libwebrtc_dynamic.cpp`
- built runtime artifacts

Tasks:

- hide or remove `libwebrtc.so`
- rerun relevant path
- confirm HumbleNet falls back to microstack cleanly

Done when:

- fallback remains operational

Status:

- `[ ]` not done yet

### 6.3 Exercise failure paths

Targets:

- new backend implementation
- `tests/test_webrtc.cpp`

Tasks:

- validate malformed SDP handling
- validate malformed ICE candidate handling
- validate channel close behavior
- validate repeated connect/disconnect behavior

Done when:

- failure cases do not leak or crash

Status:

- `[ ]` not done yet

## Phase 7. Provider-Side Patches

### 7.1 Patch provider fork when build or packaging issues are provider-owned

Submodule:

- `3rdparty/webrtc-native-build`

Provider repo:

- `git@github.com:caiiiycuk/webrtc-native-build.git`

Tasks:

- patch provider build scripts when needed
- patch provider output layout when needed
- patch provider export/packaging behavior when needed

Rule:

- provider build problems belong in the provider fork
- HumbleNet ABI adaptation belongs in HumbleNet

Done when:

- HumbleNet does not carry provider-specific hacks that belong upstream in the fork

Status:

- `[~]` in progress
- one provider bootstrap issue was discovered: nested `3rdParty/depot_tools` was not initialized
- it was handled locally by initializing provider submodules
- no upstream patch to the fork has been committed from this repository yet

### 7.2 Update pinned submodule commit

Files:

- submodule pointer in the main repository

Tasks:

- commit provider changes in the fork
- advance the submodule pointer intentionally
- keep HumbleNet changes aligned with the pinned provider version

Done when:

- the repository reproduces the exact provider behavior needed

Status:

- `[ ]` not done yet

## Phase 8. CI

### 8.1 Add Linux external-WebRTC job

Likely file:

- `.github/workflows/humblenet-linux.yml`

Tasks:

- optionally initialize submodules
- build provider artifacts or consume pinned outputs
- build HumbleNet external backend
- run `tests/test_webrtc.cpp`

Done when:

- CI covers the new Linux path

Status:

- `[ ]` not done yet

### 8.2 Preserve existing default path

Likely file:

- `.github/workflows/humblenet-linux.yml`

Tasks:

- make sure current non-external build path still works

Done when:

- migration does not silently replace baseline coverage

Status:

- `[~]` partially done
- local CMake verification shows the default path still configures successfully
- CI confirmation is still pending

## Phase 9. Documentation

### 9.1 Update project docs

Likely files:

- `README.md`
- possibly a dedicated Linux WebRTC doc later

Tasks:

- document submodule requirement
- document provider fork usage
- document external backend build flags
- document runtime placement of `libwebrtc.so`
- document fallback behavior

Done when:

- another engineer can build and run the Linux external backend from docs

Status:

- `[~]` partially done
- internal migration/design/checklist docs are updated
- public README-level usage docs are still pending

## File-by-File Summary

### Files very likely to change

- `.gitmodules`
- `CMakeLists.txt`
- `src/humblenet/CMakeLists.txt`
- `src/humblenet/src/libwebrtc_dynamic.cpp`
- `src/humblenet/webrtc/exported_linux.map`
- `tests/test_webrtc.cpp`
- `.github/workflows/humblenet-linux.yml`
- `README.md`

### Files likely to be added

- `3rdparty/webrtc-native-build` as submodule
- `src/humblenet/webrtc/webrtc_native_linux.cpp`
- optional helper headers under `src/humblenet/webrtc/`

### Files to treat as reference, not foundation

- `src/humblenet/webrtc/webrtc_native.cpp`
- `3rdparty/chromium_webrtc/CMakeLists.txt`

## Execution Order

1. Add and pin provider submodule
2. Inspect provider outputs
3. Add CMake option and path wiring
4. Implement new Linux backend source
5. Build `libwebrtc.so`
6. Verify runtime loading
7. Run `tests/test_webrtc.cpp`
8. Patch provider fork if needed
9. Add CI coverage
10. Update documentation

## Definition of Done

The work is done when:

- the submodule is present and pinned
- the Linux backend builds from documented inputs
- external `libwebrtc.so` loads successfully
- WebRTC test passes with external backend
- fallback to microstack still works
- CI documents or validates the path
- implementation and usage are documented
