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
from torch.optim.lr_scheduler import ReduceLROnPlateau

device = 'cuda:0' if torch.cuda.is_available() else 'cpu'

class ConvConcat(nn.Module):
    def __init__(self,
                 backbones: list[nn.Module],
                 x: torch.Tensor,
                 device: str = "cpu"
                 ):
        super().__init__()
        self.__backbone_feature = 2048
        self.__backbones = []
        for backbone in backbones:
            for param in backbone.parameters():
                param.requires_grad = False  # Freeze backbones
            self.__backbones.append(nn.Sequential(
                backbone,
                nn.AdaptiveAvgPool2d((1, 1)),   # Global average pooling
                nn.Flatten(start_dim=1, end_dim=-1),
                nn.Linear(in_features=backbone(x).shape[1], out_features=self.__backbone_feature),
                nn.ReLU(inplace=True),
                nn.Linear(in_features=self.__backbone_feature, out_features=self.__backbone_feature),
                nn.ReLU(inplace=True)
            ).to(device))

        for i, backbone in enumerate(self.__backbones):
            print(f"Backbone {i}: {backbone}")
        in_features = self.__backbone_feature * len(self.__backbones)

        self.__classifier = nn.Sequential(
            # nn.Flatten(),       # Flatten multiple network outputs
            nn.Linear(in_features=in_features, out_features=4096, bias=True),
            nn.ReLU(inplace=True),
            nn.Dropout(p=0.5, inplace=False),
            nn.Linear(in_features=4096, out_features=2048, bias=True),
            nn.ReLU(inplace=True),
            nn.Dropout(p=0.5, inplace=False),
            nn.Linear(in_features=2048, out_features=1024, bias=True),
            nn.ReLU(inplace=True),
            nn.Dropout(p=0.5, inplace=False),
            nn.Linear(in_features=1024, out_features=512, bias=True),
            nn.ReLU(inplace=True),
            nn.Dropout(p=0.5, inplace=False),
            nn.Linear(in_features=512, out_features=2, bias=True),
            nn.Softmax(dim=-1)
        )
        self.__classifier.to(device)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        feature = [
            backbone(x)
            for backbone in self.__backbones
        ]
        z = torch.concat(feature, dim=1)
        return self.__classifier(z)

