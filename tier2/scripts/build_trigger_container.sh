#!/bin/bash
podman run --rm -v $(pwd):/work -w /work debian:bookworm-slim bash -c "
apt-get update -y && \
apt-get install -y gcc-aarch64-linux-gnu libc6-dev-arm64-cross && \
aarch64-linux-gnu-gcc -static tier2/exploit/reproducer/trigger2.c -pthread -o tier2/rootfs/trigger
"
