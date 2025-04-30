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