#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from abc import ABC, abstractmethod
from functools import cached_property
from pathlib import Path

import numpy as np
import pandas as pd
import medpy.io
import nibabel as nib
import random

class LabelABC(ABC):
    @abstractmethod
    def __init__(self, value):
        self.__value = value

    @property
    def value(self):
        return self.__value


class ClassLabel(LabelABC):
    def __init__(self, value: int, c: int = None, names: [str] = None):
        super().__init__(value)
        assert c is not None or names is not None, "c or names must be given."
        self.__c = c if c is not None else len(names)
        self.__names = names if names is not None else [str(i) for i in range(c)]
        assert self.__c == len(self.__names), "c == len(names) must be satisfied."
        assert 0 <= value <= self.__c, f"Given \"value\"={value} out of range: 0~{self.__c}"
        self.__value = value

    def __eq__(self, other: int | str) -> bool:
        if type(other) is int:
            return self.__value == other
        elif type(other) is str:
            return self.__names[self.__value] == other
        else:
            return False

    @property
    def c(self) -> int:
        return self.__c

    @cached_property
    def value1hot(self):
        return np.eye(self.__c)[self.__value]


class DataABC(ABC):
    @abstractmethod
    def __init__(self, root: Path, annotation: (pd.Series | pd.DataFrame)):
        self.__root = root
        self.__annotation = annotation

    @abstractmethod
    def __str__(self) -> str:
        return "DataABC(ABC)"

    @property
    def subject_number(self) -> int:
        return self.__annotation["subject"]

    @cached_property
    def xray(self) -> (np.ndarray, medpy.io.header.Header):
        subject_str = f"{self.subject_number:03}"
        if hasattr(self, "_synth_root") and self._synth_root is not None:
            synth_dir = Path(self._synth_root)
            candidates = sorted(synth_dir.glob(f"{subject_str}_*.nii.gz"))
            if candidates:
                chosen = random.choice(candidates)
                nii = nib.load(str(chosen)).get_fdata()
                if nii.ndim == 3:
                    nii = nii[:, :, 0]  # assume single-slice
                return nii.astype(np.float32), None

        path = self.__root / f"{subject_str}" / "xp.dcm"
        return medpy.io.load(str(path))


class AnnotatedData(DataABC):
    def __init__(self, root: Path, annotation: pd.Series, synth_root: Path = None):
        super().__init__(root, annotation)
        self.__root = root
        self._synth_root = synth_root
        self.__side = ClassLabel({"l": 0, "r": 1}[annotation["side"]], c=2, names=["left", "right"])
        self.__xDepth = ClassLabel(annotation["xDepth"], c=3, names=["A", "B", "C"])
        self.__xSpace = ClassLabel(annotation["xSpace"], c=3, names=["Class1", "Class2", "Class3"])
        self.__axis  = ClassLabel(annotation["axis"], c=4, names=["mesial", "distal", "transverse","inversion"])
        self.__RCnumber=ClassLabel(annotation["RCnumber"],c=3,names=["1","2","3"])
        self.__RCtype=ClassLabel(annotation["RCtype"],c=3,names=["straight","curve","C-shaped"])
        self.__clarity=ClassLabel(annotation["clarity"],c=2,names=["clear","unclear"])
        self.__risk=ClassLabel(annotation["risk"],c=3,names=["mild","moderate","severe"])
        self.__Mdistance=ClassLabel(annotation["Mdistance"],c=3,names=["over","under","contact"])
        self.__Mcurve=ClassLabel(annotation["Mcurve"],c=2,names=["no","yes"])
        self.__AMposition=ClassLabel(annotation["AMposition"],c=4,names=["cheek","downward","tongue","interradicular"])
        self.__hypoesthesia=ClassLabel(annotation["hypoesthesia"],c=2,names=["negative","positive"])

    def __str__(self) -> str:
        return f"AnnotatedData(subject={self.subject_number})"

    def label(self, name: str) -> LabelABC:
        return getattr(self, f"_{self.__class__.__name__}__{name}")

    @property
    def hypoesthesia(self) -> ClassLabel:
        return self.__hypoesthesia


class Dataset:
    def __init__(self, root: Path, annotation: pd.DataFrame, synth_root: Path = None):
        self.__root = root
        self.__annotation = annotation
        self._synth_root = synth_root

    def get_data(self, number: int) -> AnnotatedData:
        return AnnotatedData(
            root=self.__root,
            annotation=self.__annotation.iloc[number],
            synth_root=self._synth_root
        )

    def list_data(self):
        return self.__annotation.index

    def iter_data(self):
        for i in self.list_data():
            yield self.get_data(i)


# Test
if __name__ == '__main__':
    def main():
        annotation = pd.read_csv(
            Path("~/workspace/kobe-mCanal/_data/list-231026.csv").expanduser(),
            index_col=0
        )
        dataset = Dataset(
            root=Path("/net/nfs3/export/dataset/morita/kobe-u/oral/MandibularCanal/"),
            annotation=annotation,
            synth_root=Path("/path/to/synthetic/ctxp/mean")
        )

        for data in dataset.iter_data():
            img, _ = data.xray
            print(f"#{data.subject_number:03}: shape={img.shape}")

    main()
