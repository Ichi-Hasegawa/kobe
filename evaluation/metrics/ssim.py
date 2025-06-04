import numpy as np
import nibabel as nib
from skimage.metrics import structural_similarity as ssim
import matplotlib.pyplot as plt
import os
import cv2
import pandas as pd
import seaborn as sns

def load_nifti_image(file_path):
    if not os.path.exists(file_path):
        return None  
    img = nib.load(file_path)
    return img.get_fdata()

def normalize_image(img):
    min_val = np.min(img)
    max_val = np.max(img)
    # 最小値を0、最大値を1に正規化
    return (img - min_val) / (max_val - min_val)

def calculate_ssim(generated_img, reference_img):
    if generated_img.shape != reference_img.shape:
        generated_img = cv2.resize(generated_img, (reference_img.shape[1], reference_img.shape[0]))
    
    generated_img = normalize_image(generated_img)
    reference_img = normalize_image(reference_img)
    
    data_range = reference_img.max() - reference_img.min()

    ssim_value, _ = ssim(generated_img, reference_img, full=True, data_range = data_range)
    return ssim_value

# 画像ディレクトリのパス
reference_image_dir = '/net/nfs3/export/home/hasegawa/workspace/kobe-oral/PanoramaCT/data/xp/nifti/'
generated_image_dirs = [
    '/net/nfs3/export/home/hasegawa/workspace/kobe-oral/PanoramaCT/data/ctxp/Parida/sampling_off_headpos_off/nifti/',
    '/net/nfs3/export/home/hasegawa/workspace/kobe-oral/PanoramaCT/data/ctxp/Parida/sampling_off_headpos_on/nifti/',
    '/net/nfs3/export/home/hasegawa/workspace/kobe-oral/PanoramaCT/data/ctxp/Parida/sampling_on_headpos_off/nifti/',
    '/net/nfs3/export/home/hasegawa/workspace/kobe-oral/PanoramaCT/data/ctxp/Parida/sampling_on_headpos_on/nifti/'
]

# 最初の324枚の画像ファイル名を指定
image_files = [f"{str(i).zfill(3)}.nii.gz" for i in range(1, 325)]  # 1〜324の画像

# SSIM計算を格納する辞書
ssim_values = {
    'sampling_off_headpos_off': [],
    'sampling_off_headpos_on': [],
    'sampling_on_headpos_off': [],
    'sampling_on_headpos_on': []
}

# スキップしなかった画像ファイル名を保持するリスト
valid_image_files = []

# 各画像のSSIMを計算
for image_file in image_files:
    # 提供画像の読み込み
    reference_image = load_nifti_image(os.path.join(reference_image_dir, image_file))
    
    # もし提供画像がない場合、スキップ
    if reference_image is None:
        print(f"Reference image {image_file} not found. Skipping.")
        continue
    
    # 各生成画像と比較
    for idx, generated_image_dir in enumerate(generated_image_dirs):
        generated_image = load_nifti_image(os.path.join(generated_image_dir, image_file))
        
        # 生成画像がない場合、スキップ
        if generated_image is None:
            print(f"Generated image {image_file} not found in {generated_image_dir}. Skipping.")
            continue
        
        # SSIM計算
        ssim_val = calculate_ssim(generated_image, reference_image)
        
        # 手法ごとにSSIMを保存
        if idx == 0:
            ssim_values['sampling_off_headpos_off'].append(ssim_val)
        elif idx == 1:
            ssim_values['sampling_off_headpos_on'].append(ssim_val)
        elif idx == 2:
            ssim_values['sampling_on_headpos_off'].append(ssim_val)
        elif idx == 3:
            ssim_values['sampling_on_headpos_on'].append(ssim_val)

    # スキップしなかった画像ファイル名を追加
    valid_image_files.append(image_file)

# 結果をDataFrameに格納
df = pd.DataFrame(ssim_values)
df['image_file'] = valid_image_files  # 画像ファイル名を追加

# 結果を指定されたディレクトリにエクセルファイルとして保存
output_file = '/net/nfs3/export/home/hasegawa/workspace/kobe-oral/PanoramaCT/ImageAnalysis/_out/ssim_results.xlsx'
df.to_excel(output_file, index=False)

print(f"SSIM results saved to {output_file}")

plt.figure(figsize=(10, 6))
sns.boxplot(data=df.drop(columns=['image_file']), palette="Set2")

plt.title('SSIM Comparison for Different Methods (First 324 Images)', fontsize=14)
plt.xlabel('Method', fontsize=12)
plt.ylabel('SSIM Value', fontsize=12)

boxplot_image_path = '/net/nfs3/export/home/hasegawa/workspace/kobe-oral/PanoramaCT/ImageAnalysis/_out/ssim_boxplot.png'
plt.savefig(boxplot_image_path)

print(f"Boxplot saved to {boxplot_image_path}")

plt.show()