# ESP32 PUF (Physical Unclonable Function) Generation

This project implements a Physical Unclonable Function (PUF) on the ESP32 microcontroller using SRAM power-up states as a hardware fingerprint. The generated PUF key is unique to each device and can be used for device authentication and cryptographic key generation.

## Overview

The PUF leverages the random initial state of uninitialized SRAM memory upon power-on to generate a unique, device-specific cryptographic key. Through multiple power cycles and statistical analysis, stable bits are identified and hashed to create a reproducible 256-bit key.

## Software Requirements

- **Arduino IDE** (version 1.8.x or 2.x)
- **ESP32 Board Support** (install via Arduino Board Manager)
- **CoolTerm** - Serial terminal application for monitoring output
  - Download from: https://freeware.the-meiers.org/

## Hardware Requirements

- ESP32 Development Board (e.g., ESP32-WROOM, ESP32-DevKitC)
- USB Cable for programming and power cycling

## Configuration Parameters

```cpp
#define PUF_SIZE 256              // PUF size in bytes (256 bytes = 2048 bits)
#define ENROLL_ROUNDS 7           // Number of power cycles for enrollment
#define STABILITY_THRESHOLD 0.85  // Minimum stability ratio (85%)
```

## How It Works

### 1. **Memory Allocation**
Allocates a 256-byte block from ESP32's internal SRAM to capture power-on states.

### 2. **Enrollment Phase (7 Power Cycles)**
- On each physical power-on, the code reads the uninitialized RAM state
- Each bit position is tracked across all 7 power cycles
- Bits that consistently appear as '1' or '0' are marked as stable
- Requires **physical power cycling** (unplugging USB) for valid samples

### 3. **Key Generation**
- After 7 successful enrollments, stable bits are identified
- Only bits with ≥85% stability threshold are used
- The stable bit pattern is hashed using SHA-256
- Final output: 256-bit cryptographic key unique to the device

## Installation & Usage

### Step 1: Setup Arduino IDE
1. Install ESP32 board support:
   - Open Arduino IDE → File → Preferences
   - Add to "Additional Board Manager URLs":
     ```
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to Tools → Board → Board Manager
   - Search "ESP32" and install "esp32 by Espressif Systems"

2. Select your board:
   - Tools → Board → ESP32 Arduino → Select your ESP32 board model

### Step 2: Upload Code
1. Open `PUF/pufDeri.ino` in Arduino IDE
2. Connect your ESP32 via USB
3. Select the correct COM port: Tools → Port
4. Click Upload (or press Ctrl+U)

### Step 3: Enrollment Process (using CoolTerm)
1. Open CoolTerm and connect to the ESP32's serial port
   - Baud Rate: 115200
   - Data Bits: 8, Parity: None, Stop Bits: 1

2. **Perform 7 Physical Power Cycles:**
   - Each time you see: `>> ACTION: Unplug and Replug now.`
   - **Physically disconnect the USB cable**
   - Wait 2-3 seconds
   - **Reconnect the USB cable**
   - Observe the training step increment (1/7, 2/7, ... 7/7)

3. After the 7th power cycle, the final PUF key will be displayed:
   ```
   Training Complete! calculating stable bits...
   Stability: XXXX / 2048 bits used.
   ---------------------------------------------
   FINAL PUF KEY: [64 hex characters]
   ---------------------------------------------
   ```

## Important Notes

⚠️ **Power Cycling Requirements:**
- Only **physical power cycles** (USB unplug/replug) are valid for enrollment
- Soft resets (reset button, software reset) are automatically ignored
- The code detects reset type and will reject soft resets

⚠️ **Storage Persistence:**
- Enrollment data is stored in ESP32's NVS (Non-Volatile Storage)
- The enrollment state persists across power cycles
- To restart enrollment, uncomment the reset code in `loop()` function

## Resetting the Enrollment

To clear stored data and restart enrollment:

1. Uncomment this line in the `loop()` function:
   ```cpp
   prefs.begin("puf_optimized"); prefs.clear(); Serial.println("WIPED"); while(1);
   ```
2. Upload the modified code
3. Let it run once (you'll see "WIPED" in serial output)
4. Comment out the line again and re-upload
5. Begin a new enrollment cycle

## Technical Details

- **PUF Type:** SRAM-based
- **Key Length:** 256 bits (32 bytes)
- **Hash Algorithm:** SHA-256 (mbedTLS library)
- **Storage:** ESP32 NVS (Non-Volatile Storage)
- **Memory Allocation:** Internal 8-bit SRAM

## Applications

- Device authentication
- Cryptographic key generation
- Hardware fingerprinting
- IoT security
- License binding

## Troubleshooting

**Problem:** "Storage Error: Accumulator size mismatch"
- **Solution:** The storage namespace was corrupted. The code will auto-reset.

**Problem:** "CRITICAL ERROR: Failed to save to storage!"
- **Solution:** NVS partition may be full. Use the reset procedure above.

**Problem:** Enrollment stuck or not progressing
- **Solution:** Ensure you're physically unplugging USB, not just pressing reset button.

## License

This project is for educational and research purposes.


---

*Generated PUF keys are device-specific and should be kept secure. Do not share your device's PUF key.*
