# Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.

"""Pretrain Llama"""

import torch
from functools import partial
from megatron import get_args
from megatron import print_rank_0
from megatron import get_timers
from megatron import get_tokenizer
from megatron.core import tensor_parallel
from megatron.core.enums import ModelType
from megatron.data.sft_dataset import build_train_valid_test_datasets
from megatron.model import LlamaModel
from megatron.training import pretrain
from megatron.utils import get_ltor_masks_and_position_ids
from megatron.utils import average_losses_across_data_parallel_group
from collections import OrderedDict
import numpy as np
import os, sys
from megatron.core import mpu
from mlflow.ksmlflow_runner import mlflowRunner, mlflowAsyncRunner, AsyncType
import os
from pathlib import Path
import json
import shutil


def model_provider(pre_process=True, post_process=True):
    """Build the model."""

    print_rank_0('building Llama model ...')
    model = LlamaModel(
        num_tokentypes=0,
        parallel_output=True,
        pre_process=pre_process,
        post_process=post_process
    )
    return model


def get_batch(data_iterator):
    """Generate a batch"""
    args = get_args()
    tokenizer = get_tokenizer()

    # Items and their type.

    keys = ['input_ids', 'label_mask']
    datatype = torch.int64

    # Broadcast data.
    if data_iterator is not None:
        data = next(data_iterator)
    else:
        data = None
    data_b = tensor_parallel.broadcast_data(keys, data, datatype)

    # Unpack.
    if args.sft_padding or args.micro_batch_size > 1 or (
            args.sequence_parallel and mpu.get_tensor_model_parallel_world_size() > 1):
        tokens = data_b['input_ids'][:, :].contiguous()
        labels = torch.cat([tokens[:, 1:], torch.ones_like(tokens[:, 0].unsqueeze(1))], dim=-1).contiguous()
        attention_mask = tokens.ne(tokenizer.pad_token_id)
        label_mask = data_b['label_mask']
        loss_mask = torch.cat([label_mask[:, 1:], torch.zeros_like(label_mask[:, 0].unsqueeze(1))], dim=-1)
    else:
        tokens = data_b['input_ids'][:, :-1].contiguous()
        labels = data_b['input_ids'][:, 1:].contiguous()
        attention_mask = None
        loss_mask = data_b['label_mask'][:, 1:]
        # print(f"{mpu.get_pipeline_model_parallel_rank()}/{mpu.get_pipeline_model_parallel_world_size()-1} tokens shape: {tokens.shape} labels shape: {labels.shape}")

    position_ids = torch.arange(tokens.shape[-1], dtype=torch.long,
                                device=tokens.device)

    return tokens, labels, loss_mask, attention_mask, position_ids


def loss_func(loss_mask, output_tensor):
    losses = output_tensor.float()
    loss_mask = loss_mask.view(-1).float()

    de_generation_loss = -1 * torch.log(1 - torch.exp(-1 * losses) + 1e-10)
    # 只有loss 等于-1的地方加这个loss，其他不加这个
    de_generation_loss[loss_mask.view(*de_generation_loss.size()) != -1] = 0
    if de_generation_loss.sum() > 0:
        print(de_generation_loss.sum() / ((loss_mask.view(*de_generation_loss.size()) == -1).sum()))

    # losses[loss_mask.view(*losses.size()) != 1] = 0
    losses = losses * (loss_mask.view(*losses.size()) == 1)
    # de_generation_loss = de_generation_loss * (1 - ((de_generation_loss < 0.05) | (de_generation_loss > 0.8)).float())
    de_generation_loss[
        (de_generation_loss < 0.05) | (de_generation_loss > 0.8)] = 0  # 打完之后，值很低 # 这个阈值看起来还是要调低，到1的时候，截断错误会少一些？
    if de_generation_loss.sum() > 0:
        print('domasked', de_generation_loss.sum() / (de_generation_loss != 0).sum())

    losses = losses + de_generation_loss

    if loss_mask.abs().sum() == 0:
        loss = torch.sum(losses)
    else:
        loss = torch.sum(losses) / loss_mask.abs().sum()

    # Reduce loss for logging.
    averaged_loss = average_losses_across_data_parallel_group([loss])

    return loss, {'lm loss': averaged_loss[0]}


