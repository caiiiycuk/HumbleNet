HumbleNet
=========

[![humblenet-linux](https://github.com/caiiiycuk/HumbleNet/actions/workflows/humblenet-linux.yml/badge.svg)](https://github.com/caiiiycuk/HumbleNet/actions/workflows/humblenet-linux.yml)

HumbleNet is a cross platform networking library that utilizes WebRTC and WebSockets to handle network communication.

Using WebRTC and Websockets allows HumbleNet to support not only traditional platforms such as Windows, OS X, and Linux, but also web platforms such as ASM.JS / Emscripten.

Language Support
----------------
HumbleNet is a simple clean C based API that allows for wrappers to be written for any language that offers C binding.

Included Language Bindings
==========================
* C/C++
  - a simple C header is available for both C and C++ development.  There is also a BSD socket wrapper that will redirect the C socket API through humblenet.
* C#
  - wrappers are included that work in Unity for its Desktop and WebGL platform support. They should also work in any other .NET application.

Demo applications
=================
We have several demos to show off integrating HumbleNet.

TestPeer
--------
This is a simple "chat" app in the tests folder for both C and C#.   The C app will run on Windows, MacOS X, Linux, and Emscripten.   The C# will run on .NET or Mono.

Quake 2
-------
We have a port of Quake2 (Based on [R1Q2](http://www.r1ch.net/stuff/r1q2/)) that includes HumbleNet networking and runs on Windows, Mac OS X, Linux and Emscripten.

[Quake 2 HumbleNet port](https://github.com/HumbleNet/quake2)

Quake 3
-------
We have a port of Quake3 (Based on [QuakeJS](http://quakejs.com/)) that includes HumbleNet networking and runs on Windows, Mac OS X, Linux and Emscripten.

[Quake 3 HumbleNet port](https://github.com/HumbleNet/quake3)

Build Instructions
==================

Project Dependencies:
---------------------

1. CMake (https://cmake.org/):
    - Linux: use your preferred package manager, like `apt-get`.
    - Mac: can be installed using `brew`.
    - Win: download the installer via site https://cmake.org/download/.
2. Make (https://www.gnu.org/software/make/manual/make.html)
    - Unix: already installed.
    - Win: you can use Cygwin.
3. FlatBuffers (https://google.github.io/flatbuffers/index.html)
    - Download the source code. The HumbleNet is way behind the current version of FlatBuffers. It's using the version 1.6.0 (currently 1.7.1).
        1. You can download/clone the repo, and revert the master to commit `81ecc98e023f85fe003a27e920e78b34db8a0087` [here](https://github.com/google/flatbuffers/commit/81ecc98e023f85fe003a27e920e78b34db8a0087#diff-e644a513ebf0d4b999ed39c245f8f3db).
        2. Or download the Release [here](https://github.com/google/flatbuffers/releases/tag/v1.6.0). This should work (not tested).
    - Follow the instructions here (https://google.github.io/flatbuffers/flatbuffers_guide_building.html);
    - You should get as result the `flatc` compiler executable.
    - Put your output directory (where the `flatc` exists, generally a `build` dir) into your `PATH`, so the compiler can be accessed anywhere.
4. Go Language [here](https://golang.org/)
    - Download and install the compiler via [download page](https://golang.org/dl/).
    - Test on your terminal if you can execute `go version`;
    - Tested here with version `go1.9.1 darwin/amd64`.

Compilation:
------------

Follow steps from [action file](https://github.com/caiiiycuk/HumbleNet/blob/master/.github/workflows/humblenet-linux.yml)

