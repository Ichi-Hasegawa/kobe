#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from typing import Tuple
import datetime
from pathlib import Path

import torch
import torch.nn as nn
import torch.utils.data
from torch.optim import lr_scheduler
from torch.utils.tensorboard import SummaryWriter

from metric import transform_select, dataloader, model_select, eva_index

device = 'cuda:0' if torch.cuda.is_available() else 'cpu'


def exec_training(
        dataset: Path,
        work_root: Path,
        model: str,
        grid: Tuple[int, int],
        epochs: int,
        batch_size: int,
        num_classes: int,
        part: str
):
    # Temporary working directory
    workdir = work_root / datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    workdir.mkdir(parents=True, exist_ok=True)

    transform = transform_select(model)

    # dataloader
    train_loader, valid_loader, test_loader = dataloader(
        data=Path(dataset/"list.csv"),
        splits=Path(dataset/"20250307.csv"),
        part=part,
        grid=grid,
        transform=transform,
        batch_size=batch_size
    )

    net = model_select(model=model, num_classes=num_classes)
    net.to(device)

    optimizer = torch.optim.Adam(net.parameters(), lr=0.001)
    # criterion = nn.BCEWithLogitsLoss()
    criterion = nn.BCELoss()

    with open(workdir / "model.txt", "w") as f:
        f.write(f"{net}\n{optimizer}\n{grid}\n{part}\n{batch_size}")

    # metrics
    f1, recall, precision = eva_index(device=device, num_classes=num_classes)

    # Logging by TensorBoard
    logger = SummaryWriter(log_dir=str(workdir))

    for epoch in range(epochs):
        print(f"Epoch [{epoch:5}/{epochs:5}]")

        metrics = {
            'train': {'loss': .0, 'f1': .0, 'recall': .0, 'precision': .0},
            'valid': {'loss': .0, 'f1': .0, 'recall': .0, 'precision': .0},
            'test': {'loss': .0, 'f1': .0, 'recall': .0, 'precision': .0}
        }

        # Switch to training mode
        net.train()
        for x, y_true, _ in train_loader:
            # x, y_true = x.to(device), y_true.to(device)
            # y_pred = net(x).to(torch.float32)
            # y_true = y_true.to(torch.float32)
            x, y_true = x.to(device), y_true.to(device)
            y_pred = net(x).to()

            y_pred = y_pred.to(torch.float64)
            y_true = y_true.to(torch.float64)

            loss = criterion(y_pred, y_true)
            loss.backward()
            optimizer.step()
            optimizer.zero_grad(set_to_none=True)

            y_pred = y_pred.argmax(dim=1)
            y_true = y_true.argmax(dim=1)

            # print(f"{epoch}-{batch}: pred", y_pred.argmax(dim=1).to('cpu').detach().numpy())
            # print(f"{epoch}-{batch}: true", y_true.argmax(dim=1).to('cpu').detach().numpy())

            metrics['train']['loss'] += loss.item() / len(train_loader)
            metrics['train']['f1'] += f1(y_pred, y_true).item() / len(train_loader)
            metrics['train']['recall'] += recall(y_pred, y_true).item() / len(train_loader)
            metrics['train']['precision'] += precision(y_pred, y_true).item() / len(train_loader)

        #     print("f1=", f1(y_pred, y_true).item())
        #     print("recall=", recall(y_pred, y_true).item())
        #     print("precision=", precision(y_pred, y_true).item())
        #
        #     print("\r Batch({:6}/{:6})[{}]: loss={:.4}".format(
        #         batch, len(train_loader),
        #         ('=' * (30 * batch // len(train_loader)) + " " * 30)[:30],
        #         loss.item(),
        #         ", ".join([
        #             f'{key}={metrics["train"][key]:.2}'
        #             for key in ['f1', 'recall', 'precision']
        #         ])
        #     ), end="")
        # print('')

        # Save trained model
        # torch.save(net.state_dict(), workdir / f"{net.__class__.__name__}_{epoch:04}.pth")

        # Switch to training mode
        net.eval()
        with torch.no_grad():
            for x, y_true, _ in valid_loader:
                x, y_true = x.to(device), y_true.to(device)
                y_pred = net(x)

                y_pred_c = y_pred.argmax(dim=1)
                y_true_c = y_true.argmax(dim=1)

                metrics['valid']['loss'] += criterion(y_pred, y_true).item() / len(valid_loader)
                metrics['valid']['f1'] += f1(y_pred_c, y_true_c).item() / len(valid_loader)
                metrics['valid']['recall'] += recall(y_pred_c, y_true_c).item() / len(valid_loader)
                metrics['valid']['precision'] += precision(y_pred_c, y_true_c).item() / len(valid_loader)

        # print('Valid: f1={:.2}, recall={:.2}, precision={:.2}'.format(
        #     metrics['valid']['f1'],
        #     metrics['valid']['recall'],
        #     metrics['valid']['precision']
        # ))

        with torch.no_grad():
            for x, y_true, _ in test_loader:
                x, y_true = x.to(device), y_true.to(device)
                y_pred = net(x).to(torch.float32)
                # print(y_pred)
                y_true = y_true.to(torch.float32)

                y_pred_c = y_pred.argmax(dim=1)
                y_true_c = y_true.argmax(dim=1)

                metrics['test']['loss'] += criterion(y_pred, y_true).item() / len(test_loader)
                metrics['test']['f1'] += f1(y_pred_c, y_true_c).item() / len(test_loader)
                metrics['test']['recall'] += recall(y_pred_c, y_true_c).item() / len(test_loader)
                metrics['test']['precision'] += precision(y_pred_c, y_true_c).item() / len(test_loader)

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
    dataset = Path("~/workspace/MRONJ/_data/").expanduser()
    root = Path("~/data/_out/mronj/").expanduser()

    workdir = exec_training(
        dataset=dataset,
        work_root=root,
        model="efficientNet",
        grid=(5, 2),
        epochs=300,
        batch_size=128,
        num_classes=2,
        part="part3"
    )

    print("Result written in:", workdir)


if __name__ == '__main__':
    main()
