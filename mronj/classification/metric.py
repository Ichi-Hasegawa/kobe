#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
from pathlib import Path

import torch
import torch.nn as nn
import torch.utils.data
import torchvision.models
from torchmetrics.classification import F1Score, Recall, Precision

from utils.loader import XpDataset, load_fold
from utils.mobilevit import mobilevit_xxs


def transform_select(model: str):
    resize = 0
    if model == "vgg" or model == "mobileViT" or model == "mskViT":
        resize = (256, 256)
    if model == "inception":
        resize = (299, 299)
    if model == "efficientNet":
        resize = (224, 224)

    transform = torchvision.transforms.Compose([
        torchvision.transforms.ToTensor(),
        torchvision.transforms.ConvertImageDtype(torch.float32),
        torchvision.transforms.RandomRotation(
            (-180, 180),
            torchvision.transforms.InterpolationMode.BILINEAR,
            expand=True
        ),
        torchvision.transforms.RandomHorizontalFlip(0.25),
        torchvision.transforms.RandomVerticalFlip(0.25),
        torchvision.transforms.Resize(
            resize,
            torchvision.transforms.InterpolationMode.BILINEAR
        ),
        torchvision.transforms.Normalize(
            mean=(2047.5, 2047.5, 2047.5),
            std=(2047.5, 2047.5, 2047.5)
        )
    ])

    return transform


def dataloader(
        data: Path, splits: Path, part: str,
        grid: (int, int), transform: torchvision.transforms,
        batch_size: int
):

    # load data list
    train_list, valid_list, test_list = load_fold(
        data=data,
        splits=splits,
        part=part
    )

    train_dataset = XpDataset(data=train_list, grid=grid, transform=transform)
    valid_dataset = XpDataset(data=valid_list, grid=grid, transform=transform)
    test_dataset = XpDataset(data=test_list, grid=grid, transform=transform)

    # Init data loaders
    train_loader = torch.utils.data.DataLoader(
        train_dataset, batch_size=batch_size, shuffle=True,
        num_workers=os.cpu_count() // 2, pin_memory=True
    )
    valid_loader = torch.utils.data.DataLoader(
        valid_dataset, batch_size=batch_size, shuffle=True,
        num_workers=os.cpu_count() // 2, pin_memory=True
    )
    test_loader = torch.utils.data.DataLoader(
        test_dataset, batch_size=batch_size, shuffle=True,
        num_workers=os.cpu_count() // 2, pin_memory=True
    )

    return train_loader, valid_loader, test_loader


def model_select(model: str, num_classes: int):
    net = ()
    if model == "vgg":
        net = torchvision.models.vgg16_bn(weights=torchvision.models.VGG16_BN_Weights)
        print(net)

        # temp = net.classifier
        # net.classifier = nn.Sequential()
        # net.load_state_dict(torch.load(model_path))
        # net.classifier = temp

        for param in net.parameters():
            param.requires_grad = False

        for param in net.classifier.parameters():
            param.requires_grad = True

        # Replace output layer for multi-class classification
        net.classifier[6] = nn.Sequential(
            nn.Linear(
                in_features=net.classifier[6].in_features,
                out_features=num_classes,
                bias=True
            ),
            nn.Softmax()
            # nn.Sigmoid()
        )

    if model == "inception":
        net = torchvision.models.inception_v3(
            weights=torchvision.models.Inception_V3_Weights
        )
        print(net)

        # temp = net.fc
        # net.fc = nn.Sequential()
        # net.load_state_dict(torch.load(model_path))
        # net.fc = temp

        for param in net.parameters():
            param.requires_grad = False

        for param in net.fc.parameters():
            param.requires_grad = True

        net.fc = (nn.Sequential(
            nn.Linear(
                in_features=net.fc.in_features,
                out_features=num_classes,
                bias=True
            ),
            nn.Softmax()
        ))

    if model == "efficientNet":
        net = torchvision.models.efficientnet_v2_s(
            weights=torchvision.models.EfficientNet_V2_S_Weights
        )
        print(net)

        # temp = net.classifier
        # net.classifier = nn.Sequential()
        # net.load_state_dict(torch.load(model_path))
        # net.classifier = temp

        for param in net.parameters():
            param.requires_grad = False

        for param in net.classifier.parameters():
            param.requires_grad = True

        net.classifier[1] = nn.Sequential(
            nn.Linear(
                in_features=net.classifier[1].in_features,
                out_features=num_classes,
                bias=True
            ),
            nn.Softmax()
        )

    if model == "mobileViT":
        net = mobilevit_xxs()
        print(net)

    return net


def eva_index(device, num_classes: int):
    f1 = F1Score(task="binary", num_classes=num_classes).to(device)
    recall = Recall(task="binary", average='macro', num_classes=num_classes).to(device)
    precision = Precision(task="binary", average='macro', num_classes=num_classes).to(device)

    return f1, recall, precision
