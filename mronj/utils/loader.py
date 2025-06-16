#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from typing import Tuple
import warnings
from pathlib import Path
import random

import numpy as np
import pandas as pd
import torch.utils.data
import torchvision.transforms
import medpy.io


class XpDataset(torch.utils.data.Dataset):
    def __init__(
            self,
            data: [(str, str, (int, int, int, int))],
            grid: Tuple[int, int],
            transform: torchvision.transforms

    ):
        super(XpDataset, self).__init__()

        self.__grid = grid
        self.__transform = transform

        self._data = [
            (
                img_path, msk_path, pos, roi,
                self.mask2class(msk_path=msk_path, pos=pos, roi=roi)    # class
            )
            for img_path, msk_path, pos in data
            for roi in range(grid[0] * grid[1])
        ]

    @property
    def grid(self):
        return self.__grid

    def __len__(self) -> int:
        return len(self._data)

    def __getitem__(self, item: int):
        img_path, msk_path, pos, roi, c = self._data[item]
        # x, y, _, _ = pos
        xd, yd, wd, hd = self.convert_xywh(pos, roi)

        with warnings.catch_warnings():
            # Suppress warning;
            #   WARNING: In /tmp/SimpleITK-build/ITK/Modules/IO/GDCM/src/itkGDCMImageIO.cxx, line 359
            #   GDCMImageIO (0xba3abb0): Converting from MONOCHROME1 to MONOCHROME2 may impact the meaning
            #   of DICOM attributes related to pixel values.
            warnings.simplefilter('ignore')

            img, _ = medpy.io.load(img_path)
            # msk, _ = medpy.io.load(msk_path)

        img = img[xd: xd+wd, yd: yd+hd]
        img = np.stack([img, img, img], axis=2).squeeze()
        img = self.__transform(img)

        label = torch.eye(2)[c]

        return img, label, (xd, yd, wd, hd)

    def convert_xywh(
            self, pos: (int, int, int, int), roi: int
    ) -> (int, int, int, int):
        gx, gy = self.grid
        x, y, xx, yy = pos
        wd, hd = (xx - x) // gx, (yy - y) // gy
        xd = x + (roi // 2 * wd)
        yd = y + (roi % 2 * hd)

        return xd, yd, wd, hd

    def mask2class(self, msk_path: str, pos: (int, int, int, int), roi: int) -> int:
        with warnings.catch_warnings():
            # Suppress warning;
            #   WARNING: In /tmp/SimpleITK-build/ITK/Modules/IO/GDCM/src/itkGDCMImageIO.cxx, line 359
            #   GDCMImageIO (0xba3abb0): Converting from MONOCHROME1 to MONOCHROME2 may impact the meaning
            #   of DICOM attributes related to pixel values.
            warnings.simplefilter('ignore')

            msk, _ = medpy.io.load(msk_path)

        xd, yd, wd, hd = self.convert_xywh(pos, roi)

        return 1 if msk[xd: xd+wd, yd: yd+hd].sum() > 1 else 0


class XpDataset4SCL(XpDataset):
    def __init__(
            self,
            data: [(str, str, (int, int, int, int))],
            grid: Tuple[int, int],
            transform: torchvision.transforms
    ):
        super(XpDataset4SCL, self).__init__(
            data=data,
            grid=grid,
            transform=transform
        )

        data0, data1 = [], []
        for img_path, msk_path, pos, roi, c in self._data:
            if c == 1:
                data1.append((img_path, msk_path, pos, roi, c))
            else:
                data0.append((img_path, msk_path, pos, roi, c))

        self._data = data0 + random.choices(data1, k=len(data0))


def load_dataset(path: Path):
    return [
        (row["image"], row["mask"], (row["x"], row["y"], row["xx"], row["yy"]))
        for _, row in pd.read_csv(path).iterrows()
    ]


def load_fold(data: Path, splits: Path, part: str):
    data = pd.read_csv(data).values.tolist()
    splits = pd.read_csv(splits).values.tolist()
    subjects, datasets = {}, {}

    if part == "part1":
        folds = {"train": [0, 1], "valid": [2], "test": [3]}
    elif part == "part2":
        folds = {"train": [1, 2], "valid": [3], "test": [0]}
    elif part == "part3":
        folds = {"train": [2, 3], "valid": [0], "test": [1]}
    else:
        folds = {"train": [3, 0], "valid": [1], "test": [2]}

    for key, fold in folds.items():
        subjects[key] = []
        for group in fold:
            for subject, n in splits:
                if n == group:
                    subjects[key].append(subject)

    for key, patients in subjects.items():
        datasets[key] = []
        for patient in patients:
            for p, image, msk, x, y, xx, yy in data:
                if p == patient:
                    datasets[key].append((image, msk, (x, y, xx, yy)))

    return datasets["train"], datasets["valid"], datasets["test"]

