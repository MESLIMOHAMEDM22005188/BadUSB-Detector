#include <Arduino.h>

enum Mode {
  FAST_BOT,
  JITTER_BOT,
  HUMAN_GAUSSIAN,
  LOW_AND_SLOW,
  BURSTY,
  MARKOV_MIXED
};

Mode mode = MARKOV_MIXED;

// ---------- Config ----------
const unsigned long BASE_MEAN = 85;   // ms
const unsigned long BASE_STD  = 25;   // ms
const unsigned long MIN_DELAY = 5;
const unsigned long MAX_DELAY = 600;

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
}

void loop() {
  unsigned long delayMs = generateDelay();

  // Output one delay per "keystroke"
  Serial.println(delayMs);

  delay(delayMs);
}