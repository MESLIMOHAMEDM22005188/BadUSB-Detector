import random

# --- YOUR PERSONALIZED HUMAN METRICS (Calculated from human_data.csv) ---
# We use 'Inter-Char Delay' as the target because it captures the total rhythm.
# Target Total: ~285ms (Mean) to ~300ms (Median+Hold)
# We subtract 5ms (Device Hold Time) to get the required Script Delay.

HUMAN_MEAN_DELAY = 280  # Your average rhythm (compensated)
HUMAN_STD_DEV = 305  # Your high variance (You have a "messy" human style)

# Pause logic based on your data's "Long Pauses" (Max was ~4000ms)
WORD_PAUSE_ADD = 150  # Extra time for spacebar
SENTENCE_PAUSE_ADD = 600  # Extra time for thinking


def get_human_delay():
    """Generates a delay based on your specific CSV distribution."""
    # We use a lower bound of 20ms because standard deviation is huge (305ms)
    # This prevents negative numbers while keeping the "long tail" of slow keystrokes.
    delay = random.gauss(HUMAN_MEAN_DELAY, HUMAN_STD_DEV)
    return int(max(20, min(delay, 4000)))


def generate_payload(text_lines):
    print("REM --- PERSONALIZED HUMAN MIMICRY PAYLOAD ---")
    print("REM Based on human_data.csv metrics")
    print("DELAY 2000")
    print("GUI SPACE")
    print("DELAY 500")
    print("STRING textedit")
    print("DELAY 500")
    print("ENTER")
    print("DELAY 1000")
    print("CMD n")
    print("DELAY 1000")

    full_text = " ".join(text_lines)

    for char in full_text:
        # 1. Type the character (Instant Key Press)
        if char == " ":
            print("SPACE")
        elif char == "\n":
            print("ENTER")
        else:
            # Note: This 'STRING' command has the fixed ~5ms hold time
            print(f"STRING {char}")

        # 2. Wait (Randomized Flight Time)
        # This aligns the 'Inter-Char Delay' to match your 300ms average
        delay = get_human_delay()

        # 3. Add Contextual Pauses
        if char == " ":
            delay += WORD_PAUSE_ADD
        elif char in [".", ",", "!", "?"]:
            delay += SENTENCE_PAUSE_ADD

        print(f"DELAY {delay}")


# --- TEXT TO TYPE ---
target_text = [
    "ABSTRACT: THE EVOLUTION OF HID ATTACKS",
    "\n",
    "The proliferation of Human Interface Device (HID) attacks represents a significant shift in physical security paradigms.",
    "Unlike traditional malware which relies on software vulnerabilities, BadUSB devices exploit the inherent trust operating systems place in keyboards.",
    "When a device like this Flipper Zero is connected, it enumerates as a standard input device, bypassing firewalls and antivirus software.",
    "\n",
    "This specific payload is designed to generate training data for a Machine Learning defense system based on Keystroke Dynamics.",
    "The core hypothesis is that while machines are naturally deterministic, humans are stochastic.",
    "A standard script types with zero variance (Standard Deviation = 0), whereas this script introduces pseudorandom delays between every character event.",
    "\n",
    "METHODOLOGY:",
    "\n",
    "Hardware: Flipper Zero mimicking a USB Keyboard.",
    "Software: DuckyScript payload with hardcoded timing jitter.",
    "Detection: A Random Forest Classifier trained to distinguish biological rhythms from algorithmic emulation.",
    "\n",
    "By analyzing the 'Flight Time' (interval between key press and release) and the 'Dwell Time' (duration of press), we can construct a biometric signature.",
    "Even with the randomized delays implemented in this code, specific artifacts remain.",
    "For instance, this code uses a linear distribution for randomization, while genuine human typing follows a normal or log-normal distribution.",
    "A sophisticated AI model should detect this statistical anomaly.",
    "\n",
    "Furthermore, this script lacks the 'burstiness' of human thought.",
    "Humans type entire words quickly (muscle memory) but pause before difficult words.",
    "This Flipper simply randomizes blocks of text independently, creating a 'drunk typist' profile rather than a focused human profile.",
    "This distinction is subtle but mathematically significant for high-precision detection.",
    "\n",
    "CONCLUSION:",
    "\n",
    "This dataset of roughly 1200 characters serves as the 'Advanced Machine' class.",
    "It is harder to detect than a Flipper Zero running a simple script, but still distinct from the user's authentic typing pattern.",
    "End of transmission."
]

generate_payload(target_text)