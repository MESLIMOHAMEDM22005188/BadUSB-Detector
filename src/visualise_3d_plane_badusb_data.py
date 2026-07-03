# import pandas as pd
# import seaborn as sns
# import matplotlib.pyplot as plt
# import matplotlib.patches as mpatches
# import os
# import math
#
# # 1. SETUP DIRECTORY
# output_dir = "../images"
# if not os.path.exists(output_dir):
#     os.makedirs(output_dir)
#     print(f"Created directory: {output_dir}")
#
# # 2. LOAD DATA
# try:
#     human = pd.read_csv('../data/training_data/human_data.csv', on_bad_lines='skip')
#     human['Label'] = 'Human'
#
#     badusb = pd.read_csv('../data/training_data/badusb_data.csv', on_bad_lines='skip')
#     badusb['Label'] = 'BadUSB'
#
#     badusb = badusb.rename(columns=lambda x: x.replace(';', '').strip())
#     human = human.rename(columns=lambda x: x.replace(';', '').strip())
#
#     df = pd.concat([human, badusb], ignore_index=True)
#     df = df[df['flight_time_ms'] != 0]
#     print("Data loaded successfully.")
#
# except FileNotFoundError:
#     print("Error: CSV files not found. Please check paths.")
#     exit()
#
# # PART A: SAVE THE FULL DASHBOARD (All 6 in 1)
# fig, axes = plt.subplots(2, 3, figsize=(18, 12), gridspec_kw={'hspace': 0.4})
# fig.suptitle('Keystroke Dynamics Visualization Dashboard', fontsize=20)
#
# # Plot 1: Distribution
# sns.histplot(data=df, x='key_hold_time_ms', hue='Label', kde=True, ax=axes[0, 0], palette=['blue', 'red'])
# axes[0, 0].set_title('1. Distribution (Hold Time)')
#
# # Plot 2: Variance
# sns.boxplot(data=df, x='Label', y='flight_time_ms', ax=axes[0, 1], palette=['blue', 'red'])
# axes[0, 1].set_title('2. Variance (Flight Time)')
# axes[0, 1].set_ylim(-400, 1500)
#
# # Plot 3: Density
# sns.violinplot(data=df, x='Label', y='inter_char_delay_ms', ax=axes[0, 2], palette=['blue', 'red'])
# axes[0, 2].set_title('3. Density Shape (Down-Down)')
# axes[0, 2].set_ylim(-250, 1500)
#
# # Plot 4: Clustering
# sns.scatterplot(data=df, x='flight_time_ms', y='key_hold_time_ms', hue='Label', style='Label', ax=axes[1, 0], palette=['blue', 'red'], alpha=0.6)
# axes[1, 0].set_title('4. Cluster Separation')
# axes[1, 0].set_xlim(0, 6000)
# axes[1, 0].set_ylim(0, 400)
#
# # Plot 5: Rhythm (First 200 Keys)
# sample = df.groupby('Label').head(200).copy()
# sample['Key_Index'] = sample.groupby('Label').cumcount()
# sns.lineplot(data=sample, x='Key_Index', y='flight_time_ms', hue='Label', palette=['blue', 'red'], alpha=0.8, ax=axes[1, 1])
# axes[1, 1].set_title('5. Typing Rhythm (First 200 Keys)')
#
# # Plot 6: Correlation
# numeric_cols = ['flight_time_ms', 'inter_char_delay_ms', 'key_hold_time_ms']
# corr = df[numeric_cols].corr()
# sns.heatmap(corr, annot=True, cmap='coolwarm', ax=axes[1, 2])
# axes[1, 2].set_title('6. Feature Correlation')
#
# plt.tight_layout(rect=[0, 0.03, 1, 0.95])
# plt.savefig(f'{output_dir}/dashboard_all.png')
# plt.close()
# print(f"Saved: {output_dir}/dashboard_all.png")
#
# # PART B: SAVE INDIVIDUAL GRAPHS (6 Separate Images)
#
# plt.figure(figsize=(8, 6))
# sns.histplot(data=df, x='key_hold_time_ms', hue='Label', kde=True, palette=['blue', 'red'])
# plt.title('Distribution of Hold Time')
# plt.savefig(f'{output_dir}/1_distribution_hold_time.png')
# plt.close()
#
# plt.figure(figsize=(8, 6))
# sns.boxplot(data=df, x='Label', y='flight_time_ms', palette=['blue', 'red'])
# plt.title('Variance of Flight Time')
# plt.ylim(-400, 1500)
# plt.savefig(f'{output_dir}/2_variance_flight_time.png')
# plt.close()
#
# plt.figure(figsize=(8, 6))
# sns.violinplot(data=df, x='Label', y='inter_char_delay_ms', palette=['blue', 'red'])
# plt.title('Density Shape of Inter-Char Delay')
# plt.ylim(-250, 1500)
# plt.savefig(f'{output_dir}/3_density_inter_char.png')
# plt.close()
#
# plt.figure(figsize=(8, 6))
# sns.scatterplot(data=df, x='flight_time_ms', y='key_hold_time_ms', hue='Label', style='Label', palette=['blue', 'red'], alpha=0.6)
# plt.title('Cluster Separation (Flight vs Hold)')
# plt.xlim(0, 6000)
# plt.ylim(0, 400)
# plt.savefig(f'{output_dir}/4_cluster_separation.png')
# plt.close()
#
# plt.figure(figsize=(10, 6))
# sns.lineplot(data=sample, x='Key_Index', y='flight_time_ms', hue='Label', palette=['blue', 'red'], alpha=0.8)
# plt.title('Typing Rhythm (First 200 Keys)')
# plt.xlabel('Keystroke Sequence')
# plt.ylabel('Flight Time (ms)')
# plt.savefig(f'{output_dir}/5_typing_rhythm.png')
# plt.close()
#
# plt.figure(figsize=(8, 6))
# sns.heatmap(corr, annot=True, cmap='coolwarm')
# plt.title('Feature Correlation Matrix')
# plt.savefig(f'{output_dir}/6_correlation_matrix.png')
# plt.close()
#
# print(f"All 6 standard graphs saved to: {output_dir}/")
#
#
# # PART C: ADVANCED GRAPHS (7 through 10)
# print("Generating Advanced Graphs... (This may take a moment)")
#
# # Filter extreme outliers just for these graphs to keep them visually clean
# df_adv = df.dropna(subset=['flight_time_ms', 'key_hold_time_ms', 'inter_char_delay_ms'])
# df_adv = df_adv[(df_adv['flight_time_ms'] > 0) & (df_adv['flight_time_ms'] < 2000)]
# df_adv = df_adv[(df_adv['key_hold_time_ms'] > 0) & (df_adv['key_hold_time_ms'] < 200)]
#
# # --- 7. PAIRPLOT (Feature Matrix) ---
# sns.pairplot(df_adv[['flight_time_ms', 'inter_char_delay_ms', 'key_hold_time_ms', 'Label']],
#              hue='Label', palette=['red', 'blue'], plot_kws={'alpha': 0.5}, diag_kind='kde')
# plt.suptitle("Pairwise Feature Relationships: Human vs Algorithmic Typing", y=1.02, fontsize=16)
# plt.savefig(f"{output_dir}/7_pairplot.png", bbox_inches='tight')
# plt.close()
#
# # --- 8. 2D KDE JOINTPLOT (Density Topography) ---
# g = sns.jointplot(data=df_adv, x="flight_time_ms", y="key_hold_time_ms", hue="Label",
#                   palette=['red', 'blue'], kind="kde", alpha=0.7, fill=True)
# g.fig.suptitle("2D Density Topography: Flight vs Hold Time", y=1.02, fontsize=14)
# g.set_axis_labels('Flight Time (ms)', 'Key Hold Time (ms)')
# plt.savefig(f"{output_dir}/8_kde_density.png", bbox_inches='tight')
# plt.close()
#
# # --- 9. 3D SCATTER PLOT ---
# fig = plt.figure(figsize=(10, 8))
# ax = fig.add_subplot(111, projection='3d')
#
# df_sample = df_adv.groupby('Label').apply(lambda x: x.sample(min(len(x), 500))).reset_index(drop=True)
#
# colors = {'Human': 'blue', 'BadUSB': 'red'}
# ax.scatter(df_sample['flight_time_ms'], df_sample['key_hold_time_ms'], df_sample['inter_char_delay_ms'],
#            c=df_sample['Label'].map(colors), alpha=0.6, s=20)
#
# ax.set_xlabel('Flight Time (ms)')
# ax.set_ylabel('Hold Time (ms)')
# ax.set_zlabel('Inter-Char Delay (ms)')
# plt.title('3D Geometric Boundary of Keystroke Dynamics', fontsize=16)
#
# red_patch = mpatches.Patch(color='red', label='BadUSB')
# blue_patch = mpatches.Patch(color='blue', label='Human')
# plt.legend(handles=[red_patch, blue_patch])
#
# plt.savefig(f"{output_dir}/9_3d_scatter.png", bbox_inches='tight')
# plt.close()
#
# # --- 10. RADAR/SPIDER CHART (Biometric Signature) ---
# categories = ['Flight Time', 'Inter-Char Delay', 'Hold Time']
# N = len(categories)
#
# df_numeric = df_adv[['flight_time_ms', 'inter_char_delay_ms', 'key_hold_time_ms']]
# df_normalized = (df_numeric - df_numeric.min()) / (df_numeric.max() - df_numeric.min())
# df_normalized['Label'] = df_adv['Label']
#
# means = df_normalized.groupby('Label').mean()
#
# angles = [n / float(N) * 2 * math.pi for n in range(N)]
# angles += angles[:1]
#
# fig, ax = plt.subplots(figsize=(6, 6), subplot_kw=dict(polar=True))
# plt.xticks(angles[:-1], categories, size=12)
#
# # Plot Human Signature
# values_human = means.loc['Human'].values.flatten().tolist()
# values_human += values_human[:1]
# ax.plot(angles, values_human, linewidth=2, linestyle='solid', label='Human (Gaussian)', color='blue')
# ax.fill(angles, values_human, 'blue', alpha=0.25)
#
# # Plot BadUSB Signature
# values_badusb = means.loc['BadUSB'].values.flatten().tolist()
# values_badusb += values_badusb[:1]
# ax.plot(angles, values_badusb, linewidth=2, linestyle='solid', label='BadUSB (Uniform)', color='red')
# ax.fill(angles, values_badusb, 'red', alpha=0.25)
#
# plt.title('Normalized Biometric Signature Profiles', y=1.08, fontsize=16)
# plt.legend(loc='upper right', bbox_to_anchor=(0.1, 0.1))
# plt.savefig(f"{output_dir}/10_radar_signature.png", bbox_inches='tight')
# plt.close()
#
# print(f"Success! All 10 graphs (Standard + Advanced) have been saved to the {output_dir} folder.")