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