#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import datetime
import os
from pathlib import Path

import torch
import torch.nn as nn
import torch.utils.data
import torchvision.models
from collections import OrderedDict


def load_vgg16(path: Path, out_features: int) -> nn.Module:
    net = torchvision.models.vgg16()
    net.classifier[6] = nn.Sequential(
        nn.Linear(
            in_features=net.classifier[6].in_features,
            out_features=out_features,
            bias=True
        ),
        nn.Softmax()
    )
    net.load_state_dict(torch.load(path))

    return net


def load_vgg16_bn(path: Path, out_features: int) -> nn.Module:
    net = torchvision.models.vgg16_bn()
    net.classifier[6] = nn.Sequential(
        nn.Linear(
            in_features=net.classifier[6].in_features,
            out_features=out_features,
            bias=True
        ),
        nn.Sigmoid()
    )
    net.load_state_dict(torch.load(path))

    return net


def load_inception3(path: Path, out_features: int) -> nn.Module:
    net = torchvision.models.inception_v3()
    net.fc = nn.Linear(
        in_features=net.fc.in_features,
        out_features=out_features,
        bias=True
    )

    net.load_state_dict(torch.load(path))

    new_net = nn.Sequential(OrderedDict([
        ("avgpool", net.avgpool),
        ("dropout", net.dropout),
        ("fc", net.fc)
    ]))

    net.avgpool = nn.Sequential()
    net.dropout = nn.Sequential()
    net.fc = nn.Sequential()

    return new_net, net


def load_efficientnet(path: Path, out_features: int) -> nn.Module:
    net = torchvision.models.efficientnet_v2_l()
    net.classifier[1] = nn.Sequential(
        nn.Linear(
            in_features=net.classifier[1].in_features,
            out_features=out_features,
            bias=True
        )
    )
    net.load_state_dict(torch.load(path))

    return net
