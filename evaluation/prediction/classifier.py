#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import datetime
from pathlib import Path

import torch
import torch.nn as nn
import torch.utils.data
import torchvision
from torch.utils.tensorboard import SummaryWriter
from torchmetrics.classification import F1Score, Recall, Precision, AUROC

from utils.loader import load_dataset, XpDataset

from torch.nn.functional import softmax

device = 'cuda:0' if torch.cuda.is_available() else 'cpu'

def load_base_model(path: Path, out_features: int) -> nn.Module:
    net = torchvision.models.vgg16()
    net.classifier[6] = nn.Sequential(
        nn.Linear(
            in_features=net.classifier[6].in_features,
            out_features=out_features,
            bias=True
        ),
        nn.Sigmoid()
    )
    net.load_state_dict(torch.load(path))
    net = net.features
    return net

def exec_training(
        root: Path,
        annotation: Path,
        split: Path,
        label: str,
        work_root: Path
):
    transform = torchvision.transforms.Compose([
        torchvision.transforms.ConvertImageDtype(torch.float32),
        torchvision.transforms.RandomRotation(
            (-180, 180),
            torchvision.transforms.InterpolationMode.BILINEAR,
            expand=True
        ),
        torchvision.transforms.RandomHorizontalFlip(0.25),
        torchvision.transforms.RandomVerticalFlip(0.25),
        torchvision.transforms.Resize(
            (224, 224),
            torchvision.transforms.InterpolationMode.BILINEAR
        ),
        torchvision.transforms.Normalize(
            mean=(2047.5, 2047.5, 2047.5),
            std=(2047.5, 2047.5, 2047.5)
        )
    ])

    train_list, valid_list, test_list = load_dataset(
        root=root, annotation=annotation, split=split
    )

    train_dataset = XpDataset(label=label, data=train_list, transform=transform)
    valid_dataset = XpDataset(label=label, data=valid_list, transform=transform)
    num_classes = train_dataset.c

    train_loader = torch.utils.data.DataLoader(
        train_dataset, batch_size=32, shuffle=True,
        num_workers=os.cpu_count() // 2, pin_memory=True
    )
    valid_loader = torch.utils.data.DataLoader(
        valid_dataset, batch_size=32, shuffle=True,
        num_workers=os.cpu_count() // 2, pin_memory=True
    )

    net = torchvision.models.vgg16_bn(weights=torchvision.models.VGG16_BN_Weights)
    net.classifier[6] = nn.Sequential(
        nn.Linear(
            in_features=net.classifier[6].in_features,
            out_features=num_classes,
            bias=True
        ),
        nn.Sigmoid()
    )
    net.to(device)

    epochs = 600
    optimizer = torch.optim.Adam(net.parameters(), lr=0.001)
    criterion = nn.BCELoss()

    f1 = F1Score(task="binary", num_classes=num_classes).to(device)
    recall = Recall(task="binary", threshold=0.5, num_classes=num_classes).to(device)
    precision = Precision(task="binary", average='macro', num_classes=num_classes).to(device)
    auroc = AUROC(task="binary", average='macro', num_classes=num_classes).to(device)

    workdir = work_root / datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    workdir.mkdir(parents=True, exist_ok=True)
    logger = SummaryWriter(log_dir=str(workdir))

    for epoch in range(epochs):
        print(f"Epoch [{epoch:5}/{epochs:5}]")

        metrics = {
            'train': {'loss': .0, 'f1': .0, 'recall': .0, 'precision': .0, "auroc": .0},
            'valid': {'loss': .0, 'f1': .0, 'recall': .0, 'precision': .0, "auroc": .0},
        }

        net.train()
        for batch, (x, y_true) in enumerate(train_loader):
            x, y_true = x.to(device), y_true.to(device).to(torch.float32)
            y_pred = net(x).to(torch.float32)

            loss = criterion(y_pred, y_true)
            loss.backward()
            optimizer.step()
            optimizer.zero_grad(set_to_none=True)

            y_pred_c = y_pred.argmax(dim=1)
            y_true_c = y_true.argmax(dim=1)

            metrics['train']['loss'] += loss.item() / len(train_loader)
            metrics['train']['f1'] += f1(y_pred_c, y_true_c).item() / len(train_loader)
            metrics['train']['recall'] += recall(y_pred_c, y_true_c).item() / len(train_loader)
            metrics['train']['precision'] += precision(y_pred_c, y_true_c).item() / len(train_loader)
            metrics['train']['auroc'] += auroc(y_pred_c, y_true_c).item() / len(train_loader)

        net.eval()
        with torch.no_grad():
            for x, y_true in valid_loader:
                x = x.to(device)
                y_true = y_true.to(device).to(torch.float32)
                y_pred = net(x).to(torch.float32)

                y_pred_c = y_pred.argmax(dim=1)
                y_true_c = y_true.argmax(dim=1)

                metrics['valid']['loss'] += criterion(y_pred, y_true).item() / len(valid_loader)
                metrics['valid']['f1'] += f1(y_pred_c, y_true_c).item() / len(valid_loader)
                metrics['valid']['recall'] += recall(y_pred_c, y_true_c).item() / len(valid_loader)
                metrics['valid']['precision'] += precision(y_pred_c, y_true_c).item() / len(valid_loader)
                metrics['valid']['auroc'] += auroc(y_pred_c, y_true_c).item() / len(valid_loader)

        print('Validation: loss={:.2}, f1={:.2}, recall={:.2}, precision={:.2}, auroc={:.2}'.format(
            metrics['valid']['loss'],
            metrics['valid']['f1'],
            metrics['valid']['recall'],
            metrics['valid']['precision'],
            metrics['valid']['auroc']
        ))

        for ds_name, ds_vals in metrics.items():
            for key, val in ds_vals.items():
                logger.add_scalar(f"{ds_name}/{key}", val, epoch)

    return workdir

def main():
    workdir = exec_training(
        root=Path("/net/nfs3/export/dataset/morita/kobe-u/oral/MandibularCanal/"),
        annotation=Path("../_data/list-241120.csv").absolute(),
        split=Path("../_data/split_241120.csv").absolute(),
        label="hypoesthesia",
        work_root=Path("~/data/_out/kobe-mCanal/").expanduser()
    )

    print("Results written in:", workdir)

if __name__ == '__main__':
    main()

    
    