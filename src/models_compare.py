import pandas as pd
import numpy as np
import os
import matplotlib.pyplot as plt
import seaborn as sns
import joblib
import time

# antreneaza datele din cele doua fisiere de antrenament
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import accuracy_score, f1_score, roc_auc_score, roc_curve, confusion_matrix

# Import the ML Algorithms
from sklearn.linear_model import LogisticRegression
from sklearn.svm import SVC
from sklearn.ensemble import RandomForestClassifier
from xgboost import XGBClassifier
from sklearn.neural_network import MLPClassifier

# 1. SETUP DIRECTORY FOR ML GRAPHS
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
output_dir = os.path.join(SCRIPT_DIR, "../images/ml_results_data")
if not os.path.exists(output_dir):
    os.makedirs(output_dir)
    print(f"Created directory: {output_dir}")

# 2. LOAD & CLEAN DATA
# Corectat: data în loc de data_old conform structurii din VS Code
badusb_path = os.path.join(SCRIPT_DIR, "../data/training_data/badusb_data.csv")
human_path = os.path.join(SCRIPT_DIR, "../data/training_data/human_data.csv")

try:
    badusb = pd.read_csv(badusb_path, on_bad_lines='skip')
    human = pd.read_csv(human_path, on_bad_lines='skip')
    print("✓ Datele CSV au fost încărcate cu succes!")
except FileNotFoundError:
    print(f"❌ EROARE CRITICĂ: Nu găsesc fișierele de date.")
    print(f"Caut BadUSB în: {os.path.abspath(badusb_path)}")
    print(f"Caut Uman în: {os.path.abspath(human_path)}")
    print("Verifică structura folderelor (asigură-te că folderul se numește 'data' și nu 'data_old').")
    exit(1)

# Fix data leakage
badusb = badusb.rename(columns=lambda x: x.replace(';', '').strip())
human = human.rename(columns=lambda x: x.replace(';', '').strip())

if 'label' in badusb.columns: badusb = badusb.drop(columns=['label'])
if 'label' in human.columns: human = human.drop(columns=['label'])

badusb["label"] = 1
human["label"] = 0
data = pd.concat([badusb, human], ignore_index=True)

# Keep only numbers and drop NaNs
X = data.drop(columns=["label"]).select_dtypes(include=[np.number])
data_clean = pd.concat([X, data["label"]], axis=1).dropna()
X = data_clean.drop(columns=["label"])
y = data_clean["label"]

# 3. SPLIT AND SCALE THE DATA
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)

# Scaling is CRITICAL for Neural Networks to converge properly!
scaler = StandardScaler()
X_train_scaled = scaler.fit_transform(X_train)
X_test_scaled = scaler.transform(X_test)

# 4. DEFINE THE 5 MODELS (KNN Removed)
models = {
    "Logistic Reg": LogisticRegression(random_state=42),
    "SVM (RBF)": SVC(probability=True, random_state=42),
    "Random Forest": RandomForestClassifier(n_estimators=100, random_state=42),
    "XGBoost": XGBClassifier(eval_metric='logloss', random_state=42),
    "Neural Network": MLPClassifier(hidden_layer_sizes=(64, 32), max_iter=500, random_state=42)
}

# 5. TRAIN AND EVALUATE
results = []
roc_data = {}
cm_data = {}

print("\nAntrenare Modele & Generare Predicții... (Rețeaua Neuronală poate dura câteva secunde)")

for name, model in models.items():
    # Masuram timpul de antrenament
    start_time = time.time()
    model.fit(X_train_scaled, y_train)
    train_time = time.time() - start_time

    y_pred = model.predict(X_test_scaled)
    y_prob = model.predict_proba(X_test_scaled)[:, 1]

    # Calculate Metrics
    acc = accuracy_score(y_test, y_pred)
    f1 = f1_score(y_test, y_pred)
    auc = roc_auc_score(y_test, y_prob)

    # Calculate ROC Curve points
    fpr, tpr, _ = roc_curve(y_test, y_prob)

    # Store data for plotting
    results.append({"Model": name, "Accuracy (%)": round(acc * 100, 2), "F1-Score": round(f1, 4), "ROC-AUC": round(auc, 4)})
    roc_data[name] = {"fpr": fpr, "tpr": tpr, "auc": auc}
    cm_data[name] = confusion_matrix(y_test, y_pred)

    print(f"[{name}] Antrenat. Acuratețe: {acc*100:.2f}%")

results_df = pd.DataFrame(results)

# Print Leaderboard to Terminal
print("\n" + "="*40)
print("🏆 FINAL ML LEADERBOARD 🏆")
print("="*40)
print(results_df.sort_values(by="Accuracy (%)", ascending=False).to_string(index=False))
print("="*40 + "\n")

# =========================================================
# PART 6: EXPORT FINAL MODEL FOR C++ INFERENCE
# =========================================================
# We retrain the winning XGBoost model on the ENTIRE dataset
# (train + test) to give it maximum knowledge before export.

print("Re-antrenare Model (XGBoost) pe întregul set de date pentru export...")
final_model = XGBClassifier(eval_metric='logloss', random_state=42)

# Pentru XGBoost nu e obligatorie scalarea, dar daca antrenezi pe X, vei face predictii in C++ direct pe valorile brute
final_model.fit(X, y)

export_path = os.path.join(SCRIPT_DIR, "badusb_xgboost.json")
final_model.save_model(export_path)

print(f"✓ Export Complet! Modelul gata pentru C++ a fost salvat în: {export_path}")
print("\n[SYSTEM] Export modele shadow pentru viitor...")

# Create a dedicated folder for the exported models (relativ la SCRIPT_DIR)
models_dir = os.path.join(SCRIPT_DIR, 'shadow_models')
if not os.path.exists(models_dir):
    os.makedirs(models_dir)
    print(f"[INFO] Am creat directorul: {models_dir}/")

# 1. Save the Scaler (This is CRITICAL for the Neural Network and SVM)
scaler_path = os.path.join(models_dir, 'shadow_scaler.pkl')
joblib.dump(scaler, scaler_path)
print(" -> Salvat: shadow_scaler.pkl")

# 2. Save each model individually
for name, model in models.items():
    # Clean the name for a filename (e.g., "SVM (RBF)" -> "shadow_svm_rbf.pkl")
    clean_name = name.lower().replace(" ", "_").replace("(", "").replace(")", "").replace("=", "")
    filename = f"shadow_{clean_name}.pkl"
    file_path = os.path.join(models_dir, filename)

    joblib.dump(model, file_path)
    print(f" -> Salvat: {filename}")

print(f"\n[SUCCES] Toate modelele au fost exportate ca fișiere .pkl în folderul '{models_dir}'.")