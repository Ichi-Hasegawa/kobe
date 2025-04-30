#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import warnings
from pathlib import Path

import numpy as np
import pandas as pd
import torch.utils.data
import torchvision.transforms
import cv2
import os
import nibabel as nib

from skimage.io import imread, imsave
from skimage.color import rgb2gray, gray2rgb

from utils.dataset import Dataset, AnnotatedData


class XpDataset(torch.utils.data.Dataset):
    def __init__(self, data: [AnnotatedData], label: str, transform: torchvision.transforms, synth_dirs: list[Path] = None):
        super(XpDataset, self).__init__()
        self.__data = []
        for d in data:
            self.__data.append(d)  # 実画像
            if synth_dirs is not None:
                subject_str = f"{d.subject_number:03}"
                for synth_root in synth_dirs:
                    matches = sorted(Path(synth_root).rglob(f"{subject_str}_*.nii.gz"))
                    for path in matches:
                        synth_data = AnnotatedData(
                            root=d._DataABC__root,
                            annotation=d._DataABC__annotation,
                        )
                        synth_data._synth_path_override = path
                        self.__data.append(synth_data)

        self.__label = label
        self.__transform = transform
        print(f"Loaded {len(data)} real + {len(self.__data) - len(data)} synthetic = {len(self.__data)} samples")


    def __len__(self) -> int:
        return len(self.__data)

    @property
    def c(self) -> int:
        return self.__data[0].label(self.__label).c

    def __getitem__(self, item: int) -> (torch.Tensor, torch.Tensor):
        label = self.__data[item].label(self.__label).value1hot
        label = torch.tensor(label, dtype=torch.float32)

        if hasattr(self.__data[item], '_synth_path_override'):
            img = nib.load(str(self.__data[item]._synth_path_override)).get_fdata()
            if img.ndim == 3:
                img = img[:, :, 0]
        else:
            img, _ = self.__data[item].xray

        img = img.squeeze()
        img = (img - np.mean(img)) / np.std(img)
        img = (img - img.min()) / (img.max() - img.min()) * 255

        output_dir = Path("~/data/images/").expanduser()
        if self.__data[item].label("side") == "left":
            img = img[:img.shape[0]//2, :]
        else:
            img = img[img.shape[0]//2:, :]
            img = np.flipud(img)
            #file_name = f"right_{item:04d}.jpeg"
            #save_path = os.path.join(output_dir, file_name)
            #cv2.imwrite(save_path, img.astype(np.uint8))

        img = np.stack([img, img, img], axis=0).astype(np.float32)

        with warnings.catch_warnings():
            warnings.simplefilter('ignore')
            img = self.__transform(torch.from_numpy(img))

        return img, label

def load_dataset(
        root: Path,
        annotation: Path,
        split: Path,
        synth_dirs: list[Path] = None
) -> ([AnnotatedData], [AnnotatedData], [AnnotatedData]):
    annotation = pd.read_csv(annotation, index_col=0)
    split = pd.read_csv(split)

    datasets = {"train": [], "valid": [], "test": []}
    for key in datasets.keys():
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
