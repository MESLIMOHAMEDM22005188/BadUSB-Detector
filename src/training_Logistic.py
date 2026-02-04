import pandas as pd
import numpy as np
from sklearn.linear_model import LogisticRegression, LinearRegression
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score, confusion_matrix
import m2cgen as m2c  # pip install m2cgen

# 1. LOAD YOUR SPECIFIC DATA
# ---------------------------------------------------------
print("[*] Loading Data...")
human_df = pd.read_csv('../data/human_data.csv')
badusb_df = pd.read_csv('../data/badusb_data.csv')

# 2. FEATURE ENGINEERING (Windowing)
# ---------------------------------------------------------
def create_features(df, window_size=5):
    features = []
    labels = []

    # We loop through the data, looking at 5 keys at a time
    for i in range(len(df) - window_size):
        window = df.iloc[i : i + window_size]

        # Calculate Statistics for this window
        avg_delay = window['inter_char_delay_ms'].mean()
        std_delay = window['inter_char_delay_ms'].std() # Jitter
        min_delay = window['inter_char_delay_ms'].min()
        max_delay = window['inter_char_delay_ms'].max()
        avg_hold  = window['key_hold_time_ms'].mean()

        # Feature Vector: [AvgDelay, Jitter, MinDelay, MaxDelay, AvgHold]
        features.append([avg_delay, std_delay, min_delay, max_delay, avg_hold])
        labels.append(window['label'].iloc[0])

    return features, labels

print("[*] Processing Windows...")
X_human, y_human = create_features(human_df)
X_bad, y_bad = create_features(badusb_df)

# Combine them
X = np.array(X_human + X_bad)
y = np.array(y_human + y_bad)

# 3. TRAIN MODEL
# ---------------------------------------------------------
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

model = LinearRegression()
model.fit(X_train, y_train)

# 4. EVALUATE
# ---------------------------------------------------------
preds = model.predict(X_test)
acc = accuracy_score(y_test, preds)

print(f"\n[RESULT] Model Accuracy: {acc*100:.2f}%")
print("Confusion Matrix:")
print(confusion_matrix(y_test, preds))

# 5. EXPORT TO C++
# ---------------------------------------------------------
# This generates the "score()" function automatically
code = m2c.export_to_c(model)

with open("../include/LogisticModel.h", "w") as f:
    f.write("// ACCURACY: " + str(acc) + "\n")
    f.write("// FEATURES: [AvgDelay, Jitter, MinDelay, MaxDelay, AvgHold]\n\n")
    f.write(code)

print("\n[SUCCESS] C++ Code saved to ../include/LogisticModel.h")