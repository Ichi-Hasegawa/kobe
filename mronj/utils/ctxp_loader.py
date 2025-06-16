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
from skimage.transform import resize


class XpDataset(torch.utils.data.Dataset):
    def __init__(
        self,
        data: [(str, str, (int, int, int, int), (int, int, int, int))],
        grid: Tuple[int, int],
        transform: torchvision.transforms.Compose
    ):
        super(XpDataset, self).__init__()

        self.__grid = grid
        self.__transform = transform

        self._data = [
            (
                img_path, msk_path, pos_img, pos_msk, roi,
                self.mask2class(msk_path=msk_path, pos=pos_msk, roi=roi)
            )
            for img_path, msk_path, pos_img, pos_msk in data
            for roi in range(grid[0] * grid[1])
        ]
        # self._loaded_images = set()

    @property
    def grid(self):
        return self.__grid

    def __len__(self):
        return len(self._data)

    def __getitem__(self, item: int):
        img_path, msk_path, pos_img, pos_msk, roi, c = self._data[item]
        xi, yi, wi, hi = self.convert_xywh(pos_img, roi)
        xm, ym, wm, hm = self.convert_xywh(pos_msk, roi)

        with warnings.catch_warnings():
            warnings.simplefilter('ignore')
            img, _ = medpy.io.load(img_path)
            msk, _ = medpy.io.load(msk_path)
        
        # if img_path not in self._loaded_images:
        #     self._loaded_images.add(img_path)
        #     print(f"[LOAD] image: {img_path}")

        img = img[xi: xi+wi, yi: yi+hi]
        msk_crop = msk[xm: xm+wm, ym: ym+hm]

        # マスクサイズに画像を合わせる（リサイズ）
        h_msk, w_msk = msk_crop.shape
        img = resize(img, (h_msk, w_msk), preserve_range=True)
        if img.size == 0 or msk_crop.size == 0:
            print(f"[ERROR] Empty image or mask crop at {img_path}, roi={roi}, pos_img={pos_img}, pos_msk={pos_msk}")
            raise ValueError("Invalid crop result")

        if np.any(np.array(img.shape) == 0) or np.any(np.array(msk_crop.shape) == 0):
            print(f"[ERROR] Zero dimension after crop. img: {img.shape}, msk: {msk_crop.shape}")

        img = np.stack([img, img, img], axis=2).squeeze()
        img = self.__transform(img)

        label = torch.eye(2)[c]
        return img, label, (xi, yi, wi, hi)

    def convert_xywh(
        self, pos: Tuple[int, int, int, int], roi: int
    ) -> Tuple[int, int, int, int]:
        gx, gy = self.__grid
        x, y, xx, yy = pos
        assert xx > x and yy > y, f"[convert_xywh] Invalid ROI: {pos}"
        wd, hd = (xx - x) // gx, (yy - y) // gy
        xd = x + (roi // gy) * wd
        yd = y + (roi % gy) * hd
        return xd, yd, wd, hd

    def mask2class(self, msk_path: str, pos: Tuple[int, int, int, int], roi: int) -> int:
        with warnings.catch_warnings():
            warnings.simplefilter('ignore')
            msk, _ = medpy.io.load(msk_path)
        xd, yd, wd, hd = self.convert_xywh(pos, roi)
        return 1 if msk[xd: xd+wd, yd: yd+hd].sum() > 1 else 0


class XpDataset4SCL(XpDataset):
    def __init__(
        self,
        data: [(str, str, (int, int, int, int), (int, int, int, int))],
        grid: Tuple[int, int],
        transform: torchvision.transforms.Compose
    ):
        super().__init__(data=data, grid=grid, transform=transform)

        data0, data1 = [], []
        for img_path, msk_path, pos_img, pos_msk, roi, c in self._data:
            if c == 1:
                data1.append((img_path, msk_path, pos_img, pos_msk, roi, c))
            else:
                data0.append((img_path, msk_path, pos_img, pos_msk, roi, c))

        self._data = data0 + random.choices(data1, k=len(data0))


def load_dataset(path: Path):
    df = pd.read_csv(path)
    return [
        (
            row["image"], row["mask"],
            (row["x_img"], row["y_img"], row["xx_img"], row["yy_img"]),
            (row["x_msk"], row["y_msk"], row["xx_msk"], row["yy_msk"])
        )
        for _, row in df.iterrows()
    ]


def load_fold(data: Path, splits: Path, part: str):
    df_data = pd.read_csv(data)
    df_splits = pd.read_csv(splits)
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
            for patient, n in df_splits.values.tolist():
                if n == group:
                    subjects[key].append(str(patient))  

    for key, patients in subjects.items():
        datasets[key] = []
        for _, row in df_data.iterrows():
            if str(row["patient"]) in patients:
                datasets[key].append((
                    row["image"], row["mask"],
                    (row["x_img"], row["y_img"], row["xx_img"], row["yy_img"]),
                    (row["x_msk"], row["y_msk"], row["xx_msk"], row["yy_msk"])
                ))
        if key == "train":
            unique_imgs = set(x[0] for x in datasets[key])
            synth_imgs = [p for p in unique_imgs if "Panorama" in p]
            real_imgs = [p for p in unique_imgs if "Panorama" not in p]
            print(f"[DEBUG] train画像数: total={len(unique_imgs)}, real={len(real_imgs)}, synthetic={len(synth_imgs)}")

    return datasets["train"], datasets["valid"], datasets["test"]

