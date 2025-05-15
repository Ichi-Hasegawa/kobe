#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from abc import ABC, abstractmethod
from functools import cached_property
from pathlib import Path

import numpy as np
import pandas as pd
import medpy.io


class LabelABC(ABC):
    @abstractmethod
    def __init__(self, value):
        self.__value = value

    @property
    def value(self):
        return self.__value


class ClassLabel(LabelABC):
    def __init__(
            self,
            value: int,             # Numerical value
            c: int = None,          # Number of classes
            names: [str] = None     # Class names
    ):
        super().__init__(value)

        assert c is not None or names is not None, "c or names must be given."
        self.__c = c if c is not None else len(names)
        self.__names = names if names is not None else [str(i) for i in range(c)]
        assert self.__c == len(self.__names), "c == len(names) must be satisfied."

        assert 0 <= value <= self.__c, f"Given \"value\"=3 out of range: 0~{self.__c}"
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
        # path = self.__root / f"{self.subject_number:03}" / "xp.nii.gz"
        path = self.__root / f"{self.subject_number:03}" / "xp.dcm"

        return medpy.io.load(str(path))

    # @cached_property
    # def mask(self) -> (np.ndarray, medpy.io.header.Header):
    #     path = self.__root / f"{self.subject_number:03}" / "mask.nii.gz"

    #     return medpy.io.load(str(path))


class AnnotatedData(DataABC):
    def __init__(self, root: Path, annotation: pd.Series):
        super().__init__(root, annotation)
        self.__root = root

        # Init annotation accessors
        self.__side = ClassLabel(
            {"l": 0, "r": 1}[annotation["side"]], c=2, names=["left", "right"]
        )  # Excel-E
        self.__xDepth = ClassLabel(annotation["xDepth"], c=3, names=["A", "B", "C"])  # Excel-I
        self.__xSpace = ClassLabel(annotation["xSpace"], c=3, names=["Class1", "Class2", "Class3"])  # Excel-J
        self.__axis  = ClassLabel(annotation["axis"],   c=4, names=["mesial", "distal", "transverse","inversion"])  # Excel-K
        self.__RCnumber=ClassLabel(annotation["RCnumber"],c=3,names=["1","2","3"]) # Excel-P
        self.__RCtype=ClassLabel(annotation["RCtype"],c=3,names=["straight","curve","C-shaped"]) # Excel-Q
        self.__clarity=ClassLabel(annotation["clarity"],c=2,names=["clear","unclear"]) # Excel-R
        self.__risk=ClassLabel(annotation["risk"],c=3,names=["mild","moderate","severe"]) # Excel-W 
        self.__Mdistance=ClassLabel(annotation["Mdistance"],c=3,names=["over","under","contact"]) # Excel-X
        self.__Mcurve=ClassLabel(annotation["Mcurve"],c=2,names=["no","yes"]) # Excel-Y
        self.__AMposition=ClassLabel(annotation["AMposition"],c=4,names=["cheek","downward","tongue","interradicular"]) # Excel-Z
        self.__hypoesthesia=ClassLabel(annotation["hypoesthesia"],c=2,names=["negative","positive"]) # Excel-AB

    def __str__(self) -> str:
        return "\n".join([
            "AnnotatedData()",
            f"  root:           {self.__root}",
            f"  index:          {self.number}",
            f"  subject:        #{self.subject_number}",
            "  labels:",
            "      panorama:",
            f'        depth:      {self.label("xDepth")}'
            f'        space:      {self.label("xSpace")}'
            f'         axis:      {self.label("axis")}'
            f'     RCnumber:      {self.label("RCnumber")}'
            f'       RCtype:      {self.label("RCtype")}'
            f'      clarity:      {self.label("clarity")}'
            f'         risk:      {self.label("risk")}'
            f'    Mdistance:      {self.label("Mdistance")}'
            f'       Mcurve:      {self.label("Mcurve")}'
            f'   AMposition:      {self.label("AMposition")}'
            f' hypoesthesia:      {self.label("hypoesthesia")}'
        ])

    @property
    def number(self) -> int:
        return int(self.__annotation.name)

    def label(self, name: str) -> LabelABC:
        return getattr(self, f"{name}")

    @property
    def side(self) -> ClassLabel:
        return self.__side

    @property
    def xDepth(self) -> ClassLabel:
        return self.__xDepth

    @property
    def xSpace(self) -> ClassLabel:
        return self.__xSpace

    @property
    def axis(self) -> ClassLabel:
        return self.__axis
    
    @property
    def RCnumber(self) -> ClassLabel:
        return self.__RCnumber
    
    @property
    def RCtype(self) -> ClassLabel:
        return self.__RCtype
    
    @property
    def clarity(self) -> ClassLabel:
        return self.__clarity
    
    @property
    def risk(self) -> ClassLabel:
        return self.__risk
    
    @property
    def Mdistance(self) -> ClassLabel:
        return self.__Mdistance
    
    @property
    def Mcurve(self) -> ClassLabel:
        return self.__Mcurve
    
    @property
    def AMposition(self) -> ClassLabel:
        return self.__AMposition

    @property
    def hypoesthesia(self) -> ClassLabel:
        return self.__hypoesthesia

