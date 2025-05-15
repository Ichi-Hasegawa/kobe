import torch
import torch.nn as nn
import torchvision
from pathlib import Path
from torch.utils.data import DataLoader
from torchmetrics.classification import F1Score, Recall, Precision, AUROC
from utils.loader import load_dataset, XpDataset

device = 'cuda:0' if torch.cuda.is_available() else 'cpu'

# データ準備
root = Path("/net/nfs3/export/dataset/morita/kobe-u/oral/MandibularCanal/")
annotation = Path("./_data/list-241120.csv").absolute()
split = Path("./_data/split_241120.csv").absolute()
label = "hypoesthesia"

_, valid_list, _ = load_dataset(root=root, annotation=annotation, split=split)
transform = torchvision.transforms.Compose([
    torchvision.transforms.ConvertImageDtype(torch.float32),
    torchvision.transforms.Resize((224, 224), torchvision.transforms.InterpolationMode.BILINEAR),
    torchvision.transforms.Normalize(mean=(2047.5, 2047.5, 2047.5), std=(2047.5, 2047.5, 2047.5))
])
valid_dataset = XpDataset(label=label, data=valid_list, transform=transform)
valid_loader = DataLoader(valid_dataset, batch_size=32, shuffle=False, num_workers=4, pin_memory=True)

num_classes = valid_dataset.c

# モデル初期化（未学習）
net = torchvision.models.vgg16_bn(weights=torchvision.models.VGG16_BN_Weights)
net.classifier[6] = nn.Sequential(
    nn.Linear(net.classifier[6].in_features, num_classes, bias=True),
    nn.Sigmoid()
)
net.to(device)
net.eval()

# 指標定義
criterion = nn.BCELoss()
f1 = F1Score(task="binary", num_classes=num_classes).to(device)
recall = Recall(task="binary", threshold=0.5, num_classes=num_classes).to(device)
precision = Precision(task="binary", average='macro', num_classes=num_classes).to(device)
auroc = AUROC(task="binary", average='macro', num_classes=num_classes).to(device)

# 評価
metrics = {'loss': 0.0, 'f1': 0.0, 'recall': 0.0, 'precision': 0.0, 'auroc': 0.0}

with torch.no_grad():
    for x, y_true in valid_loader:
        x, y_true = x.to(device), y_true.to(device)
        y_pred = net(x).float()
        y_true = y_true.float()

        metrics['loss'] += criterion(y_pred, y_true).item() / len(valid_loader)

        y_pred_cls = y_pred.argmax(dim=1)
        y_true_cls = y_true.argmax(dim=1)

        metrics['f1'] += f1(y_pred_cls, y_true_cls).item() / len(valid_loader)
        metrics['recall'] += recall(y_pred_cls, y_true_cls).item() / len(valid_loader)
        metrics['precision'] += precision(y_pred_cls, y_true_cls).item() / len(valid_loader)
        metrics['auroc'] += auroc(y_pred_cls, y_true_cls).item() / len(valid_loader)

# 結果表示
print("=== Untrained Model Evaluation ===")
for k, v in metrics.items():
    print(f"{k}: {v:.4f}")

# validation setに陽性があるか確認
print(torch.stack([y for _, y in valid_dataset]).sum(dim=0))
