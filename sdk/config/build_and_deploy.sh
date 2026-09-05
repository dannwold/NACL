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