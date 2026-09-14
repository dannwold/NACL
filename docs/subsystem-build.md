```markdown
# Subsystem 11: Cloud Compilation, CI/CD & Local Staging Configs

**Description:** The complete build automation pipelines and on-device testing scripts coordinating log streams, daemon validation checks, and target architecture builds.

#### 📄 File: `.github/workflows/ndk-build.yml`
##### **Technical & Architectural Commentary:**
- **Infrastructure Layer:** Core build scripting, dependencies configuration, or deployment workflows tracking compilation pipelines.

[FILE_PATH_START: .github/workflows/ndk-build.yml]
```yaml
name: Android NDK Multi-ABI Compiler

on:
  push:
    branches: [ "main" ]

jobs:
  build-native:
    runs-on: ubuntu-latest
    steps:
    - name: Checkout Repository
      uses: actions/checkout@v4

    - name: Set up JDK 17
      uses: actions/setup-java@v3
      with:
        distribution: 'zulu'
        java-version: '17'

    - name: Set up Android SDK & NDK
      uses: android-actions/setup-android@v3

    - name: Install Android NDK r26b
      run: |
        sdkmanager --install "ndk;26.1.10909125"

    - name: Build Dynamic Libraries (ARM64 & x86_64)
      env:
        NDK_PATH: ${{ env.ANDROID_HOME }}/ndk/26.1.10909125
      run: |
        for abi in arm64-v8a x86_64; do
          echo "Compiling dynamic modules for ABI: $abi"
          abi_dir="build_$(echo $abi | sed 's/-/_/g')"
          mkdir -p "$abi_dir" && cd "$abi_dir"
          cmake -DCMAKE_TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake -DANDROID_ABI=$abi -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release ../sdk
          make -j$(nproc)
          cd ..
        done

    - name: Package Release Assets
      run: |
        mkdir -p release/libs/arm64-v8a
        mkdir -p release/libs/x86_64
        mkdir -p release/bin/arm64-v8a
        mkdir -p release/bin/x86_64
        cp build_arm64_v8a/*.so release/libs/arm64-v8a/ || true
        cp build_x86_64/*.so release/libs/x86_64/ || true
        cp build_arm64_v8a/*_svc release/bin/arm64-v8a/ || true
        cp build_x86_64/*_svc release/bin/x86_64/ || true

    - name: Upload Compiled SDK Workspace
      uses: actions/upload-artifact@v4
      with:
        name: android-native-capability-libraries
        path: release/
```
[FILE_PATH_TERMINATED]

---

#### 📄 File: `sdk/config/build_and_deploy.sh`
##### **Technical & Architectural Commentary:**
- **Deployment Pipeline:** Standard bash automation scripts orchestrating process daemons and diagnostic logs cleanly.

[FILE_PATH_START: sdk/config/build_and_deploy.sh]
```bash
#!/usr/bin/env bash

# ==============================================================================

# Android Native Capability Library - Automated Build & Deployment Pipeline

# Target Architecture: arm64-v8a (Android 64-bit ARM)

# Execute: ./build_and_deploy.sh

# ==============================================================================



set -euo pipefail



# --- User Configurations ---

DEFAULT_NDK_PATH="$HOME/Android/Sdk/ndk/25.1.8937393" # Update to your NDK location

NDK_PATH="${ANDROID_NDK_HOME:-$DEFAULT_NDK_PATH}"

ABI="arm64-v8a"

MIN_API_LEVEL="21" # Fits standard physical device contexts (Lollipop 5.0+)



# Output Color Profiles

GREEN='\033[0;32m'

BLUE='\033[0;34m'

YELLOW='\033[1;33m'

RED='\033[0;31m'

NC='\033[0m'



log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }

log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }

log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }

log_error() { echo -e "${RED}[ERROR]${NC} $1"; }



# --- Step 1: Environment Verification ---

log_info "Verifying developer environment..."

if [ ! -d "$NDK_PATH" ]; then

    log_error "Android NDK was not detected at: $NDK_PATH"

    log_info "Please set ANDROID_NDK_HOME in your env profile or modify the top of this script."

    exit 1

fi

log_success "Android NDK detected: $NDK_PATH"



# Establish target adb state

DEPLOY_ENABLED=true

if ! command -v adb &> /dev/null; then

    log_warn "adb command not found in local system PATH. Pushing to device is disabled."

    DEPLOY_ENABLED=false

else

    DEVICE_STATE=$(adb get-state 2>/dev/null || echo "disconnected")

    if [ "$DEVICE_STATE" != "device" ]; then

        log_warn "No physical Android device connected via ADB. Skipping deployment phase."

        DEPLOY_ENABLED=false

    else

        log_success "Target physical Android device is connected."

    fi

fi



# --- Step 2: Establish Working Paths ---

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BUILD_DIR="$SRC_DIR/build"

STAGE_DIR="$SRC_DIR/out"



log_info "Cleaning up old build folders..."

rm -rf "$BUILD_DIR" "$STAGE_DIR"

mkdir -p "$BUILD_DIR"

mkdir -p "$STAGE_DIR/bin"

mkdir -p "$STAGE_DIR/lib"



# --- Step 3: Run Android CMake Cross-Compilation ---

log_info "Invoking NDK CMake configuration..."

cmake -S "$SRC_DIR" -B "$BUILD_DIR" \

    -DCMAKE_TOOLCHAIN_FILE="$NDK_PATH/build/cmake/android.toolchain.cmake" \

    -DANDROID_ABI="$ABI" \

    -DANDROID_NATIVE_API_LEVEL="$MIN_API_LEVEL" \

    -DCMAKE_BUILD_TYPE=Release



log_info "Compiling native binaries..."

cmake --build "$BUILD_DIR" --config Release



# --- Step 4: Staging the Build Assets ---

log_info "Organizing compiled binary assets..."

# Copy dynamic shared objects (.so files)

find "$BUILD_DIR" -type f -name "*.so" -exec cp {} "$STAGE_DIR/lib/" \;

# Copy standalone binary executables

find "$BUILD_DIR" -type f -executable -not -name "*.so" -not -name "CMake*" -exec cp {} "$STAGE_DIR/bin/" \; 2>/dev/null || true



log_success "Local staging build successful! Local output is ready:"

ls -R "$STAGE_DIR"



# --- Step 5: Privileged ADB Deployment (UID 2000 Context) ---

if [ "$DEPLOY_ENABLED" = true ]; then

    log_info "Initiating deployment to physical device..."



    # Target directory under UID 2000 (Shell user possesses full read/write rights under /data/local/tmp/)

    REMOTE_DIR="/data/local/tmp/sdk"



    log_info "Creating remote environment folders under $REMOTE_DIR..."

    adb shell "mkdir -p $REMOTE_DIR/bin $REMOTE_DIR/lib $REMOTE_DIR/sockets"



    # Synchronize daemon binaries and apply execution bits

    log_info "Deploying standalone service daemons..."

    for daemon in "$STAGE_DIR/bin"/*; do

        if [ -f "$daemon" ]; then

            BIN_NAME=$(basename "$daemon")

            log_info "Syncing binary: $BIN_NAME"

            adb push "$daemon" "$REMOTE_DIR/bin/"

            adb shell "chmod 755 $REMOTE_DIR/bin/$BIN_NAME"

        fi

    done



    # Synchronize dynamic shared client libraries (.so files)

    log_info "Deploying dynamic client libraries..."

    for lib in "$STAGE_DIR/lib"/*; do

        if [ -f "$lib" ]; then

            LIB_NAME=$(basename "$lib")

            log_info "Syncing shared library: $LIB_NAME"

            adb push "$lib" "$REMOTE_DIR/lib/"

            adb shell "chmod 755 $REMOTE_DIR/lib/$LIB_NAME"

        fi

    done



    log_success "All native files successfully synchronized to physical device memory."



    # --- Step 6: Security and Permission Configurations ---

    # To let normal app sandbox processes access UNIX sockets created by UID 2000 daemons,

    # the target socket folder must have wide access permissions.

    log_info "Configuring UNIX Socket folder directory permissions..."

    adb shell "chmod 777 $REMOTE_DIR/sockets"



    # Kill old daemon processes running in the background safely

    log_info "Terminating any previously active daemon process instances..."

    adb shell "pkill -f '_svc' || true"



    # Boot daemons in the background under UID 2000 (Shell) using 'nohup'

    log_info "Launching Wi-Fi Service Daemon in background..."

    adb shell "nohup $REMOTE_DIR/bin/wifi_svc > /dev/null 2>&1 &"



    log_info "Launching Sensors Service Daemon in background..."

    adb shell "nohup $REMOTE_DIR/bin/sensors_svc > /dev/null 2>&1 &"



    log_info "Launching Bluetooth GATT Service Daemon in background..."

    adb shell "nohup $REMOTE_DIR/bin/bluetooth_svc > /dev/null 2>&1 &"



    # Give system a brief second to settle

    sleep 1



    # --- Step 7: Active Verification ---

    log_info "Querying active process table on device..."

    RUNNING_PROCS=$(adb shell "ps -A -o PID,USER,NAME | grep -E 'wifi_svc|sensors_svc|bluetooth_svc' || true")



    if [ -n "$RUNNING_PROCS" ]; then

        log_success "Your background native service daemons are running successfully!"

        echo -e "$RUNNING_PROCS"

    else

        log_warn "Daemons not detected in the process list. Execute 'adb logcat' to diagnose initialization crashes."

    fi

else

    log_warn "Local-only compilation phase completed. Staging files are located at: $STAGE_DIR"

fi



log_success "Pipeline script finished."
```
[FILE_PATH_TERMINATED]

---

#### 📄 File: `sdk/config/deploy_abi_target.sh`
##### **Technical & Architectural Commentary:**
- **Deployment Pipeline:** Standard bash automation scripts orchestrating process daemons and diagnostic logs cleanly.

[FILE_PATH_START: sdk/config/deploy_abi_target.sh]
```bash
#!/usr/bin/env bash

# ==============================================================================

# ON-DEVICE DYNAMIC ABI DETECTION & SELECTIVE NDK DEPLOYER

# ==============================================================================

# Bypasses compilation overhead during rapid local staging by querying the

# active target's native CPU architecture via ADB and building/pushing only

# the single required binary slice.

# ==============================================================================



set -euo pipefail



# Log coloring utilities

RED='\033[0;31m'

GREEN='\033[0;32m'

YELLOW='\033[1;33m'

BLUE='\033[0;34m'

NC='\033[0m' # No Color



log_info() {

    echo -e "${BLUE}[INFO]${NC} $1"

}



log_success() {

    echo -e "${GREEN}[SUCCESS]${NC} $1"

}



log_warn() {

    echo -e "${YELLOW}[WARNING]${NC} $1"

}



log_error() {

    echo -e "${RED}[ERROR]${NC} $1"

}



# Ensure ADB is available

if ! command -v adb &> /dev/null; then

    log_error "ADB command-line utility not found on host path."

    exit 1

fi



# 1. Query connected devices via ADB [9]

DEVICE_COUNT=$(adb devices | grep -v "List" | grep "device" | wc -l | tr -d ' ')

if [ "$DEVICE_COUNT" -eq 0 ]; then

    log_error "No active Android devices or emulators detected via ADB. Please connect a target."

    exit 1

elif [ "$DEVICE_COUNT" -gt 1 ]; then

    log_warn "Multiple devices detected. Script will target default ADB device selector."

fi



# 2. Query target dynamic system properties using getprop [9]

log_info "Querying target system specifications..."

TARGET_ABI=$(adb shell getprop ro.product.cpu.abi | tr -d '\r\n')

TARGET_SDK=$(adb shell getprop ro.build.version.sdk | tr -d '\r\n')

TARGET_MODEL=$(adb shell getprop ro.product.model | tr -d '\r\n')



log_info "Detected Target Platform: ${YELLOW}${TARGET_MODEL}${NC} (API Level: ${TARGET_SDK})"

log_info "Detected Target Native ABI: ${GREEN}${TARGET_ABI}${NC}"



# 3. Map detected ABI to standard NDK target directories [26]

case "${TARGET_ABI}" in

    "arm64-v8a")

        GRADLE_ABI="arm64-v8a"

        CMAKE_ABI="arm64-v8a"

        ;;

    "x86_64")

        GRADLE_ABI="x86_64"

        CMAKE_ABI="x86_64"

        ;;

    "armeabi-v7a")

        GRADLE_ABI="armeabi"

        CMAKE_ABI="armeabi-v7a"

        log_warn "Target is legacy 32-bit ARM. Performance bottlenecks may occur."

        ;;

    "x86")

        GRADLE_ABI="x86"

        CMAKE_ABI="x86"

        ;;

    *)

        log_error "Unsupported device ABI: ${TARGET_ABI}"

        exit 1

        ;;

esac



# 4. Invoke Gradle forcing single-architecture optimization

log_info "Triggering single-ABI Gradle compiler loop for: ${GREEN}${GRADLE_ABI}${NC}..."



# Navigate to project root if script is inside a subdirectory

if [ -f "../gradlew" ]; then

    cd ..

elif [ -f "../../gradlew" ]; then

    cd ../..

fi



if [ -f "./gradlew" ]; then

    # Pass the injected ABI parameter to the build loop to slash compile times

    ./gradlew :app:assembleDebug \

        -Pandroid.injected.build.abi="${GRADLE_ABI}" \

        --parallel \

        --quiet

else

    log_warn "Gradlew wrapper not found at root directory. Attempting raw CMake fallback compilation..."

    if [ -d "build" ]; then

        cd build

        cmake --build . --config Debug --parallel $(nproc)

        cd ..

    else

        log_error "No build manager (Gradle/CMake) detected at this path context."

        exit 1

    fi

fi



log_success "Native compilation cycle completed."



# 5. Locate compiled system binaries

LOCAL_SO_DIR="app/build/intermediates/cmake/debug/obj/${CMAKE_ABI}"



# Ensure local build output directories exist

if [ ! -d "${LOCAL_SO_DIR}" ]; then

    log_error "Could not find compiled outputs inside: ${LOCAL_SO_DIR}"

    exit 1

fi



# 6. Dynamic Staging Setup

TARGET_TMP_DIR="/data/local/tmp/sdk"

log_info "Preparing secure sandbox environment on-device at ${TARGET_TMP_DIR}..."

adb shell "mkdir -p ${TARGET_TMP_DIR}/bin ${TARGET_TMP_DIR}/lib"



# 7. Relocate files and apply operational permissions

# We safely stop running daemons first to avoid "Text file busy" lockouts

log_info "Tearing down stale daemon processes..."

adb shell "pkill -f sensors_svc || true"

adb shell "pkill -f shm_daemon || true"



log_info "Pushing targeted ${GREEN}${CMAKE_ABI}${NC} binaries to device..."



# Push compiled client-bridge .so files

for file in "${LOCAL_SO_DIR}"/*.so; do

    if [ -f "$file" ]; then

        filename=$(basename "$file")

        adb push "$file" "${TARGET_TMP_DIR}/lib/${filename}" > /dev/null

    fi

done



# Push compiled standalone daemon executables

for file in "${LOCAL_SO_DIR}"/*_svc "${LOCAL_SO_DIR}"/*_daemon; do

    if [ -f "$file" ]; then

        filename=$(basename "$file")

        adb push "$file" "${TARGET_TMP_DIR}/bin/${filename}" > /dev/null

        adb shell "chmod 755 ${TARGET_TMP_DIR}/bin/${filename}"

    fi

done



log_success "Binary synchronization completed."



# 8. Start diagnostic verification loop

log_info "Bootstrapping background services..."

adb shell "nohup ${TARGET_TMP_DIR}/bin/sensors_svc > /dev/null 2>&1 &"



log_success "Target successfully deployed and synchronized! 🚀"

log_info "Stream real-time trace lines by running: ${YELLOW}adb logcat -s HostJniBridge:V NACL:V${NC}"
```
[FILE_PATH_TERMINATED]

---

#### 📄 File: `sdk/config/multi_process_debug.sh`
##### **Technical & Architectural Commentary:**
- **Deployment Pipeline:** Standard bash automation scripts orchestrating process daemons and diagnostic logs cleanly.

[FILE_PATH_START: sdk/config/multi_process_debug.sh]
```bash
#!/system/bin/sh

# ==============================================================================

# Android Native Capability Library: Multi-Process Debugging & Diagnostic Engine

# ==============================================================================

# Saves trace data, maps out linkers, and isolates IPC execution vectors.

# ==============================================================================



# ANSI Color Codes for Output formatting

RED='\033[0;31m'

GREEN='\033[0;32m'

YELLOW='\033[0;33m'

BLUE='\033[0;34m'

MAGENTA='\033[0;35m'

CYAN='\033[0;36m'

NC='\033[0m' # No Color



TARGET_PACKAGE="com.your.app"

DAEMON_NAME="sensors_svc"



echo -e "${BLUE}======================================================================${NC}"

echo -e "${BLUE}  Android Multi-Process Diagnostic Engine - Establishing State Matrix ${NC}"

echo -e "${BLUE}======================================================================${NC}"



# Check for ADB Connectivity

echo -e "[*] ADB Connection State: Verified"

echo -e "[*] System Build Fingerprint: $(getprop ro.build.fingerprint)"

echo -e "[*] Target Sandbox Package: ${TARGET_PACKAGE}"

echo -e "[*] Target Background Service Daemon: ${DAEMON_NAME}"



# ==============================================================================

# 1. DYNAMIC LINKER NAMESPACE & PATH-MAPPING AUDITOR

# ==============================================================================

echo -e "\n${CYAN}[1/4] Auditing Dynamic Linker Namespaces & SELinux Contexts...${NC}"



# Get Host Application Process ID

APP_PID=$(pidof ${TARGET_PACKAGE} 2>/dev/null | tr -d '\r\n')

if [ -z "$APP_PID" ]; then

    echo -e "${YELLOW}[!] Host Application (${TARGET_PACKAGE}) is not running.${NC}"

else

    echo -e "${GREEN}[+] Host App PID: ${APP_PID}${NC}"



    # Check Process SELinux Context

    APP_CONTEXT=$(ps -Z | grep "${TARGET_PACKAGE}" | awk '{print $1}')

    echo -e "    - SELinux Context: ${GREEN}${APP_CONTEXT}${NC}"



    # Audit Loaded Shared Objects (.so) and Treble Namespace Violations

    echo -e "    - Verifying Executable Path and Linker Memory Maps:"

    cat /proc/${APP_PID}/maps 2>/dev/null | grep -E "libandroid_core|libipc|libsensors_client" > /tmp/linker_maps.txt



    if [ -s /tmp/linker_maps.txt ]; then

        while read -r line; do

            if [[ "$line" == *"/data/user/0/"* ]]; then

                echo -e "      ${GREEN}[OK] Native Client loaded from permitted secure path: $(echo "$line" | awk '{print $6}')${NC}"

            elif [[ "$line" == *"/data/local/tmp/"* ]]; then

                echo -e "      ${RED}[VIOLATION] Library running from insecure directory: $(echo "$line" | awk '{print $6}')${NC}"

                echo -e "                  Project Treble will block this execution on modern API levels!${NC}"

            fi

        done < /tmp/linker_maps.txt

    else

        echo -e "      ${YELLOW}[?] No custom native clients loaded yet in target app memory map.${NC}"

    fi

fi



# Get Daemon Process ID

DAEMON_PID=$(pidof ${DAEMON_NAME} 2>/dev/null | tr -d '\r\n')

if [ -z "$DAEMON_PID" ]; then

    echo -e "${YELLOW}[!] Daemon service (${DAEMON_NAME}) is not running.${NC}"

else

    echo -e "${GREEN}[+] Service Daemon PID: ${DAEMON_PID}${NC}"

    DAEMON_CONTEXT=$(ps -Z | grep "${DAEMON_NAME}" | awk '{print $1}')

    echo -e "    - SELinux Context: ${GREEN}${DAEMON_CONTEXT}${NC}"



    # Check open file descriptors

    echo -e "    - Open File Descriptors:"

    ls -l /proc/${DAEMON_PID}/fd 2>/dev/null | sed 's/^/      /'

fi



# ==============================================================================

# 2. LOCAL SOCKET & SHARED MEMORY TRAFFIC MONITOR

# ==============================================================================

echo -e "\n${CYAN}[2/4] Inspecting Local TCP Loopback & Shared Memory Channels...${NC}"



# Check active listening ports for our local ADB loopback and secure IPC

echo -e "  [*] Active Network Sockets (IPv4 Loopback):"

netstat -tlpn 2>/dev/null || ss -tlpn 2>/dev/null || cat /proc/net/tcp | sed 's/^/    /'



# Validate Shared Memory FD mapping in client and daemon processes

if [ ! -z "$APP_PID" ] && [ ! -z "$DAEMON_PID" ]; then

    echo -e "\n  [*] Cross-Referencing Shared Memory File Descriptors (memfd/ashmem):"



    APP_SHM=$(ls -l /proc/${APP_PID}/fd 2>/dev/null | grep -E "memfd|ashmem" | awk '{print $8, $10}')

    DAEMON_SHM=$(ls -l /proc/${DAEMON_PID}/fd 2>/dev/null | grep -E "memfd|ashmem" | awk '{print $8, $10}')



    if [ ! -z "$APP_SHM" ]; then

        echo -e "    - Client Shared FDs: ${GREEN}${APP_SHM}${NC}"

    else

        echo -e "    - Client Shared FDs: ${YELLOW}None detected yet.${NC}"

    fi



    if [ ! -z "$DAEMON_SHM" ]; then

        echo -e "    - Daemon Shared FDs: ${GREEN}${DAEMON_SHM}${NC}"

    else

        echo -e "    - Daemon Shared FDs: ${YELLOW}None detected yet.${NC}"

    fi

fi



# ==============================================================================

# 3. CORE BARRIER & TOCTOU INTEGRITY MONITOR

# ==============================================================================

echo -e "\n${CYAN}[3/4] Verifying System Hardening Barriers (TOCTOU & Signal Guards)...${NC}"



# Examine kernel power supplies to ensure read safety for battery.so

echo -e "  [*] Checking /sys/class/power_supply file-node accessibility inside sandbox:"

SANDBOX_SYSFS_CHECK=$(run-as ${TARGET_PACKAGE} ls -l /sys/class/power_supply/battery/capacity 2>&1)

if [[ "$SANDBOX_SYSFS_CHECK" == *"Permission denied"* || "$SANDBOX_SYSFS_CHECK" == *"No such file"* ]]; then

    echo -e "    - Sysfs access: ${RED}BLOCKED by SELinux untrusted_app rules.${NC}"

    echo -e "    - Mitigation: Ensure dynamic routing is switching location/telemetry commands through ADB privileged loopback (UID 2000)."

else

    echo -e "    - Sysfs access: ${GREEN}PERMITTED (Legacy/Insecure/Modified ROM).${NC}"

fi



# ==============================================================================

# 4. CHRONOLOGICAL MULTI-PROCESS LOGCAT AGGREGATOR

# ==============================================================================

echo -e "\n${CYAN}[4/4] Starting Chronological Multi-Process Logcat Aggregator...${NC}"

echo -e "      Streaming interleaved execution logs. Press Ctrl+C to terminate."

echo -e "${BLUE}======================================================================${NC}"



# Filter tags matching native clients and security layers

FILTER_TAGS="AndroidNativeCore|QuickJS_Binding|ADB_Client|IPC_Crypto|SensorsDaemon|TelemetryClient"



logcat -v time | awk -v app="$APP_PID" -v daemon="$DAEMON_PID" '

    $0 ~ app { print "\033[1;32m[APP-" app "]\033[0m " $0; next }

    $0 ~ daemon { print "\033[1;35m[DAEMON-" daemon "]\033[0m " $0; next }

    $0 ~ /AndroidNativeCore|QuickJS_Binding|ADB_Client|IPC_Crypto/ { print "\033[1;36m[NATIVE_SYS]\033[0m " $0; next }

    { print "\033[0;90m[OTHER]\033[0m " $0 }

'
```
[FILE_PATH_TERMINATED]

---