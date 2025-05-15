import pandas as pd
from sklearn.model_selection import train_test_split

# CSV読み込み
df = pd.read_csv('list-250514.csv')
df.columns = df.columns.str.strip()  # 念のため空白削除

# -------------------------
# 1. subjectごとに最大hypoesthesia値をとって陽性ラベル化
# -------------------------
subject_labels = df.groupby('subject')['hypoesthesia'].max().reset_index()
subject_labels.columns = ['subject', 'label']  # 1=陽性含む, 0=陰性のみ

# 陽性・陰性患者をリスト化
pos_subjects = subject_labels[subject_labels['label'] == 1]['subject'].tolist()
neg_subjects = subject_labels[subject_labels['label'] == 0]['subject'].tolist()

# -------------------------
# 2. subject単位でtrain/valid/test分割
# -------------------------
# 陽性患者: train=21, valid=10, test=12（人数指定）
pos_train, pos_temp = train_test_split(pos_subjects, train_size=21, random_state=42)
pos_valid, pos_test = train_test_split(pos_temp, test_size=12, random_state=42)

# 陰性患者: 6:2:2 の比率
neg_train, neg_temp = train_test_split(neg_subjects, test_size=0.4, random_state=42)
neg_valid, neg_test = train_test_split(neg_temp, test_size=0.5, random_state=42)

# -------------------------
# 3. subject → dataset 対応マップ作成
# -------------------------
subject_to_dataset = {}
for s in pos_train:  subject_to_dataset[s] = 'train'
for s in pos_valid:  subject_to_dataset[s] = 'valid'
for s in pos_test:   subject_to_dataset[s] = 'test'
for s in neg_train:  subject_to_dataset[s] = 'train'
for s in neg_valid:  subject_to_dataset[s] = 'valid'
for s in neg_test:   subject_to_dataset[s] = 'test'

# -------------------------
# 4. ユニークな subject を抽出して保存
# -------------------------
out_df = pd.DataFrame([
    {'subject': s, 'dataset': d}
    for s, d in subject_to_dataset.items()
])
out_df.to_csv('subject_split_fixed.csv', index=False)
