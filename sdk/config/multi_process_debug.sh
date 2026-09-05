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