package com.your.app;



import android.content.Context;

import android.net.nsd.NsdManager;

import android.net.nsd.NsdServiceInfo;

import android.os.Bundle;

import android.util.Log;

import android.widget.Button;

import android.widget.EditText;

import android.widget.TextView;

import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;

import androidx.appcompat.app.AppCompatActivity;



import java.io.File;

import java.io.FileOutputStream;

import java.io.InputStream;

import java.io.OutputStream;



public class MainActivity extends AppCompatActivity {

    private static final String TAG = "HostAppBootstrapper";

    private NsdManager mNsdManager;

    private NsdManager.DiscoveryListener mDiscoveryListener;

    private int mResolvedAdbPort = -1;



    // Load our host JNI bridging library on startup

    static {

        System.loadLibrary("native_host_bridge");

    }



    // Native C++ declarations for the dynamic bootstrap layers

    private native boolean nativeBootRuntime(String secureLibPath, String secureKeyPath);

    private native boolean nativeAuthenticateADB(int port, String pairingCode);



    @Override

    protected void onCreate(Bundle savedInstanceState) {

        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);



        TextView statusText = findViewById(R.id.status_text);

        Button btnRelocate = findViewById(R.id.btn_relocate);

        Button btnDiscover = findViewById(R.id.btn_discover);

        Button btnPair = findViewById(R.id.btn_pair);



        // 1. Dynamic Treble Linker Relocation

        btnRelocate.setOnClickListener(v -> {

            boolean success = relocateLibraryAssets();

            if (success) {

                statusText.setText("Status: Modules Relocated Safely!");

                Toast.makeText(this, "Linker Relocation Complete", Toast.LENGTH_SHORT).show();

            } else {

                statusText.setText("Status: Relocation Failed!");

            }

        });



        // 2. Discover Local Wireless Debugging Port

        btnDiscover.setOnClickListener(v -> {

            statusText.setText("Status: Discovering ADB Service Port...");

            discoverAdbService();

        });



        // 3. Complete Handshake

        btnPair.setOnClickListener(v -> {

            if (mResolvedAdbPort == -1) {

                Toast.makeText(this, "Please discover active ADB ports first!", Toast.LENGTH_LONG).show();

                return;

            }

            promptPairingCode();

        });

    }



    /**

     * Bypasses Project Treble's dynamic linker namespace policies by copying

     * precompiled .so files from assets into the app's secure executable directory.

     */

    private boolean relocateLibraryAssets() {

        try {

            File targetDir = new File(getFilesDir(), "lib");

            if (!targetDir.exists() && !targetDir.mkdirs()) {

                return false;

            }



            // Target precompiled dynamic module list

            String[] libs = {"libsensors_client.so", "libbluetooth_client.so"};

            for (String libName : libs) {

                File outFile = new File(targetDir, libName);



                try (InputStream in = getAssets().open(libName);

                     OutputStream out = new FileOutputStream(outFile)) {

                    byte[] buffer = new byte[8192];

                    int read;

                    while ((read = in.read(buffer)) != -1) {

                        out.write(buffer, 0, read);

                    }

                }



                // Set executable permissions so Bionic dynamic loader can load it

                if (!outFile.setExecutable(true, true)) {

                    Log.e(TAG, "Failed to set execution permission for " + libName);

                    return false;

                }

            }



            // Boot our native QuickJS engine context pointing to relocations

            File keysDir = new File(getFilesDir(), "keys");

            if (!keysDir.exists()) keysDir.mkdirs();



            return nativeBootRuntime(targetDir.getAbsolutePath(), keysDir.getAbsolutePath());

        } catch (Exception e) {

            Log.e(TAG, "Asset relocation error: ", e);

            return false;

        }

    }



    /**

     * Discovers active dynamic ADB service ports over local loopback (NSD).

     */

    private void discoverAdbService() {

        mNsdManager = (NsdManager) getSystemService(Context.NSD_SERVICE);

        mDiscoveryListener = new NsdManager.DiscoveryListener() {

            @Override

            public void onStartDiscoveryFailed(String serviceType, int errorCode) {

                Log.e(TAG, "NSD Discovery failed: " + errorCode);

                mNsdManager.stopServiceDiscovery(this);

            }



            @Override

            public void onStopDiscoveryFailed(String serviceType, int errorCode) {

                mNsdManager.stopServiceDiscovery(this);

            }



            @Override

            public void onDiscoveryStarted(String serviceType) {

                Log.d(TAG, "ADB Port Discovery Started");

            }



            @Override

            public void onDiscoveryStopped(String serviceType) {

                Log.d(TAG, "Discovery Stopped");

            }



            @Override

            public void onServiceFound(NsdServiceInfo serviceInfo) {

                // Look for the wireless debugging service descriptor

                if (serviceInfo.getServiceType().equals("_adb-tls-connect._tcp.") ||

                    serviceInfo.getServiceType().equals("_adb._tcp.")) {

                    mNsdManager.resolveService(serviceInfo, new NsdManager.ResolveListener() {

                        @Override

                        public void onResolveFailed(NsdServiceInfo serviceInfo, int errorCode) {

                            Log.e(TAG, "Resolve failed: " + errorCode);

                        }



                        @Override

                        public void onServiceResolved(NsdServiceInfo resolvedInfo) {

                            mResolvedAdbPort = resolvedInfo.getPort();

                            runOnUiThread(() -> {

                                TextView statusText = findViewById(R.id.status_text);

                                statusText.setText("Status: ADB Discovered on Port: " + mResolvedAdbPort);

                                Toast.makeText(MainActivity.this, "Port Resolved: " + mResolvedAdbPort, Toast.LENGTH_SHORT).show();

                            });

                        }

                    });

                }

            }



            @Override

            public void onServiceLost(NsdServiceInfo serviceInfo) {

                Log.e(TAG, "Service lost: " + serviceInfo);

            }

        };



        mNsdManager.discoverServices("_adb-tls-connect._tcp.", NsdManager.PROTOCOL_DNS_SD, mDiscoveryListener);

    }



    private void promptPairingCode() {

        final EditText input = new EditText(this);

        new AlertDialog.Builder(this)

                .setTitle("Wireless Pair Handshake")

                .setMessage("Enter the 6-digit dynamic system debugging code:")

                .setView(input)

                .setPositiveButton("Authenticate", (dialog, which) -> {

                    String code = input.getText().toString().trim();

                    new Thread(() -> {

                        boolean authed = nativeAuthenticateADB(mResolvedAdbPort, code);

                        runOnUiThread(() -> {

                            if (authed) {

                                Toast.makeText(this, "Handshake Perfect! Secured UID 2000 context.", Toast.LENGTH_LONG).show();

                            } else {

                                Toast.makeText(this, "Authentication Failed. Check logs.", Toast.LENGTH_LONG).show();

                            }

                        });

                    }).start();

                })

                .setNegativeButton("Cancel", null)

                .show();

    }



    @Override

    protected void onDestroy() {

        if (mNsdManager != null && mDiscoveryListener != null) {

            try {

                mNsdManager.stopServiceDiscovery(mDiscoveryListener);

            } catch (Exception ignored) {}

        }

        super.onDestroy();

    }

}