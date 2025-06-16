#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from pathlib import Path
import medpy.io
import numpy as np
from PIL import Image
import pandas as pd
import matplotlib.pyplot as plt


def graph_plot():
    name = "048"
    path = Path(f"/net/nfs3/export/dataset/morita/kobe-u/oral/mronj/{name}/xp.nii.gz")
    img, _ = medpy.io.load(str(path))
    img = (img - img.min()) / (img.max() - img.min()) * 255
    v2, i2 = [], []

    for i, v in enumerate(img.sum(axis=1)):
        print(v[0])
        i2.append(i)
        v2.append(v[0])
    plt.plot(
        i2, v2, linestyle="solid", linewidth=2.0
    )
    plt.xlabel("y")
    plt.ylabel("sum.pixel")
    plt.tick_params(direction='in')

    plt.savefig(f"Xgraph-{name}.png")
    plt.show()


def get_pixel():
    path = Path("/net/nfs3/export/dataset/morita/kobe-u/oral/mronj/004/xp.nii.gz")
    img, _ = medpy.io.load(str(path))
    img = (img - img.min()) / (img.max() - img.min()) * 255

    y, y2, y3 = [], [], []
    t, t2, t3 = [], [], []

    # yoko
    for i, v in enumerate(img.sum(axis=1)):
        print(v[0])
        y.append(v[0])
        y2.append((i, v[0]))
    # print(y2)
    for i2, v2 in sorted(y2, reverse=True):
        y3.append((i2, v2))
    # print(y3)


def load_dataset(path: Path):
    return [
        (row["image"], row["mask"])
        for _, row in pd.read_csv(path).iterrows()
    ]


def crop_image():
    df = Path("~/data/_out/mronj/extra_img").expanduser()
    df.mkdir(parents=True, exist_ok=True)
    root = Path("/_data/valid.csv")
    dataset = load_dataset(path=root)

    # for count, (path, msk_path) in enumerate(dataset):
    path = "/net/nfs3/export/dataset/morita/kobe-u/oral/mronj/063/xp.nii.gz"
    img, _ = medpy.io.load(str(path))
    x, x2, x3 = [], [], []
    y, y2, y3 = [], [], []
    img = (img - img.min()) / (img.max() - img.min()) * 255

    # yoko
    for i, v in enumerate(img.sum(axis=1)):
        x.append((i, v[0]))

    for i2, v2 in sorted(x, reverse=False):
        if i2 == img.shape[0]//3:
            break
        x2.append((v2, i2))

    for i2, v2 in sorted(x, reverse=True):
        if i2 == (img.shape[0]-img.shape[0]//3):
            break
        x3.append((v2, i2))

    # tate
    for i, v in enumerate(img.sum(axis=0)):
        y2.append((i, v[0]))

    for i2, v2 in sorted(y2, reverse=False):
        if i2 == img.shape[1]//3:
            break
        y3.append((v2, i2))

    print(min(x2)[1], ",", min(y3)[1], ",", min(x3)[1], ",", img.shape[1])

    img = img[min(x2)[1]: min(x3)[1], min(y3)[1]:]
    img = np.stack([img, img, img], axis=2).squeeze()
    img = Image.fromarray(
        img.astype(np.uint8)
    ).transpose(method=Image.Transpose.TRANSPOSE)
    # img.save(df/f"{count}.png")
    img.save("test_063.png")

    # print(
    #     # path, ",", msk_path, ",",
    #     min(x2)[1], ",", min(y3)[1], ",",
    #     min(x3)[1], ",", img.shape[1],
    #     sep=''
    # )


if __name__ == '__main__':
    crop_image()
