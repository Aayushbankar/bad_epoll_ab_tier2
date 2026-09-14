./run_qemu.sh > /dev/null 2>&1 &
Q=$!
sleep 5
stdbuf -o0 gdb -batch -q -ex "target remote :1234" -ex "quit" > /dev/null
kill $Q || true
pkill -f qemu-system-aarch64
