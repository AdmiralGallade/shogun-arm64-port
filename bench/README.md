# Trap-cost benchmark

Measures the cost of one guest-to-host trap — the thing that decides whether
the Shogun runtime holds 60 fps on device. It reproduces `guest.py`'s trap
mechanism exactly, in C, so the numbers are comparable with the Python harness.

## Why this number matters

The engine issues roughly **645 GL calls per frame**, and every one is a trap.
At 60 fps the whole frame budget is 16.7 ms, so trap cost is multiplied by 645
before it is spent.

## Measured so far

| Host | Path | ns/trap |
|---|---|---|
| CPython, x86-64 laptop | mode switch | ~12,000 |
| **C, x86-64 laptop** | **in-place, same mode** | **136** |
| C, x86-64 laptop | dispatch only (null shim) | 67 |
| C, x86-64 laptop | stop/restart | 260 |
| C, x86-64 laptop | mode switch (the common case) | 274 |
| C, arm64 Android | *not yet measured — that is what this is for* | ? |

At 274 ns the trap path costs 0.18 ms/frame, about 1% of the budget.

## Building for Android

You need the NDK and a unicorn built for `arm64-v8a`.

    git clone https://github.com/unicorn-engine/unicorn
    cd unicorn && mkdir build && cd build
    cmake .. -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
             -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
             -DCMAKE_BUILD_TYPE=Release -DUNICORN_ARCH=arm \
             -DCMAKE_INSTALL_PREFIX=$PWD/../../bench/unicorn
    cmake --build . -j && cmake --install .

Then the benchmark itself:

    cd bench && mkdir build && cd build
    cmake .. -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
             -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
             -DCMAKE_BUILD_TYPE=Release
    cmake --build . -j

## Running on the phone

    adb push trapbench ../unicorn/lib/libunicorn.so /data/local/tmp/
    adb shell chmod +x /data/local/tmp/trapbench
    adb shell LD_LIBRARY_PATH=/data/local/tmp /data/local/tmp/trapbench

Pin to a big core for a stable figure:

    adb shell taskset f0 /data/local/tmp/trapbench

## Reading the result

* Under ~500 ns/trap — the trap path is not a problem; build the runtime.
* 500-2000 ns — workable, but keep same-mode traps on the in-place fast path
  and consider batching GL state calls.
* Above ~2000 ns — 645 calls would cost over 1.3 ms/frame and rise from there;
  reduce trap frequency before going further.

A plain command-line binary is enough: this measures CPU and Unicorn overhead
only, and needs no Activity, GL context or asset pack.
