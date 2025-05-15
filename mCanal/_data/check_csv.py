
import pandas as pd

def check_split_stats(subject_split_csv: str, full_data_csv: str):
    # 分割情報と全データを読み込み
    split_df = pd.read_csv(subject_split_csv)
    full_df = pd.read_csv(full_data_csv)
    split_df.columns = split_df.columns.str.strip()
    full_df.columns = full_df.columns.str.strip()

    # データセット情報を full_df に付加
    df = pd.merge(full_df, split_df, on='subject', how='inner')

    # 各 subject ごとの最大 hypoesthesia を確認
    subject_labels = df.groupby('subject')['hypoesthesia'].max()

    # 結果格納用
    results = []

    for subset in ['train', 'valid', 'test']:
        subset_df = df[df['dataset'] == subset]

        # 患者一覧
        subjects = subset_df['subject'].unique()
        # 陽性/陰性患者数
        pos_subjects = [s for s in subjects if subject_labels[s] == 1]
        neg_subjects = [s for s in subjects if subject_labels[s] == 0]

        # データ数（行数ベース）
        pos_rows = subset_df[subset_df['hypoesthesia'] == 1].shape[0]
        neg_rows = subset_df[subset_df['hypoesthesia'] == 0].shape[0]

        results.append({
            'dataset': subset,
            '陽性患者数': len(pos_subjects),
            '陰性患者数': len(neg_subjects),
            '陽性データ数': pos_rows,
            '陰性データ数': neg_rows
        })

    # 結果をデータフレームとして表示
    result_df = pd.DataFrame(results)
    print(result_df)
def check_subject_leak(subject_split_csv: str):
    split_df = pd.read_csv(subject_split_csv)
    split_df.columns = split_df.columns.str.strip()

    # subjectごとのdataset数を確認（1つだけならOK）
    subject_counts = split_df.groupby('subject')['dataset'].nunique()
    leaked_subjects = subject_counts[subject_counts > 1]

    if leaked_subjects.empty:
        print("✅ データリークなし：全てのsubjectは1つのセットにのみ含まれています。")
    else:
        print("❌ データリーク検出：以下のsubjectが複数セットに存在します。")
        print(leaked_subjects)

# 例:
check_subject_leak('subject_split_fixed.csv')

# 例:
#check_split_stats('subject_split_fixed.csv', 'list-250514.csv')
