```markdown
# The Dynamic Linking Journey
(Bypassing Project Treble Namespace Locks)

One of the most complex challenges on modern Android versions (Android 8.0+ to Android 15) is **Project Treble's dynamic loader namespace isolation**.

Normally, applications are strictly forbidden from dynamically loading shared objects (`.so`) placed in arbitrary, writable system partitions like `/data/local/tmp/`. Linker namespaces trigger a hard segfault (`dlopen failed: library not found / permission denied`).

```
[APK Packaging]
       │ (Assets Compressed)
       ▼
[Dynamic Library assets copy] ────> Relocated to context.getFilesDir() + "/lib/"
                                        │
                                        ▼ (This folder IS writable & executable!)
                             [Safe, Un-sandboxed dlopen() load]
```

### 🔐 TEE/StrongBox AES Hardware Keys
To safeguard Unix Domain Socket IPC pipelines from adjacent malicious processes, NACL initializes an **AES-256-GCM** encryption boundary. Rather than storing keys in software assets, the host application executes an Android Keystore generation pipeline that anchors a **hardware-backed key inside the Secure Element/TEE**.

When a native daemon starts, the host application exports this key directly down the secure JNI boundary using `nativeBootRuntime` to initialize our client-daemon AES channels.

---