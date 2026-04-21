сщьь# WebRTC Chromium Roadmap

This document is the single source of truth for the remaining work on the Linux external Chromium WebRTC path.

## Goal

Finish the Linux external WebRTC integration so that WebRTC-NET:

- loads external `libwebrtc.so`;
- passes `tests/test_webrtc.cpp` with the external backend;
- falls back cleanly to the internal microstack backend when the external library is absent or incompatible;
- is covered by CI;
- remains documented clearly enough for another engineer to build and verify it.

## Current State

Build and link integration are already working. The remaining work is runtime stabilization and verification.

Observed runtime state:

- external `libwebrtc.so` loads successfully;
- modern SDP generation is confirmed;
- the current environment can expose a zero-network condition in `rtc::BasicNetworkManager`;
- the visible failure mode is `call setup timed out` in `humblenet_test_webrtc.bin.x86_64`.

## Remaining Work

### 1. Runtime Stabilization

- stabilize runtime behavior in `src/humblenet/webrtc/webrtc_native_linux.cpp`;
- determine why local peer setup does not reliably reach the established/datachannel-ready state;
- confirm exported-symbol coverage against `src/humblenet/webrtc/exported_linux.map`;
- improve diagnostics only where backend selection or connection failure causes are still ambiguous.

### 2. Callback And Lifecycle Validation

Validate the WebRTC-NET callback contract from `src/humblenet/src/libwebrtc.h`, especially:

- local description;
- ICE candidate delivery;
- `LWRTC_CALLBACK_ESTABLISHED`;
- `LWRTC_CALLBACK_CHANNEL_ACCEPTED`;
- `LWRTC_CALLBACK_CHANNEL_CONNECTED`;
- `LWRTC_CALLBACK_CHANNEL_RECEIVE`;
- `LWRTC_CALLBACK_CHANNEL_CLOSED`;
- `LWRTC_CALLBACK_DISCONNECTED`;
- `LWRTC_CALLBACK_DESTROY`.

Special attention is still required for:

- callback ordering;
- exactly-once destroy behavior;
- clean shutdown with no double-destroy;
- close during negotiation;
- close after partial setup;
- channel-close versus connection-close sequencing;
- cleanup with pending callbacks.

### 3. Functional Test Validation

- make `tests/test_webrtc.cpp` pass reliably with the external backend;
- verify stable bidirectional message exchange;
- verify repeated connect/disconnect cycles;
- validate STUN-only behavior;
- validate TURN-enabled behavior if it is required for migration acceptance.

### 4. Failure-Path Validation

- validate malformed SDP handling;
- validate malformed ICE candidate handling;
- validate send/close behavior on closed or failed channels;
- validate provider runtime failures that require teardown.

### 5. Environment And ICE

- investigate the zero-network `BasicNetworkManager` condition in the current environment;
- decide whether stabilization should rely on:
  - fixing the current environment;
  - a backend-side test fallback;
  - a different validation environment with a detectable network interface;
- rerun the external backend tests in a known-good Linux environment.

### 6. Fallback Verification

- run the relevant path without `libwebrtc.so`;
- confirm clean fallback to the microstack backend;
- confirm fallback behavior remains compatible with existing expectations.

### 7. Provider Follow-Up

- if remaining issues are provider-owned, patch `3rdparty/webrtc-native-build` in the fork rather than adding WebRTC-NET-specific hacks;
- update the pinned provider submodule revision intentionally if provider fixes are required.

### 8. CI

- add Linux CI coverage for the external WebRTC path;
- run the external backend test path in CI;
- keep the default non-external build path covered.

### 9. Documentation

- keep `README.chromium.md` aligned with the actual build and test flow;
- decide whether `README.md` also needs a short summary of the external Chromium WebRTC workflow.

## Open Technical Questions

- Is ICE candidate routing already sufficient for the local test case, or is the failure earlier in the connection lifecycle?
- Should the connection tolerate candidate delivery before the remote description is fully applied?
- Should zero-network local validation be solved by environment choice or by a test-only backend fallback?
- Is the current logging already sufficient to explain backend selection and connection failure causes in real test runs?

## Definition Of Done

This work is not done until all of the following are true:

- external `libwebrtc.so` loads and is used intentionally;
- `tests/test_webrtc.cpp` passes reliably with the external backend;
- fallback to microstack is verified explicitly;
- required provider fixes are pinned reproducibly;
- CI covers the Linux external path;
- build and verification steps remain documented.
