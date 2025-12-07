#include <Arduino.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <mbedtls/sha256.h>

// --- OPTIMIZED CONFIGURATION ---
// Reduced to 256 to prevent NVS storage overflow
#define PUF_SIZE 256              
#define ENROLL_ROUNDS 7           
#define STABILITY_THRESHOLD 0.85  

Preferences prefs;
byte *puf_ram_block;

// --- SHA-256 Helper ---
void computeSHA256(const uint8_t *data, size_t len, uint8_t *out) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); 
    mbedtls_sha256_update(&ctx, data, len);
    mbedtls_sha256_finish(&ctx, out);
    mbedtls_sha256_free(&ctx);
}

void printHash(uint8_t *hash) {
    for (int i = 0; i < 32; i++) {
        if (hash[i] < 0x10) Serial.print("0");
        Serial.print(hash[i], HEX);
    }
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(2000); // Wait for monitor
    Serial.println("\n=== ESP32 OPTIMIZED PUF ENROLLMENT ===");

    // 1. ALLOCATE MEMORY (Consistent Address)
    puf_ram_block = (byte *) heap_caps_malloc(PUF_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (puf_ram_block == NULL) {
        Serial.println("Error: Memory allocation failed.");
        while(1);
    }

    // 2. OPEN STORAGE
    prefs.begin("puf_optimized", false); // New namespace to avoid old garbage

    // 3. AUTO-RESET CHECK
    // If this is the first run of this new code, we reset the counter.
    if (!prefs.isKey("init_done")) {
        Serial.println(">> First run detected. initializing storage...");
        prefs.clear();
        prefs.putBool("init_done", true);
        prefs.putInt("boot_count", 0);
    }

    int boot_count = prefs.getInt("boot_count", 0);
    esp_reset_reason_t reason = esp_reset_reason();

    // 4. CHECK REBOOT TYPE
    if (reason != ESP_RST_POWERON && boot_count < ENROLL_ROUNDS) {
        Serial.println(">> IGNORED: Soft Reset detected."); 
        Serial.println(">> ACTION: You must physically UNPLUG the USB.");
        return;
    }

    // --- ENROLLMENT LOGIC ---
    if (boot_count < ENROLL_ROUNDS) {
        Serial.printf("Training Step: %d / %d\n", boot_count + 1, ENROLL_ROUNDS);
        
        // Allocate small accumulator (2KB)
        size_t acc_size = PUF_SIZE * 8 * sizeof(uint8_t);
        uint8_t *accumulator = (uint8_t *)malloc(acc_size);
        
        if (accumulator == NULL) { Serial.println("Malloc failed"); return; }

        // Load or Clear Accumulator
        if (boot_count > 0) {
            // Check if we can read the file
            if (prefs.getBytesLength("acc") != acc_size) {
                 Serial.println("Storage Error: Accumulator size mismatch. Resetting...");
                 prefs.putInt("boot_count", 0);
                 free(accumulator);
                 return;
            }
            prefs.getBytes("acc", accumulator, acc_size);
        } else {
            memset(accumulator, 0, acc_size);
        }

        // Scan RAM and update counts
        for (int i = 0; i < PUF_SIZE; i++) {
            byte val = puf_ram_block[i];
            for (int bit = 0; bit < 8; bit++) {
                if ((val >> bit) & 1) {
                    accumulator[(i*8) + bit]++;
                }
            }
        }

        // SAVE TO NVS (With error checking)
        size_t written = prefs.putBytes("acc", accumulator, acc_size);
        if (written != acc_size) {
            Serial.println("CRITICAL ERROR: Failed to save to storage! (NVS Full?)");
        } else {
            prefs.putInt("boot_count", boot_count + 1);
            Serial.println(">> SNAPSHOT SAVED SUCCESSFULLY.");
            Serial.println(">> ACTION: Unplug and Replug now.");
        }
        
        free(accumulator);

    } else {
        // --- KEY GENERATION ---
        Serial.println("Training Complete! calculating stable bits...");

        size_t acc_size = PUF_SIZE * 8 * sizeof(uint8_t);
        uint8_t *accumulator = (uint8_t *)malloc(acc_size);
        prefs.getBytes("acc", accumulator, acc_size);

        uint8_t stable_key_data[PUF_SIZE];
        memset(stable_key_data, 0, PUF_SIZE);
        int stable_bits = 0;

        for (int i = 0; i < (PUF_SIZE * 8); i++) {
            float prob = (float)accumulator[i] / (float)ENROLL_ROUNDS;
            
            if (prob >= STABILITY_THRESHOLD) {
                stable_key_data[i/8] |= (1 << (i%8));
                stable_bits++;
            } else if (prob <= (1.0 - STABILITY_THRESHOLD)) {
                stable_bits++;
            }
        }
        free(accumulator);

        Serial.printf("Stability: %d / %d bits used.\n", stable_bits, PUF_SIZE*8);
        
        uint8_t final_hash[32];
        computeSHA256(stable_key_data, PUF_SIZE, final_hash);

        Serial.println("---------------------------------------------");
        Serial.print("FINAL PUF KEY: ");
        printHash(final_hash);
        Serial.println("---------------------------------------------");
    }

    free(puf_ram_block);
    prefs.end(); // Close storage safely
}

void loop() {
  // To reset training, uncomment the line below, upload, and run once.
  // prefs.begin("puf_store"); prefs.clear(); Serial.println("WIPED"); while(1);
}