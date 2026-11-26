
import json
import multiprocessing
import pickle
import random

from tqdm import tqdm
import torch
import transformers
import argparse
import os, sys


class Template:
    def __init__(self, system, roles, sep, sep2):
        self.system = system
        self.roles = roles
        self.sep = sep
        self.sep2 = sep2


default_template = Template(
    system="A chat between a curious user and an artificial intelligence assistant. "
           "The assistant gives helpful, detailed, and polite answers to the user's questions.",
    roles=("USER: ", "ASSISTANT: "),
    sep='\n',
    sep2='</s>'
)


conv_self_introduce_system_prompt = Template(
    # system="你是快手人工智能团队独立开发的一款友好且善意的AI虚拟助手，底层模型是基于Transformer的解码器(Decoder)架构的深度学习模型，致力于全方位帮助用户解决各种问题，无论是回答疑难问题还是协助处理日常琐事，您都始终乐于与用户进行愉快的对话并且根据用户的需求给出符合用户要求的结果。",
    system="我是「快意」，是一款由快手AI团队打造的大规模语言模型。我基于海量数据和知识持续迭代进化，拥有广泛的多领域知识和创作才能，具备出色的语言理解和生成能力，能够理解并高效执行各类任务。我擅长知识问答、文案创作、文字翻译、数学逻辑、代码理解和编写等任务，致力于全方位帮助用户解决各种问题，期待能够为你提供更智能、轻快的互动体验 。",
    roles=("问题: ", "答案: "),
    sep=" ",
    sep2="</s>",
)

conv_self_introduce_system_prompt_v2 = Template(
    # system="你是快手人工智能团队独立开发的一款友好且善意的AI虚拟助手，底层模型是基于Transformer的解码器(Decoder)架构的深度学习模型，致力于全方位帮助用户解决各种问题，无论是回答疑难问题还是协助处理日常琐事，您都始终乐于与用户进行愉快的对话并且根据用户的需求给出符合用户要求的结果。",
    system="我是「快意」，是一款由快手AI团队打造的大规模语言模型。我基于海量数据和知识持续迭代进化，拥有广泛的多领域知识和创作才能，具备出色的语言理解和生成能力，能够理解并高效执行各类任务。我擅长知识问答、文案创作、文字翻译、数学逻辑、代码理解和编写等任务，致力于全方位帮助用户解决各种问题，期待能够为你提供更智能、轻快的互动体验。",
    roles=("问题: ", "答案: "),
    sep="\n",
    sep2="</s>",
)
# 当前第一个sep是空格，而不是换行符。新版直接改为换行符。 另外，是否要保持兼容，也要考虑下。

null_template = Template(
    system="",
    roles=("", ""),
    sep='',
    sep2='</s>'
)

for_gsm8k = Template(
    system="",
    roles=("", ""),
    sep='',
    sep2='</s>'
)

temps = {'default': conv_self_introduce_system_prompt,
         'default2': conv_self_introduce_system_prompt_v2,
         'null_template': null_template, 'for_gsm8k': for_gsm8k}


class ConversationFormater:
    def __init__(self, max_seq_length):
        self.max_seq_length = max_seq_length
        self.eval = False

    def dat2input_ids(self, info, template_name='default'):
        if random.random() > 0.5 and template_name == 'default':
            template_name = 'default2'

        qa_pairs = info['data']
        global tokenizer
        template = temps[template_name]

        input_ids = []
        target = []
        # assert len(qa_pairs) > 0, 'NULL qa_pairs exception!'
        # if 'system' in qa_pairs[0]:
        # if len(template.system):
        #     seq = f'{template.system}{template.sep}'
        # else:
        seq = ''
        # input_ids, target = self.update(seq, input_ids, target, with_loss=False, special_token=True)

        for idx, qa in enumerate(qa_pairs):
            system_prompt = qa.get('system')
            if idx == 0:  # 首轮
                seq += "<s>"
                input_ids, target = self.update("<s>", input_ids, target, with_loss=False)
                if system_prompt is None:
                    system_prompt = template.system


            # 最后一轮允许没有answer，支持inference
            q = qa['question']
            a = qa['answer']
            if system_prompt is not None:
                part1 = system_prompt + template.sep
            else:
                part1 = ''
            if len(q):
                part1 += template.roles[0] + q + template.sep + template.roles[1]
            else:
                part1 += template.sep + template.roles[1]
            seq += part1
            input_ids, target = self.update(part1, input_ids, target, with_loss=False)

            if len(a):
                loss_flag = True
                if 'loss' in qa:
                    assert qa['loss'] in (0, 1), qa
                    if qa['loss'] == 0:
                        loss_flag = False
                part2 = a + template.sep2
                # 只有在part2上会计算loss
                seq += part2
                input_ids, target = self.update(part2, input_ids, target, with_loss=loss_flag)

        input_ids2 = tokenizer([seq]).input_ids[0]
        # 再加个padding，就完事
        if len(input_ids2) != len(input_ids) and 0:
            # 长度相同，也不见得是等价的。
            print(len(input_ids2), len(input_ids), 'not equal')
            print(input_ids)
            print(input_ids2)
            a1 = tokenizer.convert_ids_to_tokens(input_ids)
            a2 = tokenizer.convert_ids_to_tokens(input_ids2)
            print(a1)
            print(a2)
            print('==')
        if self.eval:
            input_ids = torch.tensor([input_ids], dtype=torch.int64) # 假batch
            smp = dict(input_ids=input_ids,
                       attention_mask=input_ids.ne(tokenizer.pad_token_id),
                       seq=seq)
            return smp
        input_ids, target = self.pad_it(input_ids, target)
        loss_flag = info.get('loss', 1)

        return input_ids, target, loss_flag, seq

    def pad_it(self,input_ids, target):
        input_ids = input_ids[:self.max_seq_length]
        target = target[:self.max_seq_length]

        while len(input_ids) < self.max_seq_length:
            input_ids.append(tokenizer.pad_token_id)
            target.append(-100)
        # input_ids = torch.tensor(input_ids, dtype=torch.int)
        # target = torch.tensor(target, dtype=torch.int)

        return input_ids, target

    def update(self, part, input_ids, target, with_loss, special_token=False):
        # seq_rep = tokenizer([part])
        # seq_rep = seq_rep.input_ids[0]
        tokens = tokenizer.tokenize("。"+part)
        tokens = tokens[1:]
        seq_rep = tokenizer.convert_tokens_to_ids(tokens)
        # 超长截断的问题，再思考一下。

        input_ids.extend(seq_rep)
        if with_loss:
            target.extend(seq_rep)
        else:
            target.extend([-100] * len(seq_rep))
        return input_ids, target


