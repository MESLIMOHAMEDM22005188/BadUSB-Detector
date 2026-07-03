// Advanced Adversarial Keystroke Injector (SAFE)
// Injects physical keystrokes with AI-evading timing intervals

#include <Arduino.h>
#include <Keyboard.h> // IMPORTANT: The library that makes it a real keyboard!

enum Mode {
  FAST_BOT,
  JITTER_BOT,
  HUMAN_GAUSSIAN,
  LOW_AND_SLOW,
  BURSTY,
  MARKOV_MIXED
};

// Change this to test how your C++ AI reacts to different attacks!
Mode mode = MARKOV_MIXED;

// ---------- Config ----------
const unsigned long BASE_MEAN = 85;   // ms
const unsigned long BASE_STD  = 25;   // ms
const unsigned long MIN_DELAY = 5;
const unsigned long MAX_DELAY = 600;

// Simulated typing payload (Harmless audit string)
const char payload[] = "whoami && echo 'Security Audit' > audit.txt\n";
size_t payloadIndex = 0;
size_t payloadLength = 0;

// Markov states for mixed attacker
enum State { HUMAN, BOT_BURST, PAUSE };
State state = HUMAN;

// Transition probabilities (percent)
const int P_HUMAN_TO_BURST = 10;
const int P_BURST_TO_PAUSE = 30;
const int P_PAUSE_TO_HUMAN = 70;

// ---------- Utilities ----------

// Box-Muller transform for Gaussian sampling
float randGaussian(float mean, float stddev) {
  float u1 = (random(1, 10000) / 10000.0);
  float u2 = (random(1, 10000) / 10000.0);
  float z0 = sqrt(-2.0 * log(u1)) * cos(2 * PI * u2);
  return z0 * stddev + mean;
}

unsigned long clampDelay(float val) {
  if (val < MIN_DELAY) return MIN_DELAY;
  if (val > MAX_DELAY) return MAX_DELAY;
  return (unsigned long)val;
}

// ---------- Delay Generators ----------

unsigned long fastBot() {
  return 5; // unrealistically fast constant
}

unsigned long jitterBot() {
  return random(5, 150); // wide uniform jitter
}

unsigned long humanGaussian() {
  float d = randGaussian(BASE_MEAN, BASE_STD);
  return clampDelay(d);
}

unsigned long lowAndSlow() {
  return random(180, 450); // stealthy slow typing
}

unsigned long bursty() {
  if (random(0, 100) < 75) {
    return random(40, 120); // normal-ish
  } else {
    return random(5, 20);   // burst injection
  }
}

// Markov chain: switches behavior dynamically
unsigned long markovMixed() {
  int r = random(0, 100);

  switch (state) {
    case HUMAN:
      if (r < P_HUMAN_TO_BURST) state = BOT_BURST;
      return humanGaussian();

    case BOT_BURST:
      if (r < P_BURST_TO_PAUSE) state = PAUSE;
      return random(5, 25);

    case PAUSE:
      if (r < P_PAUSE_TO_HUMAN) state = HUMAN;
      return random(200, 500);
  }

  return BASE_MEAN;
}

// ---------- Main ----------

unsigned long generateDelay() {
  switch (mode) {
    case FAST_BOT:        return fastBot();
    case JITTER_BOT:      return jitterBot();
    case HUMAN_GAUSSIAN:  return humanGaussian();
    case LOW_AND_SLOW:    return lowAndSlow();
    case BURSTY:          return bursty();
    case MARKOV_MIXED:    return markovMixed();
  }
  return BASE_MEAN;
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0)); // non-deterministic seed
  payloadLength = strlen(payload);
  
  // Initialize the USB Keyboard connection
  Keyboard.begin();
  
  // SAFETY DELAY: Wait 5 seconds before attacking!
  // This gives you time to open a text editor or terminal 
  // so it doesn't accidentally execute commands in your IDE.
  Serial.println("Waiting 5 seconds before injection...");
  delay(5000);
}

void loop() {
  unsigned long delayMs = generateDelay();
  char c = payload[payloadIndex];

  // 1. THIS IS THE MAGIC: Actually inject the keystroke into the computer!
  Keyboard.write(c);

  // 2. Still print to Serial so you can monitor the math in the background
  Serial.print("Injected: [");
  if (c == '\n') {
    Serial.print("\\n"); 
  } else {
    Serial.print(c);
  }
  Serial.print("] -> Wait: ");
  Serial.print(delayMs);
  Serial.println(" ms");

  // 3. Pause for the calculated time to spoof the AI
  delay(delayMs);

  // Move to the next character in the payload
  payloadIndex++;
  
  // Reset when the payload is finished
  if (payloadIndex >= payloadLength) {
    Serial.println("--- Payload Complete. Restarting in 5 seconds... ---");
    Keyboard.releaseAll(); // Safety measure to release any stuck keys
    delay(5000);
    payloadIndex = 0;
  }
}