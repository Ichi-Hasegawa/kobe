#!/usr/bin/env python3
# -*- coding: utf-8 -*-

def net_train(device, net, criterion, optimizer, num, ):
    x, y_true = x.to(device), y_true.to(device)
    y_pred = net(x).to()

    y_pred = y_pred.to(torch.float64)
    y_true = y_true.to(torch.float64)

    loss = criterion(y_pred, y_true)
    loss.backward()
    optimizer.step()
    optimizer.zero_grad(set_to_none=True)

    y_pred = y_pred.argmax(dim=1)
    y_true = y_true.argmax(dim=1)

    # print(f"{epoch}-{batch}: pred", y_pred.argmax(dim=1).to('cpu').detach().numpy())
    # print(f"{epoch}-{batch}: true", y_true.argmax(dim=1).to('cpu').detach().numpy())

    metrics['train']['loss'] += loss.item() / len(train_loader)
    metrics['train']['f1'] += f1(y_pred, y_true).item() / len(train_loader)
    metrics['train']['recall'] += recall(y_pred, y_true).item() / len(train_loader)
    metrics['train']['precision'] += precision(y_pred, y_true).item() / len(train_loader)

#     print("f1=", f1(y_pred, y_true).item())
#     print("recall=", recall(y_pred, y_true).item())
#     print("precision=", precision(y_pred, y_true).item())
#
#     print("\r Batch({:6}/{:6})[{}]: loss={:.4}".format(
#         batch, len(train_loader),
#         ('=' * (30 * batch // len(train_loader)) + " " * 30)[:30],
#         loss.item(),
#         ", ".join([
#             f'{key}={metrics["train"][key]:.2}'
#             for key in ['f1', 'recall', 'precision']
#         ])
#     ), end="")
# print('')