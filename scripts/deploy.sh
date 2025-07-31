#!/bin/sh

set -ex
#scp -i ../cloud/etc/yandex-vm cmake-build-debug-clang/peer-server.bin.x86_64  cloud.js-dos.com:~/peer-server.new
scp -i ../cloud/etc/yandex-vm cmake-build-release-clang/peer-server.bin.x86_64  cloud.js-dos.com:~/peer-server.new
