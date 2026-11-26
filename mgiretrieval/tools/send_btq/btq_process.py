import logging
from framework_wrappers import BtqClient,BtqException,BtqErrorCode
from model_pb2 import ModelItem, ModelUpdateMessage
import numpy as np
import struct
#from parse_emb import ParseEmb
import time
from ratelimit import limits, sleep_and_retry
from datetime import datetime, timedelta
import os
import subprocess
from optparse import OptionParser

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

def send_btq_batch(topic,key_list,content_list,slot,expire_timet=86400 * 90):
    msg=ModelUpdateMessage()
    msg.embedding_weight_size = -1
    for i in range(len(key_list)):
        key=key_list[i]
        content=content_list[i]
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

def consume_btq(topic,reader):
    try:
        m=BtqClient.consume(topic,reader,encoding=None)
        msg=ModelUpdateMessage()
        msg.ParseFromString(m.message)
        return  msg
    except BtqException as e:
        logging.error('consume btq message failed! {} {}'.format(e.code,e.msg))
        raise e
        return -1

def check_file_exists_on_hdfs(hdfs_path):
    try:
        # 使用hadoop fs -test -e命令检查文件是否存在
        # 如果文件存在，命令会返回0，否则返回1
        result = subprocess.run(['hadoop', 'fs', '-test', '-e', hdfs_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return result.returncode == 0
    except Exception as e:
        print(f"An error occurred: {e}")
        return False

def run_hadoop_command(command):
    try:
        result = subprocess.run(command, shell=True, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return result.returncode == 0
    except subprocess.CalledProcessError as e:
        print(f"An error occurred: {e}")
        return False

# @sleep_and_retry
# @limits(calls=12, period=1)
def my_func(topic, output_dir):
    try:
        msg=consume_btq(topic,'test_reader')
        for item in msg.item:
            photo_id = item.sign
            photo_dense_cap = item.embedding_weight.decode('utf-8')
            current_hour = datetime.now().strftime('%Y%m%d%H00')
            out_file = os.path.join(output_dir, 'photo_dense_caption_{}.txt'.format(current_hour))
            with open(out_file, 'a') as file:
                file.write("{}\t{}\n".format(photo_id, photo_dense_cap))
            last_hour = (datetime.now() - timedelta(hours=1)).strftime('%Y%m%d%H00')
            last_filename = os.path.join(output_dir, 'photo_dense_caption_{}.txt'.format(last_hour))
            if os.path.exists(last_filename):
                if not check_file_exists_on_hdfs('/home/ad/lijupan/llm/realtime_dense_caption/photo_dense_caption_{}.txt'.format(last_hour)):
                    flag = run_hadoop_command('hadoop fs -put {} /home/ad/lijupan/llm/realtime_dense_caption/'.format(last_filename))
                    if flag:
                        print('Put {} to hdfs successfully!'.format(last_filename))
            # /home/ad/lijupan/llm/realtime_dense_caption
    except Exception as e:
        print(e)
        time.sleep(1)

def parse_param():
    usage = "usage: %prog [output_dir] [btq]"
    parser = OptionParser(usage=usage, version="%prog 1.0")
    parser.add_option("-o",action="store",dest="output_dir",default="",type=str,help='output_dir')
    parser.add_option("-b",action="store",dest="target_btq",default="",type=str,help='target_btq')
    options, args = parser.parse_args()
    return options, args, usage

if __name__=='__main__':
    # pe=ParseEmb()
    # topic='dsp_lps_ljp_casQ'
    # output_dir='caption_data/'
    options, args, usage = parse_param()
    print(options, args, usage)
    output_dir = options.output_dir
    topic = options.target_btq

    while True:
        my_func(topic, output_dir)
        # msg=consume_btq(topic,'test_reader')
        # try:
        #     for item in msg.item:
        #         photo_id = item.sign
        #         photo_dense_cap = item.embedding_weight.decode('utf-8')
        #         fo.write("{}\t{}\n".format(photo_id, photo_dense_cap))
        #         # print("photo_id:{}, photo_dense_cap:{}".format(photo_id, photo_dense_cap))
        #         # print(item)
        #         #tmp=pe.parse_emb(item.embedding_weight,'int64')
        #         #print(tmp)
        #         #m=send_btq('ad_inner_p2l_photo_embedding_6',item.sign,tmp,0)
        #         #print(m)
        # except Exception as e:
        #     print(e)
        #     time.sleep(1)



