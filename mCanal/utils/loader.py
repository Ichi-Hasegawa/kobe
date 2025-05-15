#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import warnings
from pathlib import Path

import numpy as np
import pandas as pd
import torch.utils.data
# import torchvision
import torchvision.transforms
import cv2
import os

from skimage.io import imread, imsave
from skimage.color import rgb2gray, gray2rgb

from utils.dataset import Dataset, AnnotatedData


class XpDataset(torch.utils.data.Dataset):
    def __init__(
            self,
            data: [AnnotatedData],
            label: str,
            transform: torchvision.transforms
    ):
        super(XpDataset, self).__init__()

        self.__data = data
        self.__label = label            # Label name to be used
        self.__transform = transform

    def __len__(self) -> int:
        return len(self.__data)

    @property
    def c(self) -> int:
        # Return class number of currently selected label
        return self.__data[0].label(self.__label).c

    def __getitem__(self, item: int) -> (torch.Tensor, torch.Tensor):
        """
        :param item:    Index of item
        :return:        Return tuple of (image, label)
                        Label is always "10" <= MetricLearning
        """
        # if item > len(self):
        #     item %= len(self)

        # Get 1-hot label
        label = self.__data[item].label(self.__label).value1hot
        # Convert to torch tensor
        label = torch.tensor(label, dtype=torch.float32)

        # Get image
        img, _ = self.__data[item].xray
        # img, _ = self.__data[item].mask
        img = img.squeeze()     # Remove 1-channel-dim: (w, h, 1) -> (w, h)
        # Apply z-score normalization
        img = (img - np.mean(img)) / np.std(img)
        img = (img - img.min()) / (img.max() - img.min()) * 255
        #img = img.T
        
        output_dir = Path("~/data/images/").expanduser()
        # Use left or right half
        if self.__data[item].label("side") == "left":
            # 画像のRが右側だった場合
            #img = img[img.shape[0]//2:, :]

            # 今まで通りでよかった場合
            img = img[:img.shape[0]//2, :]

            #画像出力
            #file_name = f"left_{item:04d}.jpeg"
            #save_path = os.path.join(output_dir, file_name)
            #cv2.imwrite(save_path, img.astype(np.uint8))
        else:
            # 画像のRが右側だった場合
            #img = img[:img.shape[0]//2, :]

            # 今までどおりで良かった場合
            img = img[img.shape[0]//2:, :]

            img = np.flipud(img)
            
            #画像出力
            #file_name = f"right_{item:04d}.jpeg"
            #save_path = os.path.join(output_dir, file_name)
            #cv2.imwrite(save_path, img.astype(np.uint8))

        # Stack to convert gray->RGB
        img = np.stack([img, img, img], axis=0).astype(np.float32)
        # img = np.stack([mask, img, img], axis=0).astype(np.float32)

        # Apply image transform
        with warnings.catch_warnings():
            # Suppress warning;
            #   UserWarning: The default value of the antialias parameter of all
            #   the resizing transforms (Resize(), RandomResizedCrop(), etc.)
            warnings.simplefilter('ignore')
            img = self.__transform(torch.from_numpy(img))

        return img, label


def load_dataset(
        root: Path,         # Path to dataset root
        annotation: Path,   # Path to annotation csv
        split: Path         # Path to train/valid/test split csv
) -> ([AnnotatedData], [AnnotatedData], [AnnotatedData]):
    # Read annotation csv
    annotation = pd.read_csv(annotation, index_col=0)

    # Read split csv
    split = pd.read_csv(split)

    # Split annotation to train/valid/test

    datasets = {"train": [], "valid": [], "test": []}
    for key in datasets.keys():
        # Extract target dataset from full list
        ds_ann = annotation[annotation['subject'].isin(
            split[split['dataset'] == key]['subject']
        )]

        dataset = Dataset(root, ds_ann)

        for subject in dataset.iter_subject():
            try:
                datasets[key].append(subject.left)
            except ValueError:
                pass
            try:
                datasets[key].append(subject.right)
            except ValueError:
                pass

    return datasets['train'], datasets['valid'], datasets['test']