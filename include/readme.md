# BadUSB-Detector (Fork)

This repository is a **fork** of the original [BadUSB-Detector](#) project, a tool that detects BadUSB attacks by analyzing keystroke dynamics (typing behavior) combined with a consensus of multiple Machine Learning models (XGBoost, Random Forest, SVM, Neural Network, Logistic Regression).

> **Platform note**: this project relies on low-level APIs that are exclusive to **macOS** (`CGEventTap`, `ApplicationServices`, `Carbon`, `ioreg`, `osascript`, `diskutil`). It does not compile natively on Windows or Linux as-is.

## Fork context

The original project worked, but had several fragility points identified during a code review: duplicated logic across the data collection modules, a timing bug on fast/overlapping keystrokes, and unoptimized disk I/O. This fork fixes these issues without changing the overall architecture or the ML detection logic.

## Improvements made

### 1. Unified data collection modules

**Before:** two nearly identical, duplicated files (`CollectingDataBadUSB.cpp` and `CollectingDataHuman.cpp`), each with its own keystroke timing logic.

**After:** a single module `KeystrokeCollector.h` / `KeystrokeCollector.cpp`, parameterized via an `enum CollectionLabel { Human, BadUSB }`.

**Why this matters:**
- **Bug fixed**: the original `CollectingDataBadUSB.cpp` used a single `static TimePoint t_current_press_start` variable to measure key hold duration. In case of overlapping keystrokes (a key pressed down before the previous one is released — a common scenario during a very fast BadUSB injection), this global variable would get overwritten and produce incorrect `hold_time` measurements. These faulty measurements ended up directly in the ML training dataset, potentially degrading the model's accuracy on the exact use case it's supposed to detect.
  The new module uses a `map<CGKeyCode, PendingKeyData>` (one entry per physically pressed key), reusing the more robust approach that already existed on the "Human" collection side, and applies it consistently to both collection modes.
- **Duplication removed**: a single implementation to maintain, one place to fix future bugs.

### 2. Optimized disk flushing

**Before:** `csvFile.flush()` called on every recorded keystroke — an expensive system call at high frequency (a BadUSB attack can inject several hundred characters per second).

**After:** batched flush every 20 lines (`FLUSH_EVERY_N_LINES`), drastically reducing the number of blocking I/O calls during capture, at the cost of a limited risk of losing a few lines in case of an abrupt process crash (acceptable for training data collection).

### 3. Argument-driven `main.cpp`

**Before:** to run a data collection session (human or BadUSB) instead of detection mode, `main.cpp` had to be manually edited (comment out `ejectBadUSB()`, call the collection function), then reverted before committing — a source of mistakes and forgotten reverts.

**After:**
```bash
./design                    # detection mode (default behavior, unchanged)
./design --collect-human    # collect human typing data
./design --collect-badusb   # collect BadUSB typing data
./design --help             # help
```

### 4. Cleaned up `CMakeLists.txt`

- Removed duplicate entries (`GeminiAnalyzer.h`/`.cpp` were listed twice)
- Updated file paths following the unification of the collection modules

## What hasn't changed

- The real-time detection logic (`DisconnectKeyboard.cpp`, `BadUSBDetector.cpp`) remains identical to the original
- The ML training pipeline (`models_compare.py`, `export_to_cpp.py`) remains identical
- The Gemini AI integration for threat reports (`GeminiAnalyzer.cpp`) remains identical
- The generated CSV file format is unchanged (fully compatible with the existing training pipeline)

## Known limitations (inherited from the original, not fixed in this fork)

These points were identified during the code review but are not yet fixed in this fork. They are documented here for future reference:

- `getUsbDeviceCount()` relies on `popen("ioreg -p IOUSB | grep -c \"class\"")`, an unreliable proxy for counting connected USB devices, with an unprotected `stoi()` call that can throw on unexpected output.
- Shared state between the main thread (event tap) and the USB monitoring thread (`window_buffer`, `key_press_starts`, `pre_catch_payload`) is not protected by a mutex.
- No external configuration file: detection thresholds (`WINDOW_SIZE`, `THRESHOLD`, `SILENCE_TIMEOUT_SECONDS`) are hardcoded in the source.

## Requirements (unchanged)

- macOS with Accessibility permissions enabled for the executable (System Settings → Privacy & Security → Accessibility)
- Must run with `sudo` for low-level keyboard capture
- Dependencies: `libcurl`, `nlohmann-json`, `xgboost`, `libpcap`

## Security

An `api_key.txt` file containing a Google Gemini API key must be placed at the project root. **This file must never be committed** — make sure it's listed in `.gitignore`.