# -*- coding: utf-8 -*-
import logging
from framework_wrappers import BtqClient,BtqException,BtqErrorCode
from model_pb2 import ModelItem, ModelUpdateMessage
import numpy as np
import struct
#from parse_emb import ParseEmb
import time
from optparse import OptionParser
import os

def parse_param():
    usage = "usage: %prog [input_directory] [btq]"
    parser = OptionParser(usage=usage, version="%prog 1.0")
    parser.add_option("-i",action="store",dest="input_dir",default="",type=str,help='input_dir')
    parser.add_option("-b",action="store",dest="target_btq",default="",type=str,help='target_btq')
    options, args = parser.parse_args()
    return options, args, usage

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
def send_btq(topic,key,content,slot,expire_timet=86400 * 90):
    msg = ModelUpdateMessage()
    msg.embedding_weight_size = -1
    item = ModelItem(
        embedding_weight=content.tobytes() if content is not None else content,
        slot=slot,
        sign=key,
        expire_timet=expire_timet,
        )
    msg.item.append(item)
    try:
        BtqClient.produce(topic, [msg.SerializeToString()])
        return 0
    except BtqException as e:
        logging.error('send btq message failed! {} {}'.format(e.code, e.msg))
        return -1

if __name__ == '__main__':
    # 解析参数
    options, args, usage = parse_param()
    print(options, args, usage)
    input_dir = options.input_dir
    topic = options.target_btq

    for filename in os.listdir(input_dir):
        # 构建完整的文件路径
        file_path = os.path.join(input_dir, filename)
        
        # 确保是文件而不是目录
        if os.path.isfile(file_path):
            # 打开并读取文件
            with open(file_path, 'r', encoding='utf-8') as file:
                # 逐行读取文件内容
                for line in file:
                    # 处理每一行的数据
                    # 这里只是一个示例，你可以根据需要解析和处理每一行
                    photo_id = int(line.strip().replace('\n', ''))
                    content = None 
                    m=send_btq('dsp_lps_ljp_casQv1', photo_id, content, 0)
                    #print("photo_id:{}, m:{}".format(photo_id, m))
