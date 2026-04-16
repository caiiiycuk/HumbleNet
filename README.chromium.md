# Chromium WebRTC Build Notes

This document explains how to build HumbleNet with the external Chromium-based `libwebrtc` backend and how to verify that this path is actually being used.

Important: in this repository, "chromium-webrtc" means the new Linux path through the external provider in `3rdparty/webrtc-native-build`. The old in-tree target in `3rdparty/chromium_webrtc` is marked obsolete in the planning documents and should not be used as the main workflow.

## Expected Outcome

The intended flow is:

1. `3rdparty/webrtc-native-build` builds and installs upstream Chromium WebRTC artifacts into `dist/<BuildType>/webrtc-native-build-<version>/`.
2. The main HumbleNet project builds its own adapter `libwebrtc.so` from `src/humblenet/webrtc/webrtc_native_linux.cpp`.
3. `humblenet_test_webrtc.bin.x86_64` runs next to that `libwebrtc.so`.
4. During startup, the dynamic loader in `src/humblenet/src/libwebrtc_dynamic.cpp` prints `external webrtc implementation loaded`, which confirms that the external backend is being used instead of the microstack fallback.

## Prerequisites

- Platform: Linux.
- The `3rdparty/webrtc-native-build` submodule must be initialized.
- The provider build requires the dependencies listed in [`3rdparty/webrtc-native-build/README.md`](/home/caiiiycuk/js-dos/HumbleNet/3rdparty/webrtc-native-build/README.md):

```bash
sudo apt-get install ninja-build cmake build-essential libssl-dev libboost-all-dev
sudo apt-get install libglib2.0-dev libgtk-3-dev libpulse-dev libasound2-dev
```

You can build the provider manually if needed, but the repository already contains helper targets in the root `CMakeLists.txt`, so the recommended workflow below goes through the main project.

## Recommended Build Through The Main CMake Project

### 1. Configure a dedicated build directory

```bash
cmake -S . -B build-chromium \
  -DCMAKE_BUILD_TYPE=Release \
  -DHUMBLENET_EXTERNAL_WEBRTC=ON
```

This flag does the following:

- enables the Linux external WebRTC path;
- adds helper targets:
  - `humblenet_external_webrtc_provider_configure`
  - `humblenet_external_webrtc_provider_build`
  - `humblenet_external_webrtc_provider_install`
- enables provider autodetection under `3rdparty/webrtc-native-build/dist/Release/webrtc-native-build-<version>/`.

During the first configure, it is acceptable to see a warning about missing provider headers. That is expected: the path is intentionally bootstrap-safe before the provider artifacts are installed for the first time.

### 2. Build and install the external provider

```bash
cmake --build build-chromium --target humblenet_external_webrtc_provider_install -j
```

After this step, the provider should contain:

- `3rdparty/webrtc-native-build/dist/Release/webrtc-native-build-<version>/include`
- `3rdparty/webrtc-native-build/dist/Release/webrtc-native-build-<version>/lib`

Expected libraries:

- `libwebrtc.a`
- `libboringssl.a`
- `libyuv_internal.a`

Linux system dependencies are added from the root `CMakeLists.txt`: `dl`, `X11`, `expat`.

### 3. Run configure again

This is an important step.

```bash
cmake -S . -B build-chromium \
  -DCMAKE_BUILD_TYPE=Release \
  -DHUMBLENET_EXTERNAL_WEBRTC=ON
```

The second configure is needed because the `humblenet_external_webrtc` target is created only after the provider include/lib directories already exist. Without this step, the main `libwebrtc.so` may not switch to the new backend path.

### 4. Build the backend and the test binary

```bash
cmake --build build-chromium --target webrtc_shared humblenet_test_webrtc -j
```

At minimum, the following files should appear in `build-chromium/`:

- `libwebrtc.so`
- `humblenet_test_webrtc.bin.x86_64`

## Manual Path Overrides

