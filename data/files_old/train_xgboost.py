import pandas as pd
import numpy as np
import os

from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, confusion_matrix, roc_auc_score

from xgboost import XGBClassifier
import matplotlib.pyplot as plt

# 1. Load data
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
badusb_path = os.path.join(SCRIPT_DIR, "../data/training_data/badusb_data.csv")
human_path  = os.path.join(SCRIPT_DIR, "../data/training_data/human_data.csv")

badusb = pd.read_csv(badusb_path, on_bad_lines='skip')
human  = pd.read_csv(human_path, on_bad_lines='skip')

badusb = badusb.rename(columns=lambda x: x.replace(';', '').strip())
human = human.rename(columns=lambda x: x.replace(';', '').strip())

if 'label' in badusb.columns:
    badusb = badusb.drop(columns=['label'])
if 'label' in human.columns:
    human = human.drop(columns=['label'])
# ----------------------------

# 2. Label data (Fresh, clean labels)
badusb["label"] = 1   # BadUSB
human["label"]  = 0   # Human

data = pd.concat([badusb, human], ignore_index=True)

# 3. Keep only numeric features
X = data.drop(columns=["label"])
X = X.select_dtypes(include=[np.number])
y = data["label"]

# 4. Train / test split
X_train, X_test, y_train, y_test = train_test_split(
    X, y,
    test_size=0.2,
    random_state=42,
    stratify=y
)

# 5. XGBoost model
model = XGBClassifier(
    n_estimators=1000,
    max_depth=2,
    learning_rate=0.15,
    subsample=0.8,
    colsample_bytree=0.8,
    objective="binary:logistic",
    eval_metric="logloss",
    random_state=42
)

model.fit(X_train, y_train)

# 6. Evaluation
y_pred = model.predict(X_test)
y_prob = model.predict_proba(X_test)[:, 1]

# Using 0.5 threshold instead of 0.4 for a more balanced baseline
y_pred = (y_prob > 0.5).astype(int)

print("\nClassification Report:")
print(classification_report(y_test, y_pred, target_names=["Human", "BadUSB"]))

print("Confusion Matrix:")
print(confusion_matrix(y_test, y_pred))

print("ROC-AUC:", roc_auc_score(y_test, y_prob))

# 7. Feature importance
importances = model.feature_importances_
indices = np.argsort(importances)[::-1]

# Uncomment to see the feature importance graph!
# plt.figure(figsize=(8, 5))
# plt.title("XGBoost Feature Importance")
# plt.bar(range(len(importances)), importances[indices])
# plt.xticks(range(len(importances)), X.columns[indices], rotation=15)
# plt.tight_layout()
# plt.show()