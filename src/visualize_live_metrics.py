import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
import joblib
import os
import plotly.express as px
import webbrowser
from datetime import datetime

# genereaza grafice din setul de date din training_data (it acts independently)

print("==================================================")
print("   GENERATING LIVE ATTACK TELEMETRY GRAPHS        ")
print("==================================================")

# 1. Setup Directories
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.join(SCRIPT_DIR, "..")
MODELS_DIR = os.path.join(ROOT_DIR, "shadow_models")

# =========================================================
# NEW: Setup Dual Output Directories (Project + Desktop)
# =========================================================
current_time = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
folder_name = f"attack_{current_time}"

# Location 1: Inside your CLion Project (images/live_attack_metrics)
LOCAL_BASE_DIR = os.path.join(ROOT_DIR, "images", "live_attack_metrics")
LOCAL_OUT_DIR = os.path.join(LOCAL_BASE_DIR, folder_name)

# Location 2: Directly on your Mac Desktop!
DESKTOP_BASE_DIR = os.path.join(os.path.expanduser("~/Desktop"), "live_attack_metrics")
DESKTOP_OUT_DIR = os.path.join(DESKTOP_BASE_DIR, folder_name)

# Create both folders
for directory in [LOCAL_OUT_DIR, DESKTOP_OUT_DIR]:
    if not os.path.exists(directory):
        os.makedirs(directory)

print(f"[INFO] Graphs will be saved to Project: {LOCAL_OUT_DIR}")
print(f"[INFO] Graphs will be mirrored to Desktop: {DESKTOP_OUT_DIR}")

# 2. Load the Live Telemetry Data
telemetry_file = os.path.join(ROOT_DIR, "catch_telemetry.csv")
if not os.path.exists(telemetry_file):
    telemetry_file = os.path.join(ROOT_DIR, "post_catch_telemetry.csv")  # Fallback
    if not os.path.exists(telemetry_file):
        print("[ERROR] No telemetry CSV found. Run the C++ attack first!")
        exit()

df = pd.read_csv(telemetry_file).dropna()
print(f"[INFO] Loaded {len(df)} live keystrokes from {os.path.basename(telemetry_file)}")

# Pre-process for models (Fixing the column names for the Scaler!)
X_live = df[['flight', 'inter', 'hold']].copy()
X_live.columns = ['flight_time_ms', 'inter_char_delay_ms', 'key_hold_time_ms']

# 3. Load Scaler and Models to calculate Live Accuracy
scaler = joblib.load(os.path.join(MODELS_DIR, "shadow_scaler.pkl"))
X_live_scaled = scaler.transform(X_live.values)

models = {
    "Logistic Regression": "shadow_logistic_reg.pkl",
    "SVM (RBF)": "shadow_svm_rbf.pkl",
    "Random Forest": "shadow_random_forest.pkl",
    "Neural Network": "shadow_neural_network.pkl"
}

results = []
for name, filename in models.items():
    filepath = os.path.join(MODELS_DIR, filename)
    if os.path.exists(filepath):
        model = joblib.load(filepath)
        preds = model.predict(X_live_scaled)
        caught = sum(preds == 1)
        rate = (caught / len(preds)) * 100
        results.append({"Model": name, "Detection Rate (%)": rate})

if 'xgboost_prob' in df.columns:
    xgb_caught = sum(df['xgboost_prob'] > 0.75)
    xgb_rate = (xgb_caught / len(df)) * 100
    results.append({"Model": "XGBoost (Active C++)", "Detection Rate (%)": xgb_rate})

results_df = pd.DataFrame(results).sort_values(by="Detection Rate (%)", ascending=False)

# =========================================================
# GRAPH 1: Live Model Detection Bar Chart
# =========================================================
print(" -> Generating Graph 1: Live Model Consensus...")
plt.figure(figsize=(12, 6))
sns.barplot(data=results_df, x="Detection Rate (%)", y="Model", palette="magma")
plt.title("Live Attack Evaluation: Multi-Model Detection Rate", fontsize=16, pad=15)
plt.xlabel("Detection Rate (%)", fontsize=12)
plt.ylabel("AI Engine", fontsize=12)
plt.xlim(0, 105)

for index, value in enumerate(results_df["Detection Rate (%)"]):
    plt.text(value + 1, index, f"{value:.1f}%", va='center', fontsize=12, fontweight='bold')

plt.tight_layout()
# Save to both locations
plt.savefig(os.path.join(LOCAL_OUT_DIR, "1_live_detection_rates.png"), dpi=300)
plt.savefig(os.path.join(DESKTOP_OUT_DIR, "1_live_detection_rates.png"), dpi=300)
plt.close()

# =========================================================
# GRAPH 2: XGBoost Threat Confidence Timeline
# =========================================================
if 'xgboost_prob' in df.columns:
    print(" -> Generating Graph 2: Threat Confidence Timeline...")
    plt.figure(figsize=(12, 5))
    plt.plot(df.index, df['xgboost_prob'] * 100, color='red', linewidth=2, label="XGBoost Confidence")
    plt.axhline(y=75, color='black', linestyle='--', alpha=0.7, label="Lockdown Threshold (75%)")
    plt.fill_between(df.index, df['xgboost_prob'] * 100, 75, where=(df['xgboost_prob'] * 100 > 75),
                     interpolate=True, color='red', alpha=0.3)

    plt.title("Real-Time Threat Confidence During BadUSB Attack", fontsize=16, pad=15)
    plt.xlabel("Keystroke Number", fontsize=12)
    plt.ylabel("Malicious Probability (%)", fontsize=12)
    plt.ylim(0, 105)
    plt.legend(loc="lower right")
    plt.grid(alpha=0.3)

    plt.tight_layout()
    # Save to both locations
    plt.savefig(os.path.join(LOCAL_OUT_DIR, "2_threat_timeline.png"), dpi=300)
    plt.savefig(os.path.join(DESKTOP_OUT_DIR, "2_threat_timeline.png"), dpi=300)
    plt.close()

# =========================================================
# GRAPH 3: INTERACTIVE 3D Keystroke Dynamics
# =========================================================
print(" -> Generating Graph 3: Interactive 3D Keystroke Dynamics...")

fig = px.scatter_3d(
    df, x='flight', y='inter', z='hold',
    color='xgboost_prob' if 'xgboost_prob' in df.columns else None,
    color_continuous_scale='bluered',
    title='Interactive 3D Keystroke Dynamics (Click and Drag to Rotate)',
    labels={'flight': 'Flight Time (ms)', 'inter': 'Inter-Key Time (ms)', 'hold': 'Hold Time (ms)',
            'xgboost_prob': 'XGBoost Threat Score'},
    opacity=0.8
)

fig.update_traces(marker=dict(size=5, line=dict(width=1, color='DarkSlateGrey')))
fig.update_layout(margin=dict(l=0, r=0, b=0, t=40))

# Save HTML to both locations
local_html = os.path.join(LOCAL_OUT_DIR, "3_keystroke_dynamics_3d.html")
desktop_html = os.path.join(DESKTOP_OUT_DIR, "3_keystroke_dynamics_3d.html")

fig.write_html(local_html)
fig.write_html(desktop_html)

print(f"\n[SUCCESS] Graphs successfully saved and mirrored to Desktop!")
print("==================================================")

# Open the Desktop version in the browser
print("[INFO] Opening interactive 3D graph in your browser...")
webbrowser.open('file://' + os.path.abspath(desktop_html))