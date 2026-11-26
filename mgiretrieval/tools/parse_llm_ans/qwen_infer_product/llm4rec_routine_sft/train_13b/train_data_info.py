#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Created by MaxFu
from argparse import ArgumentParser
import pickle


def __cmd():
    """命令行参数."""
    parser = ArgumentParser(description="")
    parser.add_argument("data_dir", type=str, help="训练数据路径.")
    parser.add_argument("num_epoch", type=int, help="epoch数量.")
    parser.add_argument("global_bsz", type=int, help="global batch sizs.")
    args = parser.parse_args()

    # with open(args.data_dir, 'rb') as f:
    #     data_dict = pickle.load(f)
    # num_samples = len(data_dict['input_ids'])  # 样本数
    num_samples = int(len(open(args.data_dir, 'rb').read()) / 8)
    # print('num_samples', num_samples)
    total_samples = num_samples * args.num_epoch  # 样本数 * epoch数
    total_step = total_samples // args.global_bsz  # total steps, 样本数 * epoch数 除以 global batch size向下取整

    print(total_step)


if __name__ == "__main__":
    __cmd()
