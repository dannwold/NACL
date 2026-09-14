```markdown
# On-Device Verification & Troubleshooting Playbook
Once Jules has compiled the dynamic dynamic binaries (`.so` files) and standalone daemon executables, developers can execute a rigorous, five-step hardware verification loop inside an on-device local CLI environment like **Termux**:

### 🔍 Step 1: Pre-Flight Daemon Launch
Launch the background hardware telemetry daemon using the direct, privilege-escalated CLI path. This maps a local Unix socket to begin physical sensor reads:
```bash
# Verify the socket target directory is writable under locally tmp boundaries
mkdir -p /data/local/tmp/sdk/sockets
chmod 777 /data/local/tmp/sdk/sockets

# Launch the background telemetry daemon in background daemon state
./sensors_daemon &
```

### 📊 Step 2: Validate Event Sockets & Active Channels
Verify that the daemon has successfully established a listening boundary on the Unix domain socket path:
```bash
# Check if the domain socket exists and is in active listening state
ls -la /data/local/tmp/sdk/sockets/sensors.sock

# Monitor local open network links and socket descriptors
ss -a -x | grep "sensors.sock"
```

### 🛰️ Step 3: Stream and Unpack Raw Binary Data
Use our mock test client to hook directly into the IPC stream and verify that data packets are flowing at 50Hz:
```bash
# Launch the client module mock binary to stream raw packets
./mock_client_main

# Expected Output (Continuous high-frequency streaming):
# [SensorsClient] Connected to Unix Domain Socket!
# [SensorsClient] Received Frame - Subsystem: 3, Command: 400, Timestamp: 30489572910, X: 0.124, Y: 9.812, Z: -0.420
# [SensorsClient] Received Frame - Subsystem: 3, Command: 400, Timestamp: 30489592911, X: 0.126, Y: 9.810, Z: -0.418
```

### 📜 Step 4: Interleaved Log Inspection
To inspect synchronization streams between our multi-threaded daemon instances and the host JVM controller, run the automated diagnostic log interpreter:
```bash
# Execute the multi-process logs orchestrator
./multi_process_debug.sh

# Expected Log Stream:
# [NATIVE_SYS] [SensorsDaemon] ASensorEventQueue initialized. Read file descriptor registered.
# [NATIVE_SYS] [SensorsDaemon] Client registered. Streaming acceleration packets to fd 12.
# [HOST_JVM]   [NaclBridge] Connected to dynamic native backend. Hot SharedFlow initialized.
# [HOST_JVM]   [SignalGaugeWidget] Collector thread received X=0.124 Y=9.812. UI Redrawn.
```

---