import tqdm
import pickle
import json
import logging
import torch
from multiprocessing import Pool
import argparse
import transformers
import numpy as np
import time
import os, sys

logging.basicConfig(format='%(asctime)s - %(levelname)s - %(name)s - %(message)s',
                    datefmt='%m/%d/%Y %H:%M:%S',
                    level=logging.INFO)

logger = logging.getLogger(__name__)


def main_func_update(input_ids_data, labels_data, pad_token_id, ignore_index, pos_or_neg):
    # print(pos_or_neg.sum(), pos_or_neg.size())
    # logger.info('input_ids_data {0} | labels_data {1}'.format(input_ids_data.size(), labels_data.size()))

    # 获取input_ids
    input_ids_flatten = input_ids_data[input_ids_data != pad_token_id]

    # 获取labels_mask
    mask_pos = input_ids_data != pad_token_id
    mask_ignore = labels_data == ignore_index

    pos_or_neg = pos_or_neg.view(-1)

    labels_temp = (labels_data.mT * pos_or_neg).mT
    labels_temp[mask_ignore] = 0
    labels_temp[labels_temp > 0] = 1
    labels_temp[labels_temp < 0] = -1
    labels_mask = labels_temp[mask_pos]

    # labels_temp = labels_data[input_ids_data != pad_token_id]
    # labels_mask = labels_temp != ignore_index

    # 获取sample_len
    sample_len = (input_ids_data != pad_token_id).sum(dim=-1)

    # 获取sample_index
    sample_index = sample_len.cumsum(dim=-1)
    sample_index = torch.cat([torch.tensor([0]).int(), sample_index[:-1]], dim=-1)

    save_data_dict = {'input_ids': input_ids_flatten.int(),
                      'labels_mask': labels_mask.int(),
                      'sample_index': sample_index.long(),
                      'sample_len': sample_len.int()}

    return save_data_dict


def data_processor_chunk():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input_file', type=str, required=False)
    parser.add_argument('--tmp_dir', type=str, required=False)
    parser.add_argument('--output_file', type=str, required=False)
    parser.add_argument('--tok_max_len', default=8192, type=int)
    parser.add_argument('--tokenizer_version', default="v1")
    parser.add_argument('--model_dir', default="/mmu_vcg_wjc_hdd/duyong/kuaiyi/tokenizer_configs/tokenizer.128k.data_ratio", type=str)

    # parser.add_argument('--input_file', type=str, default="./i18n_train_0624_multi_retrival_caption_summary.json")
    # parser.add_argument('--tmp_dir', type=str,
    #                     default="/mmu_vcg_wjc_hdd/duyong/code/kwaii_train_and_inference/tokenizer/system_prompt/tmp_dir_cache")
    # parser.add_argument('--output_file', type=str,
    #                     default="/mmu_vcg2_wjc_ssd/gaoxiaoyang/aliyun/oversea_ecom_llm/sft_data/0626_29w_test/split_summary")
    # parser.add_argument('--tok_max_len', default=8192, type=int)
    # parser.add_argument('--tokenizer_version', default="v1")
    # parser.add_argument('--model_dir', default="/share/fukai07/models/tokenizer_configs/tokenizer.128k.data_ratio",
    #                     type=str)

    args = parser.parse_args()

    time_start = time.time()

    logger.info("load tokenizer ...")
    if args.tokenizer_version == 'v1':
        pwd = os.path.abspath(os.path.dirname(__file__))
        sys.path.append(pwd)
        from tokenization_kwaiyii import LlamaTokenizer
        tk = '/mmu_vcg_wjc_hdd/duyong/kuaiyi/tokenizer_configs/tokenizer.128k.data_ratio'
        tokenizer = LlamaTokenizer.from_pretrained(tk, trust_remote_code=True, add_bos_token=False)
    else:
        tokenizer = transformers.AutoTokenizer.from_pretrained(
            args.model_dir,
            model_max_length=args.tok_max_len,
            padding_side="right",
            use_fast=False
        )

    pad_token_id = tokenizer.pad_token_id
    ignore_id = -100

    logger.info("数据收集并写入文件")
    input_file = os.path.basename(f'{args.input_file}')
    fnms = os.listdir(args.tmp_dir)

    fnms = [fnm for fnm in fnms if f'{input_file}.out' in fnm]

    all_input_ids = []
    all_labels_mask = []
    all_sample_index = []
    all_sample_len = []
    pre_sample_index = 0

    for fnm in tqdm.tqdm(fnms):
        item = pickle.load(open(args.tmp_dir + '/' + fnm, 'rb'))

        input_ids_data = item['input_ids']
        labels_data = item['labels']
        pos_or_neg = item['pos_or_neg']

        save_data_dict = main_func_update(input_ids_data=input_ids_data, labels_data=labels_data,
                                          pad_token_id=pad_token_id, ignore_index=ignore_id, pos_or_neg=pos_or_neg)

        now_input_ids = save_data_dict['input_ids'].int().numpy().astype(np.int32)
        now_labels_mask = save_data_dict['labels_mask'].int().numpy().astype(np.int32)
        now_sample_index = save_data_dict['sample_index'].long().numpy().astype(np.int64)
        now_sample_len = save_data_dict['sample_len'].int().numpy().astype(np.int32)

        now_sample_index = now_sample_index + pre_sample_index

        all_input_ids.append(now_input_ids)
        all_labels_mask.append(now_labels_mask)
        all_sample_index.append(now_sample_index)
        all_sample_len.append(now_sample_len)
        pre_sample_index = now_sample_index[-1] + now_sample_len[-1]

    logger.info("文件大小，input_ids:{} labels_mask: {} sample_index: {} sample_len:{}".format(len(all_input_ids),
                                                                                              len(all_labels_mask),
                                                                                              len(all_sample_index),
                                                                                              len(all_sample_len)))
    logger.info("开始保存数据")


    all_input_ids = np.concatenate(all_input_ids)
    logger.info("开始写入input_ids, 文件大小{}".format(all_input_ids.shape))
    all_input_ids.tofile(args.output_file + '_input_ids.bin')

    all_labels_mask = np.concatenate(all_labels_mask)
    logger.info("开始写入labels_mask, 文件大小{}".format(all_labels_mask.shape))
    all_labels_mask.tofile(args.output_file + '_labels_mask.bin')

    all_sample_index = np.concatenate(all_sample_index)
    logger.info("开始写入sample_index, 文件大小{}".format(all_sample_index.shape))
    all_sample_index.tofile(args.output_file + '_sample_index.bin')

    all_sample_len = np.concatenate(all_sample_len)
    logger.info("开始写入sample_len, 文件大小{}".format(all_sample_len.shape))
    all_sample_len.tofile(args.output_file + '_sample_len.bin')

    logger.info("处理完成")


if __name__ == '__main__':
    data_processor_chunk()