class Subject(DataABC):
    def __init__(self, root: Path, annotation: pd.DataFrame):
        super().__init__(root, annotation)
        self.__root = root
        self.__clin = annotation

    def __str__(self) -> str:
        return "\n".join([
            "Subject()",
            f"  {self.__root}",
            f"  {self.__clin}",
        ])

    def get_annotated_data(self, side: str) -> AnnotatedData:
        """

        :param side:    l:left or r:right
        :return:
        """
        data_annotation = self.__clin[
            self.__clin["side"].isin([side])
        ]

        if data_annotation.empty:
            raise ValueError("Subject-#{} does not have {}-side annotation.".format(
                self.__clin["subject"].iloc[0],
                "right" if side == "r" else "left"
            ))

        return AnnotatedData(
            root=self.__root,
            annotation=data_annotation.iloc[0]  # [0] to extract pd.Series from pd.DataFrame
        )

    @cached_property
    def left(self) -> AnnotatedData:
        return self.get_annotated_data("l")

    @cached_property
    def right(self) -> AnnotatedData:
        return self.get_annotated_data("r")

    @property
    def data(self) -> AnnotatedData:
        return AnnotatedData(
            root=self.__root,
            annotation=self.__clin.iloc[0]
        )


class Dataset(object):
    def __init__(self, root: Path, annotation: pd.DataFrame):
        self.__root = root
        self.__annotation = annotation

        #print(self.__annotation)

    @property
    def root(self) -> Path:
        return self.__root

    def get_subject(self, subject: int) -> Subject:
        # Filter subject line
        subject_annotation = self.__annotation[
            self.__annotation["subject"].isin([subject])
        ]

        return Subject(root=self.__root, annotation=subject_annotation)

    def get_data(self, number: int) -> AnnotatedData:
        return AnnotatedData(
            root=self.__root,
            annotation=self.__annotation.iloc[number]
        )

    #def list_subject(self) -> [int]:
    #    return self.__annotation["subject"].unique()

    def list_subject(self) -> [int]:
        return self.__annotation["subject"]

    def iter_subject(self):
        for i in self.list_subject():
            yield self.get_subject(i)

    def list_data(self):
        return self.__annotation.index

    def iter_data(self):
        for i in self.list_data():
            yield self.get_data(i)


# Test codes
if __name__ == '__main__':
    def main():
        annotation = pd.read_csv(
            Path("~/workspace/kobe-mCanal/_data/list-231026.csv").expanduser(),
            index_col=0
        )
        dataset = Dataset(
            root=Path("/net/nfs3/export/dataset/morita/kobe-u/oral/MandibularCanal/"),
            annotation=annotation
        )
        # subject = dataset.get_subject(3)
        # data = subject.left
        # print(data)
        # img, _ = data.xray
        # print(img.shape)
        #
        # print(dataset.list_data())
        # dataset.iter_data()

        for subject in dataset.iter_subject():
            print(f"Subject-#{subject.data.subject_number}")
            # print(subject.data.number)
            print(f"    Shape: {subject.data.xray[0].shape}")


    main()