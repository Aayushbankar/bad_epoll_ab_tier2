# Reproducibility Guide (Android 14 GKI Baseline)

## Environment Variables
- `ANDROID_HOME`: `~/.local/android`
- `JAVA_HOME`: `~/.local/java/jdk-21.0.2`

## Target Acquisition
1. Downloaded System Image: `system-images;android-34;google_apis;arm64-v8a` via `sdkmanager`
2. Extracted pure kernel version using zcat on `kernel-ranchu`:
   `zcat ~/.local/android/system-images/android-34/google_apis/arm64-v8a/kernel-ranchu | strings | grep "Linux version"`
   - Result: `6.1.23-android14-4-00257-g7e35917775b8-ab9964412`

## Source Code Synchronization
The exact AOSP kernel commit is `7e35917775b8`. Because the `vmlinux` artifacts on `ci.android.com` for Build ID `9964412` have expired (404 NoSuchKey), we synchronize the exact branch and will rebuild from source:
```bash
repo init -u https://android.googlesource.com/kernel/manifest -b common-android14-6.1
repo sync -c -j4
git checkout 7e35917775b8
```

## Emulator Workaround Execution
The standard SDK `emulator` refuses to boot `arm64-v8a` on `x86_64`. To boot the pure Android kernel:
```bash
qemu-system-aarch64 \
    -machine virt \
    -cpu cortex-a57 \
    -smp 2 \
    -m 2048 \
    -kernel ~/.local/android/system-images/android-34/google_apis/arm64-v8a/kernel-ranchu \
    -initrd ~/.local/android/system-images/android-34/google_apis/arm64-v8a/ramdisk.img \
    -nographic \
    -append "console=ttyAMA0 earlycon=pl011,0x9000000" \
    -s
```
*(The `-s` flag enables the GDB stub on port 1234).*

## Hardware Verification Extractions
Config was extracted perfectly via standard Linux scripts:
```bash
extract-ikconfig ~/.local/android/system-images/android-34/google_apis/arm64-v8a/kernel-ranchu > config.txt
```
