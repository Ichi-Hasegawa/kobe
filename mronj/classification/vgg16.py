#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import datetime
import os
from pathlib import Path

import pandas as pd
import torch
import torch.nn as nn
import torch.utils.data
import torchvision.models
from torch.utils.tensorboard import SummaryWriter
from torchmetrics.classification import F1Score, Recall, Precision

from metric import transform_select, dataloader, model_select, eva_index
from utils.setup import EarlyStopping, VggBackbone

device = 'cuda:0' if torch.cuda.is_available() else 'cpu'
torch.autograd.set_detect_anomaly(False)
torch.backends.cudnn.benchmark = True
print(device)


def exec_training(
        dataset: Path,
        work_root: Path,
        model: str,
        grid: (int, int),
        epochs: int,
        batch_size: int,
        num_classes: int,
        part: str
):
    # Temporary working directory
    workdir = work_root / datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    workdir.mkdir(parents=True, exist_ok=True)

    transform = transform_select(model)

    # load dataloader
    train_loader, valid_loader, test_loader = dataloader(
        data=Path(dataset/"list.csv"),
        splits=Path(dataset/"20250307.csv"),
        part=part,
        grid=grid,
        transform=transform,
        batch_size=batch_size
    )

    # Init model
    net = model_select(model=model, num_classes=num_classes)
    net.to(device)

    # optimizer = torch.optim.RAdam(net.parameters(), lr=0.001)
    optimizer = torch.optim.Adam(net.parameters(), lr=0.001)
    # optimizer = torch.optim.SGD(net.parameters(), lr=0.001)
    # optimizer = torch.optim.SGD(net.parameters(), lr=0.001, momentum=0.9)
    
    # criterion = nn.BCEWithLogitsLoss()
    criterion = nn.BCELoss()

    with open(workdir / "model.txt", "w") as f:
        f.write(f"{net}\n{optimizer}\n{grid}\n{part}\n{batch_size}")

    # metrics
    f1, recall, precision = eva_index(device=device, num_classes=num_classes)

    # Logging by TensorBoard
    logger = SummaryWriter(log_dir=str(workdir))

    # early_stopping = EarlyStopping(patience=10, verbose=True)
    for epoch in range(epochs):
        print(f"Epoch [{epoch:5}/{epochs:5}]")

        metrics = {
            'train': {'loss': .0, 'f1': .0, 'recall': .0, 'precision': .0},
            'valid': {'loss': .0, 'f1': .0, 'recall': .0, 'precision': .0},
            'test': {'loss': .0, 'f1': .0, 'recall': .0, 'precision': .0}
        }

        # Switch to training mode
        net.train()

        for batch, (x, y_true, _) in enumerate(train_loader):
        # for batch, (x, y_true) in enumerate(train_loader):
            x, y_true = x.to(device), y_true.to(device)
            y_pred = net(x).to(torch.float32)
            y_true = y_true.to(torch.float32)
            # print(y_pred)

            loss = criterion(y_pred, y_true)
            loss.backward()
            optimizer.step()
            optimizer.zero_grad(set_to_none=True)
            # scheduler.step()

            print(f"{epoch}-{batch}: pred", y_pred.argmax(dim=1).to('cpu').detach().numpy())
            print(f"{epoch}-{batch}: true", y_true.argmax(dim=1).to('cpu').detach().numpy())

            y_pred = y_pred.argmax(dim=1)
            # print(y_pred)
            y_true = y_true.argmax(dim=1)

            metrics['train']['loss'] += loss.item() / len(train_loader)
            metrics['train']['f1'] += f1(y_pred, y_true).item() / len(train_loader)
            metrics['train']['recall'] += recall(y_pred, y_true).item() / len(train_loader)
            metrics['train']['precision'] += precision(y_pred, y_true).item() / len(train_loader)

            print("f1=", f1(y_pred, y_true).item())
            print("recall=", recall(y_pred, y_true).item())
            print("precision=", precision(y_pred, y_true).item())

            print("\r Batch({:6}/{:6})[{}]: loss={:.4}".format(
                batch, len(train_loader),
                ('=' * (30 * batch // len(train_loader)) + " " * 30)[:30],
                loss.item(),
                ", ".join([
                    f'{key}={metrics["train"][key]:.2}'
                    for key in ['f1', 'recall', 'precision']
                ])
            ), end="")
        print('')

        # ??
        # model = VggBackbone(
        #     base_model="vgg16",
        #     out_dim=256
        # )
        # state_dict = model.backbone.state_dict()
        # torch.save(state_dict(), workdir / f"{net.__class__.__name__}_{epoch:04}.pth")

        # Save trained model
        torch.save(net.state_dict(), workdir / f"{net.__class__.__name__}_{epoch:04}.pth")

        # Switch to training mode
        net.eval()
        with torch.no_grad():
            for x, y_true, _ in valid_loader:
                x, y_true = x.to(device), y_true.to(device)
                y_pred = net(x).to(torch.float32)
                y_true = y_true.to(torch.float32)

                y_pred_c = y_pred.argmax(dim=1)
                y_true_c = y_true.argmax(dim=1)

                metrics['valid']['loss'] += criterion(y_pred, y_true).item() / len(valid_loader)
                metrics['valid']['f1'] += f1(y_pred_c, y_true_c).item() / len(valid_loader)
                metrics['valid']['recall'] += recall(y_pred_c, y_true_c).item() / len(valid_loader)
                metrics['valid']['precision'] += precision(y_pred_c, y_true_c).item() / len(valid_loader)

        print('Validation: f1={:.2}, recall={:.2}, precision={:.2}'.format(
            metrics['valid']['f1'],
            metrics['valid']['recall'],
            metrics['valid']['precision']
        ))

        with torch.no_grad():
            for x, y_true, _ in test_loader:
                x, y_true = x.to(device), y_true.to(device)
                y_pred = net(x).to(torch.float32)
                y_true = y_true.to(torch.float32)

                y_pred_c = y_pred.argmax(dim=1)
                y_true_c = y_true.argmax(dim=1)

                metrics['test']['loss'] += criterion(y_pred, y_true).item() / len(test_loader)
                metrics['test']['f1'] += f1(y_pred_c, y_true_c).item() / len(test_loader)
                metrics['test']['recall'] += recall(y_pred_c, y_true_c).item() / len(test_loader)
                metrics['test']['precision'] += precision(y_pred_c, y_true_c).item() / len(test_loader)

            # early_stopping(metrics['test']['loss'], net)
            # if early_stopping.early_stop:
            #     print("Early Stopping!")
            #     break
        print('test: f1={:.2}, recall={:.2}, precision={:.2}'.format(
            metrics['test']['f1'],
            metrics['test']['recall'],
            metrics['test']['precision']
        ))

        # Logging to tensorboard
        for ds_name, ds_vals in metrics.items():
            for key, val in ds_vals.items():
                logger.add_scalar(f"{ds_name}/{key}", val, epoch)

    return workdir


def main():
    dataset = Path("~/workspace/kobe/mronj/_data/").expanduser()
    root = Path("~/workspace/kobe/mronj/_out").expanduser()

    workdir = exec_training(
        dataset=dataset,
        work_root=root,
        model="vgg",
        grid=(3, 2),
        epochs=300,
        batch_size=128,
        num_classes=2,
        part="part3"
    )

    print("Result written in:", workdir)


if __name__ == '__main__':
    main()
