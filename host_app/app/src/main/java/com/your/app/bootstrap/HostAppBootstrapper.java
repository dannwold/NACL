package com.your.app.bootstrap;



import android.content.Context;

import android.security.keystore.KeyGenParameterSpec;

import android.security.keystore.KeyProperties;

import java.io.File;

import java.io.FileOutputStream;

import java.io.InputStream;

import java.security.KeyStore;

import javax.crypto.KeyGenerator;

import javax.crypto.SecretKey;



public class HostAppBootstrapper {

    private static final String TAG = "HostAppBootstrapper";

    private static final String KEY_ALIAS = "nacl_ipc_aes_gcm_key";

    private static final String ANDROID_KEYSTORE = "AndroidKeyStore";



    // Returns or generates a hardware-backed 256-bit AES key for IPC encryption

    public static SecretKey getOrCreateHardwareKey() throws Exception {

        KeyStore keyStore = KeyStore.getInstance(ANDROID_KEYSTORE);

        keyStore.load(null);



        if (keyStore.containsAlias(KEY_ALIAS)) {

            KeyStore.SecretKeyEntry entry = (KeyStore.SecretKeyEntry) keyStore.getEntry(KEY_ALIAS, null);

            return entry.getSecretKey();

        }



        // Generate a new key inside the hardware-isolated TEE or StrongBox

        KeyGenerator keyGenerator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, ANDROID_KEYSTORE);

        KeyGenParameterSpec.Builder builder = new KeyGenParameterSpec.Builder(

                KEY_ALIAS,

                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)

                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)

                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)

                .setKeySize(256);



        // Attempt to enforce StrongBox isolation if hardware supports it

        try {

            builder.setIsStrongBoxBacked(true);

        } catch (Exception e) {

            // Fallback to standard TEE execution environment

        }



        keyGenerator.init(builder.build());

        return keyGenerator.generateKey();

    }



    // Dynamic Relocation: Copies packaged dynamic shared libraries (.so) from

    // the application's assets folder to its secure, executable files directory.

    // This systematically bypasses Project Treble namespace blocks [5].

    public static void relocateDynamicLibraries(Context context) throws Exception {

        File secureLibDir = new File(context.getFilesDir(), "lib");

        if (!secureLibDir.exists()) {

            secureLibDir.mkdirs();

        }



        String[] libraries = {

            "libbluetooth_client.so", "libwifi_client.so", "libnfc_subsystem.so",

            "libusb_subsystem.so", "libcamera_subsystem.so", "libsensors_client.so",

            "libtelephony_client.so", "libipc_crypto.so", "libshm_ring_buffer.so"

        };



        byte[] buffer = new byte[1024 * 16];

        for (String lib : libraries) {

            File destFile = new File(secureLibDir, lib);

            // In production, compare checksums first to avoid redundant overwrites

            try (InputStream is = context.getAssets().open("libs/" + lib);

                 FileOutputStream os = new FileOutputStream(destFile)) {



                int read;

                while ((read = is.read(buffer)) != -1) {

                    os.write(buffer, 0, read);

                }

            }

            // Grant executable and secure read-only permissions inside the sandbox [8]

            destFile.setReadable(true, true);

            destFile.setExecutable(true, true);

            destFile.setWritable(false, true);

        }

    }

}