def load_base_model(path: Path, out_features: int) -> nn.Module:
    net = torchvision.models.vgg16_bn(weights=torchvision.models.VGG16_BN_Weights)
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

    # Load data list
    train_list, valid_list, test_list = load_dataset(
        root=root, annotation=annotation, split=split
    )

    train_dataset = XpDataset(label=label, data=train_list, transform=transform)
    valid_dataset = XpDataset(label=label, data=valid_list, transform=transform)
    test_dataset = XpDataset(label=label, data=test_list, transform=transform)
    #for img, label in train_dataset:
    #    pass
    #exit()
    num_classes = train_dataset.c

    # Init data loaders
    train_loader = torch.utils.data.DataLoader(
        train_dataset, batch_size=32, shuffle=True, num_workers=os.cpu_count() // 2
    )
    valid_loader = torch.utils.data.DataLoader(
        valid_dataset, batch_size=32, shuffle=True, num_workers=os.cpu_count() // 2
    )
    test_loader = torch.utils.data.DataLoader(
        test_dataset, batch_size=32, shuffle=True, num_workers=os.cpu_count() // 2
    )

    backbones = [
        # hypoesthesia
        load_base_model(
            Path("~/data/_out/kobe-mCanal/20241217_172342/VGG_0044.pth").expanduser(),
            out_features=2
        ),
        # xDepth
        #load_base_model(
        #    Path("~/data/_out/kobe-mCanal/20241202_161401/VGG_0038.pth").expanduser(),
        #    out_features=3
        #),
        # xSpace
        #load_base_model(
        #    Path("~/data/_out/kobe-mCanal/20241202_183213/VGG_0050.pth").expanduser(),
        #    out_features=3
        #),
        # axis
        #load_base_model(
        #    Path("~/data/_out/kobe-mCanal/20241204_152347/VGG_0082.pth").expanduser(),
        #    out_features=4
        #),
        # RCnumber
        #load_base_model(
        #    Path("~/data/_out/kobe-mCanal/20241206_171200/VGG_0054.pth").expanduser(),
        #    out_features=3
        #),
        # RCtype
        #load_base_model(
        #    Path("~/data/_out/kobe-mCanal/20241209_145838/VGG_0027.pth").expanduser(),
        #    out_features=3
        #),
        # clarity
        load_base_model(
            Path("~/data/_out/kobe-mCanal/20241212_144122/VGG_0064.pth").expanduser(),
            out_features=2
        ),
        # risk
        #load_base_model(
        #    Path("~/data/_out/kobe-mCanal/20241213_180238/VGG_0038.pth").expanduser(),
        #    out_features=3
        #),
        # Mdistance
        #load_base_model(
        #    Path("~/data/_out/kobe-mCanal/20241129_172607/VGG_0071.pth").expanduser(),
        #    out_features=3
        #),
        # Mcurve
        load_base_model(
            Path("~/data/_out/kobe-mCanal/20241214_135402/VGG_0083.pth").expanduser(),
            out_features=2
        )#,
        # AMposition
        #load_base_model(
        #    Path("~/data/_out/kobe-mCanal/20241216_170022/VGG_0068.pth").expanduser(),
        #    out_features=4
        #)
    ]

    # Initialize fusion model
    net = ConvConcat(
        backbones = backbones,
        x = torch.rand((32, 3, 224, 224)),
        device = device)

    epochs = 300

    #optimizer = torch.optim.Adam(net.parameters(), lr = 0.001)
    optimizer = torch.optim.RAdam(net.parameters(), lr = 0.001)
    #optimizer = torch.optim.AdamW(net.parameters(), lr=1e-4, weight_decay=1e-2)

    criterion = nn.BCELoss()
    #criterion = nn.BCEWithLogitsLoss()

    #scheduler = ReduceLROnPlateau(optimizer, mode='min', factor=0.1, patience=3)

    # metrics
    f1 = F1Score(task="binary", num_classes=num_classes).to(device)
    #f1 = BinaryF1Score(threshold=0.5).to(device)
    recall = Recall(task="binary", average='macro', num_classes=num_classes).to(device)
    precision = Precision(task="binary", average='macro', num_classes=num_classes).to(device)
    auroc = AUROC(task="binary", num_classes=num_classes).to(device)

    # Temporary working directory
    workdir = work_root / datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    workdir.mkdir(parents=True, exist_ok=True)
    # Logging by TensorBoard
    logger = SummaryWriter(log_dir=str(workdir))

    for epoch in range(epochs):
        print(f"Epoch [{epoch:5}/{epochs:5}]")

        # TODO: ConfusionMatrix does not support 3-classes now.
        metrics = {
            'train': {'loss': .0, 'f1': .0, 'recall': .0, 'precision': .0, "auroc": .0},
            'valid': {'loss': .0, 'f1': .0, 'recall': .0, 'precision': .0, "auroc": .0},
            'test': {'loss': .0, 'f1': .0, 'recall': .0, 'precision': .0, "auroc": .0},
        }

        # Switch to training mode
        net.train()
        for batch, (x, y_true) in enumerate(train_loader):
            x, y_true = x.to(device), y_true.to(device)
            y_pred = net(x).to(torch.float32)
            y_true = y_true.to(torch.float32)
            
            loss = criterion(y_pred, y_true)
            loss.backward()
            optimizer.step()
            optimizer.zero_grad(set_to_none=True)

            print(f"{epoch}-{batch}: train_pred", y_pred.argmax(dim=1).to('cpu').detach().numpy())
            print(f"{epoch}-{batch}: train_true", y_true.argmax(dim=1).to('cpu').detach().numpy())

            y_pred = y_pred.argmax(dim=1)
            y_true = y_true.argmax(dim=1)

            metrics['train']['loss'] += loss.item() / len(train_loader)
            metrics['train']['f1'] += f1(y_pred, y_true).item() / len(train_loader)
            metrics['train']['recall'] += recall(y_pred, y_true).item() / len(train_loader)
            metrics['train']['precision'] += precision(y_pred, y_true).item() / len(train_loader)
            metrics['train']['auroc'] += auroc(y_pred, y_true).item() / len(train_loader)

            print("\r  Batch({:6}/{:6})[{}]: loss={:.4}, {}".format(
                batch, len(train_loader),
                ('=' * (30 * batch // len(train_loader)) + " " * 30)[:30],
                loss.item(),
                ", ".join([
                    f'{key}={metrics["train"][key]:.2}'
                    for key in ['f1', 'recall', 'precision', 'auroc']
                ])
            ), end="")
            print('\n')
        print('')


        # Save trained model
        torch.save(net.state_dict(), workdir / f"{net.__class__.__name__}_{epoch:04}.pth")

        # Switch to training mode
        net.eval()
        with torch.no_grad():
            for x, y_true in valid_loader:
                x, y_true = x.to(device), y_true.to(device)
                y_pred = net(x)

                print(f"{epoch}-{batch}: val_pred", y_pred.argmax(dim=1).to('cpu').detach().numpy())
                print(f"{epoch}-{batch}: val_true", y_true.argmax(dim=1).to('cpu').detach().numpy())
                y_pred_c = y_pred.argmax(dim=1)
                y_true_c = y_true.argmax(dim=1)

                metrics['valid']['loss'] += criterion(y_pred, y_true).item() / len(valid_loader)
                metrics['valid']['f1'] += f1(y_pred_c, y_true_c).item() / len(valid_loader)
                metrics['valid']['recall'] += recall(y_pred_c, y_true_c).item() / len(valid_loader)
                metrics['valid']['precision'] += precision(y_pred_c, y_true_c).item() / len(valid_loader)
                metrics['valid']['auroc'] += auroc(y_pred_c, y_true_c).item() / len(valid_loader)

                print('                    Validation: loss={:.2}, f1={:.2}, recall={:.2}, precision={:.2}, auroc={:.2}'.format(
                    metrics['valid']['loss'],
                    metrics['valid']['f1'],
                    metrics['valid']['recall'],
                    metrics['valid']['precision'],
                    metrics['valid']['auroc']
                ))

        with torch.no_grad():
            for x, y_true in test_loader:
            # for x, y_true in test_loader:
                x, y_true = x.to(device), y_true.to(device)
                y_pred = net(x).to(torch.float32)
                y_true = y_true.to(torch.float32)

                y_pred_c = y_pred.argmax(dim=1)
                y_true_c = y_true.argmax(dim=1)

                print(f"{epoch}-{batch}: test_pred", y_pred_c.to('cpu').detach().numpy())
                print(f"{epoch}-{batch}: test_true", y_true_c.to('cpu').detach().numpy())

                metrics['test']['loss'] += criterion(y_pred, y_true).item() / len(test_loader)
                metrics['test']['f1'] += f1(y_pred_c, y_true_c).item() / len(test_loader)
                metrics['test']['recall'] += recall(y_pred_c, y_true_c).item() / len(test_loader)
                metrics['test']['precision'] += precision(y_pred_c, y_true_c).item() / len(test_loader)
                metrics['test']['auroc'] += auroc(y_pred_c, y_true_c).item() / len(test_loader)

            # early_stopping(metrics['test']['loss'], net)
            # if early_stopping.early_stop:
            #     print("Early Stopping!")
            #     break
                print('                    Test: loss={:.2}, f1={:.2}, recall={:.2}, precision={:.2}, auroc={:.2}'.format(
                    metrics['test']['loss'],
                    metrics['test']['f1'],
                    metrics['test']['recall'],
                    metrics['test']['precision'],
                    metrics['test']['auroc']
                ))

        # Logging to tensorboard
        #   Ex.) logger.add_scalar('train/loss', metrics['train']['loss'], epoch)
        for ds_name, ds_vals in metrics.items():
            for key, val in ds_vals.items():
                logger.add_scalar(f"{ds_name}/{key}", val, epoch)

    return workdir


def main():
    workdir = exec_training(
        root=Path("/net/nfs3/export/dataset/morita/kobe-u/oral/MandibularCanal/"),
        annotation=Path("./_data/list-241120.csv").absolute(),
        split=Path("./_data/split_4-3.csv").absolute(),
        label="hypoesthesia",
        work_root=Path("~/data/_out/kobe-mCanal/").expanduser()
    )

    print("Results written in:", workdir)


if __name__ == '__main__':
    main()