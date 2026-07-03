import joblib
import m2cgen as m2c
import os
import sys
import numpy as np

# se exporta din .pkl in ShadowModels.h

# Tell Python to allow massive mathematical equations (for SVM)
sys.setrecursionlimit(10000)

print("==================================================")
print("   BUILDING MASTER C++ CONSENSUS ENGINE HEADER    ")
print("==================================================")

# 1. Dynamically find the directories
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.join(SCRIPT_DIR, "..")
MODELS_DIR = os.path.join(ROOT_DIR, "shadow_models")

# 2. Load the Scaler
scaler_path = os.path.join(MODELS_DIR, "shadow_scaler.pkl")
if not os.path.exists(scaler_path):
    print(f"[ERROR] Scaler not found at {scaler_path}.")
    exit()

scaler = joblib.load(scaler_path)
means = scaler.mean_
scales = scaler.scale_

#z=
# σ
# x−μ
# Start writing the unified C++ Header File
cpp_code = f"""// ==========================================
// AUTO-GENERATED AI MODELS FOR C++ EDR
// ==========================================
#pragma once
#include <cmath>
#include <algorithm>

// --- STANDARD SCALER ---
inline void scale_features(double flight, double inter, double hold, double* scaled_output) {{
    scaled_output[0] = (flight - {means[0]:.6f}) / {scales[0]:.6f};
    scaled_output[1] = (inter - {means[1]:.6f}) / {scales[1]:.6f};
    scaled_output[2] = (hold - {means[2]:.6f}) / {scales[2]:.6f};
}}
"""

# ==========================================
# PART A: M2CGEN MODELS (LogReg, SVM, RF)
# ==========================================
models_to_export = {
    "logistic_reg": "shadow_logistic_reg.pkl",
    "svm": "shadow_svm_rbf.pkl",
    "random_forest": "shadow_random_forest.pkl"
}

for name, filename in models_to_export.items():
    filepath = os.path.join(MODELS_DIR, filename)
    if os.path.exists(filepath):
        print(f"[*] Transpiling {name.upper()}...")
        model = joblib.load(filepath)

        raw_c_code = m2c.export_to_c(model)

        raw_c_code = raw_c_code.replace("double score(double * input)", f"inline double predict_{name}(double* input)")
        raw_c_code = raw_c_code.replace("void score(double * input, double * output)",
                                        f"inline void predict_{name}(double* input, double* output)")

        raw_c_code = raw_c_code.replace("void mul_vector_number(", "inline void mul_vector_number(")
        raw_c_code = raw_c_code.replace("void mul_vector_number(", "inline void mul_vector_number(")
        raw_c_code = raw_c_code.replace("void add_vectors(", "inline void add_vectors(")
        raw_c_code = raw_c_code.replace("void assign_vector(", "inline void assign_vector(")
        raw_c_code = raw_c_code.replace("double dot_product(", "inline double dot_product(")

        cpp_code += f"\n// --- {name.upper()} MODEL ---\n"
        cpp_code += raw_c_code
        cpp_code += "\n"
    else:
        print(f"[WARNING] Could not find {filepath}.")

# ==========================================
# PART B: CUSTOM NEURAL NETWORK
# ==========================================
nn_path = os.path.join(MODELS_DIR, "shadow_neural_network.pkl")
if os.path.exists(nn_path):
    print("[*] Extracting NEURAL NETWORK weights...")
    nn = joblib.load(nn_path)
    w1, w2, w3 = nn.coefs_
    b1, b2, b3 = nn.intercepts_


    def to_cpp_array_1d(arr, name):
        vals = ", ".join([f"{x:.6f}" for x in arr])
        return f"const double {name}[{len(arr)}] = {{{vals}}};\n"


    def to_cpp_array_2d(arr, name):
        inner = ["{" + ", ".join([f"{x:.6f}" for x in row]) + "}" for row in arr]
        return f"const double {name}[{arr.shape[0]}][{arr.shape[1]}] = {{\n    " + ",\n    ".join(inner) + "\n};\n"


    cpp_code += "\n// ==========================================\n"
    cpp_code += "// CUSTOM BARE-METAL NEURAL NETWORK\n"
    cpp_code += "// ==========================================\n\n"

    cpp_code += "namespace NN_Weights {\n"
    cpp_code += to_cpp_array_2d(w1, "W1")
    cpp_code += to_cpp_array_1d(b1, "B1")
    cpp_code += "\n"
    cpp_code += to_cpp_array_2d(w2, "W2")
    cpp_code += to_cpp_array_1d(b2, "B2")
    cpp_code += "\n"
    cpp_code += to_cpp_array_2d(w3, "W3")
    cpp_code += to_cpp_array_1d(b3, "B3")
    cpp_code += "}\n\n"

    cpp_code += """// --- ACTIVATION FUNCTIONS ---
inline double relu(double x) { return std::max(0.0, x); }
inline double sigmoid(double x) { return 1.0 / (1.0 + std::exp(-x)); }

// --- FORWARD PROPAGATION ---
inline double predict_neural_network(double* input) {
    using namespace NN_Weights;
    double h1[64];
    double h2[32];

    // Layer 1
    for(int i = 0; i < 64; i++) {
        h1[i] = B1[i];
        for(int j = 0; j < 3; j++) {
            h1[i] += input[j] * W1[j][i];
        }
        h1[i] = relu(h1[i]);
    }

    // Layer 2
    for(int i = 0; i < 32; i++) {
        h2[i] = B2[i];
        for(int j = 0; j < 64; j++) {
            h2[i] += h1[j] * W2[j][i];
        }
        h2[i] = relu(h2[i]);
    }

    // Output Layer
    double out = B3[0];
    for(int j = 0; j < 32; j++) {
        out += h2[j] * W3[j][0];
    }

    // Convert margin to probability (0 to 1)
    return sigmoid(out);
}
"""
else:
    print(f"[WARNING] Could not find Neural Network at {nn_path}.")

# 3. Save the Unified C++ Header
output_path = os.path.join(ROOT_DIR, "include", "ShadowModels.h")
with open(output_path, "w") as f:
    f.write(cpp_code)

print(f"\n[SUCCESS] Unified C++ Consensus Engine written to {output_path}!")
print("==================================================")