#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from pathlib import Path

import numpy as np
import torch
import torch.utils.data
from torch import nn
from torchvision.transforms import transforms

from utils.loader import XpDataset, load_dataset


class GaussianBlur(object):
    """blur a single image on CPU"""
    def __init__(self, kernel_size):
        radias = kernel_size // 2
        kernel_size = radias * 2 + 1
        self.blur_h = nn.Conv2d(3, 3, kernel_size=(kernel_size, 1),
                                stride=1, padding=0, bias=False, groups=3)
        self.blur_v = nn.Conv2d(3, 3, kernel_size=(1, kernel_size),
                                stride=1, padding=0, bias=False, groups=3)
        self.k = kernel_size
        self.r = radias

        self.blur = nn.Sequential(
            nn.ReflectionPad2d(radias),
            self.blur_h,
            self.blur_v
        )

        self.pil_to_tensor = transforms.ToTensor()
        self.tensor_to_pil = transforms.ToPILImage()

    def __call__(self, img):
        img = self.pil_to_tensor(img).unsqueeze(0)

        sigma = np.random.uniform(0.1, 2.0)
        x = np.arange(-self.r, self.r + 1)
        x = np.exp(-np.power(x, 2) / (2 * sigma * sigma))
        x = x / x.sum()
        x = torch.from_numpy(x).view(1, -1).repeat(3, 1)

        self.blur_h.weight.data.copy_(x.view(3, 1, self.k, 1))
        self.blur_v.weight.data.copy_(x.view(3, 1, 1, self.k))

        with torch.no_grad():
            img = self.blur(img)
            img = img.squeeze()

        img = self.tensor_to_pil(img)

        return img


def get_transform(s: int = 1):
    """
    Return a set of data augmentation transformations as described in the SimCLR paper.
    """
    color_jitter = transforms.ColorJitter(0.8 * s, 0.8 * s, 0.8 * s, 0.2 * s)
    data_transforms = transforms.Compose([
        transforms.ToTensor(),
        transforms.ConvertImageDtype(torch.float32),
        transforms.RandomHorizontalFlip(0.25),
        transforms.RandomVerticalFlip(0.25),
        transforms.RandomApply([color_jitter], p=0.8),
        transforms.RandomGrayscale(p=0.2),
        # GaussianBlur(kernel_size=int(0.1 * size)),
        transforms.Normalize(
            mean=(2047.5, 2047.5, 2047.5),
            std=(2047.5, 2047.5, 2047.5)
        ),
        # transforms.Normalize(
        #     mean=(0.5, 0.5, 0.5),
        #     std=(0.5, 0.5, 0.5)
        # ),
        # VGG, eNet
        transforms.Resize(
            (224, 224),
            transforms.InterpolationMode.BILINEAR
        ),
        # incep
        # transforms.Resize(
        #     (299, 299),
        #     transforms.InterpolationMode.BILINEAR
        # ),
    ])

    return data_transforms


class CLViewGenerator(object):
    """Take two random crops of one image as the query and key."""

    def __init__(self,
                 base_transform,
                 n_views: int
                 ):
        """

        :param base_transform:  ...
        :param n_views:         Number of views for contrastive learning training
        """
        self.base_transform = base_transform
        self.n_views = n_views

    def __call__(self, x):
        return [self.base_transform(x) for _ in range(self.n_views)]