def forward_step(data_iterator, model):
    """Forward step."""
    timers = get_timers()

    # Get the batch.
    timers('batch-generator', log_level=2).start()
    tokens, labels, loss_mask, attention_mask, position_ids = get_batch(
        data_iterator)
    timers('batch-generator').stop()

    output_tensor = model(tokens, position_ids, attention_mask,
                          labels=labels)
    from megatron.core import mpu
    tp_rank = mpu.get_tensor_model_parallel_rank()
    pp_rank = mpu.get_pipeline_model_parallel_rank()
    vpp_rank = mpu.get_virtual_pipeline_model_parallel_rank()

    return output_tensor, partial(loss_func, loss_mask)


def no_wd_decay_cond(param_name, param):
    """
    Defines whether the parameter requires weight decay.
    if param do not require weight decay, return 'True', otherwise return 'False'
    """
    # do not regularize biases nor Norm parameters
    if param_name.endswith(".bias") or len(param.shape) == 1:
        no_wd = True
    else:
        no_wd = False
    return no_wd


global num_steps_for_epoch1, num_steps_for_epoch2, num_steps_for_epoch3
num_steps_for_epoch1, num_steps_for_epoch2, num_steps_for_epoch3 = 0, 0, 0


def get_mlflow_data(runner=None, **kwargs):
    pass


def train_valid_test_datasets_provider():
    """Build train, valid, and test datasets."""
    args = get_args()

    print_rank_0('> building train, validation, and test datasets '
                 'for Llama ...')
    train_ds, valid_ds, test_ds = build_train_valid_test_datasets(
        data_impl=args.data_impl,
        seq_length=args.seq_length,
        micro_batch_size=args.micro_batch_size,
        skip_warmup=(not args.mmap_warmup),
        train_data_prefix=args.train_data_path,
        valid_data_prefix=args.valid_data_path,
        test_data_prefix=args.test_data_path)
    print_rank_0("> finished creating Llama datasets ...")

    return train_ds, valid_ds, test_ds


if __name__ == "__main__":
    torch.set_printoptions(precision=8)
    # 读取文件
    with open("train_config_info_for_script", "r") as file:  # "$LR\n$SEQ_LENGTH\n$TRAIN_DATA_PATH$init_mdl"
        learning_rate = float(file.readline().strip())
        max_seq_len = int(file.readline().strip())
        data_path = file.readline().strip()
        init_pretrained_model = file.readline().strip()
    # 读取文件
    with open("mlflow_config_info_for_script", "r") as file:  # "$uri_path\n$exp_name\n$run_name\n$save_dir"
        uri_path = file.readline().strip()
        exp_name = file.readline().strip()
        run_name = file.readline().strip()
        save_dir = file.readline().strip()

    log_params = {
        'model': 'llama',
        'arch': 'llama-13b',
        'framework': 'megatron',
        'dict': 128000,
        'tp': 2,
        'pp': 4,
    }

    mlflow_dict = {}
    mlflow_dict['experiment_name'] = exp_name
    mlflow_dict['run_name'] = run_name
    mlflow_dict['uri'] = uri_path
    mlflow_dict['save_dir'] = save_dir
    log_params['init_pretrained_model'] = init_pretrained_model
    log_params['data_path'] = data_path
    log_params['max_seq_len'] = max_seq_len
    log_params['learning_rate'] = learning_rate

    mlflow_dict['log_params'] = log_params

    import os

    mlflow_save_path = os.path.join(mlflow_dict['uri'], mlflow_dict['experiment_name'],
                                    mlflow_dict['experiment_name'] + '_' + mlflow_dict['run_name'], "artifacts")
    if not os.path.exists(mlflow_save_path):
        os.makedirs(mlflow_save_path, exist_ok=True)

    pretrain(train_valid_test_datasets_provider,
             model_provider,
             ModelType.encoder_or_decoder,
             forward_step,
             no_wd_decay_cond,
             get_mlflow_data,
             mlflow_dict=mlflow_dict,
             args_defaults={'tokenizer_type': 'NullTokenizerSft'})