global tokenizer


def process_ln(ln):
    global tokenizer, conv_formater
    info = json.loads(ln)

    # template = info.get('template', 'default')
    template = info.get('template', 'default')
    # template = 'null_template'
    input_ids, target, loss_flag, seq = conv_formater.dat2input_ids(info=info, template_name=template)
    return dict(input_ids=input_ids, labels=target, seq=seq, pos_or_neg=loss_flag)

conv_formater = None

# tokenizer = transformers.AutoTokenizer.from_pretrained(
#     "/aigc_sgply_ssd/wanghongzhen/LLM/huggingface_model/llama_13b_ext",
#     model_max_length=2048,
#     padding_side="right",
#     use_fast=False
# )

def process():
    global tokenizer, conv_formater

    parser = argparse.ArgumentParser()
    parser.add_argument('--model_name_or_path',
                        required=True, type=str)
    parser.add_argument('--cache_dir', default=None, type=str)
    parser.add_argument('--model_max_length', default=2048, type=int)
    parser.add_argument('--input_data_dir',
                        required=True, type=str)
    parser.add_argument('--output_data_dir',
                        default='',
                        type=str)
    parser.add_argument('--thread_num', default=10, type=int)
    parser.add_argument('--tokenizer_version', required=True, choices=['v0', 'v1'])
    args = parser.parse_args()

    if args.tokenizer_version == 'v1':
        pwd = os.path.abspath(os.path.dirname(__file__))
        sys.path.append(pwd)
        from tokenization_kwaiyii import LlamaTokenizer
        tk = '/mmu_vcg_wjc_hdd/duyong/kuaiyi/tokenizer_configs/tokenizer.128k.data_ratio'
        tokenizer = LlamaTokenizer.from_pretrained(tk, trust_remote_code=True, add_bos_token = False)
    else:
        tokenizer = transformers.AutoTokenizer.from_pretrained(
            args.model_name_or_path,
            model_max_length=args.model_max_length,
            padding_side="right",
            use_fast=False
        )
    conv_formater = ConversationFormater(args.model_max_length)
    if 0:
        for ln in open(args.input_data_dir):
            info = json.loads(ln)
            input_ids, target, seq = conv_formater.dat2input_ids(qa_pairs=info['data'])
    # pool = multiprocessing.Pool(1)
    # encoded_docs = pool.imap_unordered(process_ln, open(args.input_data_dir), 1)
    encoded_docs = []
    for ln in tqdm(open(args.input_data_dir).readlines()):
        encoded_docs.append(process_ln(ln))

    input_ids = []
    labels = []
    seqs = []
    pos_or_neg = []
    for smp in encoded_docs:
        input_ids.append(smp['input_ids'])
        labels.append(smp['labels'])
        seqs.append(smp['seq'])
        pos_or_neg.append(smp['pos_or_neg'])

    print(len(seqs), 'total_number')
    if len(args.output_data_dir) == 0:
        output_data_dir = args.input_data_dir + '.new_pkl'
    else:
        output_data_dir = args.output_data_dir
    pos_or_neg = torch.tensor(pos_or_neg).unsqueeze(1)

    with open(output_data_dir, 'wb') as f:
        labels = torch.tensor(labels, dtype=torch.int32)
        input_ids = torch.tensor(input_ids, dtype=torch.int32)
        all_dict = dict(input_ids=input_ids,
                        labels=labels,
                        seq=seqs,
                        attention_mask=input_ids.ne(tokenizer.pad_token_id),
                        pos_or_neg=pos_or_neg
                        )
        pickle.dump(all_dict, f)

if __name__ == '__main__':
    process()