If autodetection is not suitable, you can pass the paths explicitly:

```bash
cmake -S . -B build-chromium \
  -DCMAKE_BUILD_TYPE=Release \
  -DHUMBLENET_EXTERNAL_WEBRTC=ON \
  -DHUMBLENET_EXTERNAL_WEBRTC_INCLUDE_DIR="$PWD/3rdparty/webrtc-native-build/dist/Release/webrtc-native-build-<version>/include" \
  -DHUMBLENET_EXTERNAL_WEBRTC_LIB_DIR="$PWD/3rdparty/webrtc-native-build/dist/Release/webrtc-native-build-<version>/lib"
```

If necessary, you can also list libraries explicitly through `HUMBLENET_EXTERNAL_WEBRTC_LIBRARIES`, but in the normal Linux flow autodetection already assembles:

- `libwebrtc.a`
- `libboringssl.a`
- `libyuv_internal.a`
- `dl`
- `X11`
- `expat`

## How To Run The Check

This workflow does not use `ctest`: in `tests/CMakeLists.txt`, the test target is created as a regular executable. It should be run directly.

The simplest form is:

```bash
cd build-chromium
./humblenet_test_webrtc.bin.x86_64
```

Running from the build directory is convenient because the dynamic loader first tries to open `libwebrtc.so` by name and then attempts to open it next to the loaded module. When the binary and `libwebrtc.so` are in the same directory, the chance of silently falling back is much lower.

If you run the binary from another location, you need to make `libwebrtc.so` discoverable yourself, for example through `LD_LIBRARY_PATH`.

## How To Confirm The External Backend Is In Use

Signs that the external path is active:

- stdout/stderr contains `external webrtc implementation loaded`;
- you can see SDP/ICE logs from `tests/test_webrtc.cpp`, for example `sdp offer:` and `sdp answer:`;
- the test binary does not print `Could not open 'libwebrtc.so'`.

If `libwebrtc.so` is missing or fails to load, the code in [`src/humblenet/src/libwebrtc_dynamic.cpp`](/home/caiiiycuk/js-dos/HumbleNet/src/humblenet/src/libwebrtc_dynamic.cpp) falls back to the internal microstack implementation. In that case, the external Chromium path has not been validated.

## What Counts As A Successful Check

Minimum path validation:

1. The provider build completes successfully.
2. `libwebrtc.so` and `humblenet_test_webrtc.bin.x86_64` are built.
3. The process prints `external webrtc implementation loaded` during startup.

Full functional validation:

1. `humblenet_test_webrtc.bin.x86_64` completes all three runs.
2. The final output includes `3 / 3 test were successful`.
3. There are no `call setup timed out` messages.

## Known Limitation In The Current State

According to the planning documents, the project is currently at a stage where the build/link path works, but runtime behavior is not fully stabilized yet.

Documented limitation:

- the external backend does load successfully;
- modern SDP generation has been confirmed;
- in some Linux environments, `BasicNetworkManager` sees a zero-network condition, which prevents ICE gathering and causes the test to end with `call setup timed out`.

This means:

- `external webrtc implementation loaded` confirms that the Chromium path is active;
- `call setup timed out` in the current environment does not necessarily mean the build or link step is wrong;
- for full end-to-end validation, it is better to use a Linux host or container with a working network interface rather than an environment with no detectable networks.

## Short End-To-End Example

```bash
cmake -S . -B build-chromium \
  -DCMAKE_BUILD_TYPE=Release \
  -DHUMBLENET_EXTERNAL_WEBRTC=ON

cmake --build build-chromium --target humblenet_external_webrtc_provider_install -j

cmake -S . -B build-chromium \
  -DCMAKE_BUILD_TYPE=Release \
  -DHUMBLENET_EXTERNAL_WEBRTC=ON

cmake --build build-chromium --target webrtc_shared humblenet_test_webrtc -j

cd build-chromium
./humblenet_test_webrtc.bin.x86_64
```
