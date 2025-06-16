#!/usr/bin/env python3
# -*- coding:utf-8 -*-
import itertools
import logging
import random
from pathlib import Path

import torch
import torch.nn as nn
import torch.nn.functional as F
import torchvision.models as models
from torch.cuda.amp import GradScaler, autocast
from torch.utils.tensorboard import SummaryWriter

from utils.loader import XpDataset
from utils.supconloss import SupConLoss


class SimCLR(object):
    """
    SimCLR training runner
    """

    def __init__(self, *args, **kwargs):
        self.fp16 = True
        self.device = kwargs['device']

        self.model = kwargs['model'].to(self.device)
        self.optimizer = kwargs['optimizer']
        self.scheduler = kwargs['scheduler']
        self.temperature = kwargs['temperature']

        self.mode = kwargs['mode']
        self.epochs = 0 if self.mode == 'retrain' else kwargs['epochs']
        self.batch_size = kwargs['batch_size']
        self.n_views = kwargs['n_views']

        # Logger setting
        self.writer = SummaryWriter()
        logging.basicConfig(
            filename=Path(self.writer.log_dir) / 'training.log',
            level=logging.DEBUG
        )

        self.criterion = nn.MSELoss().to(self.device)
        # self.criterion = SupConLoss().to(self.device)

    def info_nce_loss(self, features):
        """
        NCE: Noice Contrastive Estimation
        """
        print(features.shape[0])
        print(self.batch_size)
        labels = torch.cat(
            [torch.arange(features.shape[0] // self.n_views) for _ in range(self.n_views)],
            dim=0
        )
        # print(labels)
        labels = (labels.unsqueeze(0) == labels.unsqueeze(1)).float()
        labels = labels.to(self.device)
        # batch
        features = F.normalize(features, dim=1)

        similarity_matrix = torch.matmul(features, features.T)

        # Discard the main diagonal from both: labels and similarities matrix
        mask = torch.eye(labels.shape[0], dtype=torch.bool).to(self.device)
        labels = labels[~mask].view(labels.shape[0], -1)
        similarity_matrix = similarity_matrix[~mask].view(similarity_matrix.shape[0], -1)

        # select and combine multiple positives
        positives = similarity_matrix[labels.bool()].view(labels.shape[0], -1)

        # select only negatives
        negatives = similarity_matrix[~labels.bool()].view(similarity_matrix.shape[0], -1)

        y_pred = torch.cat([positives, negatives], dim=1)

        y_true = torch.cat([
            labels[labels.bool()].view(labels.shape[0], -1),
            labels[~labels.bool()].view(labels.shape[0], -1)
        ], dim=1).to(self.device)

        y_pred = y_pred / self.temperature

        return y_pred, y_true


    @staticmethod
    def get_feature(features, labels):
        label = labels.to(torch.float32)
        label = label.argmax(dim=1)

        f_pos, f_neg = [], []
        for i, label in enumerate(label):
            for j, feature in enumerate(features):
                if label == 1 and j == i:
                    f_pos.append(feature)
                elif label == 0 and j == i:
                    f_neg.append(feature)

        return f_pos, f_neg

    # def info_loss(self, features, labels):
    #     f_pos, f_neg = SimCLR.get_feature(features, labels)
    #     count = len(labels) // 4
    #
    #     pos1 = [(p1, p2) for p1, p2 in itertools.combinations(range(len(f_pos)), 2)]
    #     pos1 = random.sample(pos1, count)
    #     pos1 = [torch.dot(f_pos[p1], f_pos[p2]) for p1, p2 in pos1]
    #
    #     pos2 = [(n1, n2) for n1, n2 in itertools.combinations(range(len(f_neg)), 2)]
    #     pos2 = random.sample(pos2, count)
    #     pos2 = [torch.dot(f_neg[n1], f_neg[n2]) for n1, n2 in pos2]
    #
    #     positives = torch.tensor(pos1 + pos2)
    #     neg = [(v1, v2) for v1, v2 in itertools.product(range(len(f_pos)), range(len(f_neg)))]
    #     neg = random.sample(neg, positives.shape[0])
    #     negatives = torch.tensor([torch.dot(f_pos[v1], f_neg[v2]) for v1, v2 in neg])
    #
    #     y_pred = torch.cat([positives, negatives], dim=0)
    #     y_pred = y_pred.to(torch.float64)
    #     y_true = torch.cat([
    #         torch.ones_like(positives),
    #         torch.zeros_like(negatives)
    #         ], dim=0
    #     )
    #
    #     return y_pred, y_true

    # def info_loss(self, features: torch.Tensor, labels: torch.Tensor):
    #     # features = nn.Softmax(features, dim=1)
    #     # similarity_matrix = torch.matmul(features, features.T)
    #     similarity_matrix = F.cosine_similarity(
    #         features[None, :, :], features[:, None, :], dim=-1
    #     )
    #
    #     pos_labels = labels.argmax(dim=1).float()
    #     pos_labels = pos_labels.unsqueeze(dim=0) * pos_labels.unsqueeze(dim=1)
    #     neg_labels = labels.argmin(dim=1).float()
    #     neg_labels = neg_labels.unsqueeze(dim=0) * neg_labels.unsqueeze(dim=1)
    #
    #     mask = (pos_labels + neg_labels) > 0
    #     positives = similarity_matrix[mask]
    #     negatives = similarity_matrix[~mask]
    #
    #     y_pred = torch.cat([positives, negatives], dim=0)
    #     y_true = torch.cat([
    #         torch.ones_like(positives),
    #         torch.zeros_like(negatives)
    #         ], dim=0
    #     )
    #
    #     return similarity_matrix, mask.float()

    # def train(self, train_loader):
    #     # scaler = torch.cuda.amp.GradScaler(enabled=self.fp16)
    #     # scaler = GradScaler(enabled=self.fp16)
    #     scaler = GradScaler()
    #
    #     logging.info(f"start SimLR {self.model}-ing for {self.epochs} epochs.")
    #
    #     for epoch_counter in range(self.epochs):
    #         metrics = {'train': {'loss': .0}}
    #         for batch_counter, (images, label, _) in enumerate(train_loader):
    #             images = torch.cat(images, dim=0)
    #             images = images.to(self.device)
    #             label = label.to(self.device)
    #             self.optimizer.zero_grad()
    #
    #             # with autocast(enabled=self.fp16):
    #             print(self.device)
    #             with autocast():
    #                 features = self.model(images)
    #                 # logits, labels = self.info_nce_loss(features)
    #                 logits, labels = self.info_loss(features, label)
    #                 loss = self.criterion(logits, labels)
    #                 metrics['train']['loss'] += loss.item() / len(train_loader)
    #                 print("\r  Epoch({:6}/{:6})[{}]: loss={:.4}".format(
    #                     epoch_counter, self.epochs,
    #                     ('=' * (30 * batch_counter // len(train_loader)) + " " * 30)[:30],
    #                     loss.item()
    #                 ), end="")
    #
    #             # self.optimizer.zero_grad()
    #             scaler.scale(loss).backward()
    #             scaler.step(self.optimizer)
    #             scaler.update()
    #             self.scheduler.step()
    #
    #         print("")
    #         # Logging to tensorboard
    #         for ds_name, ds_vals in metrics.items():
    #             for key, val in ds_vals.items():
    #                 self.writer.add_scalar("train/loss", val, epoch_counter)
    #
    #     logging.info("Training has finished.")
    #
    #     # save model checkpoint
    #     checkpoint_name = 'checkpoint_{:04d}.pth'.format(self.epochs)
    #     state_dict = self.model.backbone.state_dict()
    #     # print(state_dict.keys())
    #
    #     torch.save(state_dict, Path(f"~/data/_out/mronj/10sep_{checkpoint_name}").expanduser())
    #
    #     logging.info(f"Model checkpoint and metadata has saved at {self.writer.log_dir}.")

    def train2(self, train_loader):

        logging.info(f"start SimLR {self.model}-ing for {self.epochs} epochs.")

        for epoch_counter in range(self.epochs):
            metrics = {'train': {'loss': .0}}
            for batch_counter, (images, label, _) in enumerate(train_loader):
            # for batch_counter, (images, label) in enumerate(train_loader):
                images = torch.cat(images, dim=0)
                label = torch.cat([label, label], dim=0)

                images = images.to(self.device)
                label = label.to(self.device)
                # Vgg, eNet
                features = self.model(images)
                # incep
                # _, features = self.model(images)

                logits, labels = self.info_nce_loss(features)
                # logits, labels = self.info_loss(features, label)

                loss = self.criterion(logits, labels)
                loss.backward()
                self.optimizer.step()
                self.optimizer.zero_grad(set_to_none=True)
                # self.scheduler.step()

                metrics['train']['loss'] += loss.item() / len(train_loader)
                print("\r  Epoch({:6}/{:6})[{}]: loss={:.4}".format(
                    epoch_counter, self.epochs,
                    ('=' * (30 * batch_counter // len(train_loader)) + " " * 30)[:30],
                    loss.item()
                ), end="")

            print("")
            # Logging to tensorboard
            for ds_name, ds_vals in metrics.items():
                for key, val in ds_vals.items():
                    self.writer.add_scalar("train/loss", val, epoch_counter)

            # save model checkpoint
            checkpoint_name = 'checkpoint_{:04d}'.format(self.epochs)
            checkpoint = Path(f"~/data/_out/mronj/vgg16_{checkpoint_name}").expanduser()
            checkpoint.mkdir(parents=True, exist_ok=True)
            state_dict = self.model.backbone.state_dict()
            # print(state_dict.keys())

            torch.save(
                state_dict,
                (checkpoint / f"{self.model.__class__.__name__}_{epoch_counter:04}.pth")
            )

        logging.info("Training has finished.")
        logging.info(f"Model checkpoint and metadata has saved at {self.writer.log_dir}.")


class CNNSimCLR(nn.Module):
    def __init__(self, base_model: str, out_dim: int):
        super(CNNSimCLR, self).__init__()

        self.backbone = self._get_basemodel(base_model, num_classes=out_dim)

        # VGG, eNet
        self.backbone.classifier = nn.Sequential()
        # incep
        # self.backbone.fc = nn.Sequential()

    @property
    def base_model(self) -> str:
        return self.__base_model

    @staticmethod
    def _get_basemodel(
            model_name: str,
            num_classes: int
    ):
        backbones = {
            # "resnet18": models.resnet18(weights=None, num_classes=num_classes),
            "vgg16": models.vgg16_bn(weights=models.VGG16_BN_Weights),
            "eNet": models.efficientnet_v2_s(weights=models.EfficientNet_V2_S_Weights),
            "incep": models.inception_v3(weights=models.Inception_V3_Weights)
        }

        try:
            model = backbones[model_name]
        except KeyError:
            raise Exception(
                "Invalid backbone architecture. Check the config file and pass one of: resnet18 or resnet50"
            )
        else:
            return model

    def forward(self, x):
        return self.backbone(x)


class MetricLearningLayer(nn.Module):
    def __init__(self, alpha):
        super(MetricLearningLayer, self).__init__()

        self.alpha = alpha
        # self.ml = lambda x: self.alpha * x / torch.sqrt(torch.sum(x ** 2))

    def forward(self, x):
        # return self.ml(x)
        l2 = torch.sqrt((x ** 2).sum())
        x = self.alpha * (x / l2)
        return x

