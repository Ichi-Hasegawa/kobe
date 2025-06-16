#!/usr/bin/env python3
# -*- coding: utf-8 -*-


import torch
import numpy as np
import torch.nn as nn
import torchvision.models as models


class EarlyStopping:
    def __init__(
            self,
            patience: int,
            verbose=False,
            delta=0,
            path='checkpoint.pth',
            trace_func=print
    ):

        self.patience = patience
        self.verbose = verbose
        self.counter = 0
        self.best_score = None
        self.early_stop = False
        self.val_loss_min = np.Inf
        self.delta = delta
        self.path = path
        self.trace_func = trace_func

    def __call__(self, val_loss, model):

        score = -val_loss

        if self.best_score is None:
            self.best_score = score
            self.save_checkpoint(val_loss, model)
        elif score < self.best_score + self.delta:
            self.counter += 1
            self.trace_func(f'EarlyStopping counter: {self.counter} out of {self.patience}')
            if self.counter >= self.patience:
                self.early_stop = True
        else:
            self.best_score = score
            self.save_checkpoint(val_loss, model)
            self.counter = 0

    def save_checkpoint(self, val_loss, model):
        if self.verbose:
            self.trace_func(f'Validation loss decreased ({self.val_loss_min:.6f} --> {val_loss:.6f}).  Saving model ...')
        torch.save(model.state_dict(), self.path)
        self.val_loss_min = val_loss


class VggBackbone(nn.Module):
    def __init__(self, base_model: str, out_dim: int):
        super(VggBackbone, self).__init__()

        self.backbone = self.get_basemodel(self, base_model, num_classes=out_dim)
        self.backbone.classifier = nn.Sequential()

    @property
    def base_model(self) -> str:
        return self.__base_model

    @staticmethod
    def get_basemodel(
            self,
            model_name: str,
            num_classes: int
    ):
        backbones = {
            # "resnet18": models.resnet18(weights=None, num_classes=num_classes),
            "vgg16": models.vgg16_bn(weights=models.VGG16_BN_Weights)
        }

        try:
            model = backbones[model_name]
        except KeyError:
            raise Exception(
                "Invalid backbone architecture. Check the config file and pass one of: resnet18 or resnet50"
            )
        else:
            return model
