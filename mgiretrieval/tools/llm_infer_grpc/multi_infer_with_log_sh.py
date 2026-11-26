# -*- coding: utf-8 -*-
import json
import pandas as pd
import time
import os
os.environ["PROTOCOL_BUFFERS_PYTHON_IMPLEMENTATION"] = "python"
os.environ["KWS_SERVICE_AZ"] = "HB2AZ2"
os.environ["KWS_SERVICE_REGION"] = "HB2"
os.environ["KWS_SERVICE_DC"] = "WLF2"
os.environ["KWS_SERVICE_NAME"] = "ai-platform-dense-server"
os.environ["KWS_SERVICE_CATALOG"] = "ai-platform.ksnserver.dense-server"
os.environ["KWS_SERVICE_STAGE"] = "PROD"
import traceback
import logging
import argparse
from tqdm import tqdm
from concurrent.futures import ThreadPoolExecutor, as_completed
from kess.framework import ClientOption, GrpcClient
import protos.mmu_kuaiyi_chat_pb2 as mmu_kuaiyi_chat_pb2
from protos.mmu_kuaiyi_chat_pb2_grpc import KuaiYiChatServiceStub
from google.protobuf.json_format import MessageToDict

#配置日志记录
def configure_logging(log_file_path):
    logging.basicConfig(
        filename=log_file_path,
        level=logging.INFO,
        format='%(asctime)s - %(levelname)s - %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S'
    )
    # 添加控制台输出
    console = logging.StreamHandler()
    console.setLevel(logging.INFO)
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
    console.setFormatter(formatter)
    logging.getLogger().addHandler(console)

    # 设置特定库的日志级别为WARNING或更高
    logging.getLogger('apscheduler').setLevel(logging.WARNING)
    logging.getLogger('kess').setLevel(logging.WARNING)
    logging.getLogger('urllib3').setLevel(logging.WARNING)
    logging.getLogger('grpc').setLevel(logging.WARNING)
    logging.getLogger('kess.framework').setLevel(logging.WARNING)
    
# 配置gRPC客户端
def configure_grpc_client():
    grpc_service_name = "mmu-kuaiyi-chat-service"
    client_option = ClientOption(
        biz_def='infra',
        grpc_service_name=grpc_service_name,
        grpc_stub_class=KuaiYiChatServiceStub,
    )
    return GrpcClient(client_option)

# 读取文件中的前num_lines行并解析为JSON
def read_and_parse_file(file_path, num_lines=500):
    with open(file_path, 'r', encoding="utf8") as file:
        lines = [file.readline().strip() for _ in range(num_lines)]
    json_data_list = [json.loads(line) for line in lines if line]
    return json_data_list

def read_json_file(file_path):
    with open(file_path, 'r', encoding="utf8") as file:
        json_data_list = json.load(file)
    return json_data_list

# 调用gRPC服务
def call_grpc_service(client, infer_data):
    try:
        stub = client.get_stub()
        # 请求配置
        req = mmu_kuaiyi_chat_pb2.KuaiYiChatCompletionRequest()
        req.key = "lijupan_ac1114_qwen2_7b_instruct_outerloop_recall_241114"
        req.model = "qwen2_7b_instruct_outerloop_recall_241114"
        req.stream = False
        req.n = 1
        # input配置
        msg1 = mmu_kuaiyi_chat_pb2.MessageInfo()
        msg1.role = "user"
        msg1.content = infer_data
        req.messages.append(msg1)
        response = stub.Chat(req)
        output = ""
        if len(response.choices) > 0:
            output = response.choices[0].message["assistant"]
        return MessageToDict(response), output, None
    except Exception as e:
        error_message = str(e)
        logging.error(f"Error calling gRPC service: {error_message}")
        traceback.print_exc()
        return None, None, error_message

# 使用线程池并行调用gRPC服务
def process_data_in_parallel(client, json_data_list, max_workers=10):
    results = []
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        future_to_data = {executor.submit(call_grpc_service, client, json_data['instruction']): json_data for json_data in json_data_list}
        for future in as_completed(future_to_data):
            json_data = future_to_data[future]
            try:
                response, output, error = future.result()
                result = {
                    "user_id": json_data.get('user_id'),
                    "instruction": json_data.get('instruction'),
                    "output": output,
                    "error": error,
                    "response_info":response
                }
                results.append(result)
            except Exception as e:
                logging.error(f"Error processing data: {json_data}")
                traceback.print_exc()
                result = {
                    "user_id": json_data.get('user_id'),
                    "instruction": json_data.get('instruction'),
                    "output": None,
                    "error": str(e),
                    "response_info":None
                }
                results.append(result)
    return results
def parse_param():
    parser = argparse.ArgumentParser(description="Process some files.")
    parser.add_argument("-i", "--input_file", type=str, required=True, help="Path to the input file")
    parser.add_argument("-o", "--output_file", type=str, required=True, help="Path to the output directory")

    args = parser.parse_args()
    print(f"Parsed arguments: {args}")
    return args
# 主函数
def main():
    args = parse_param()
    output_dir = args.output_file
    data_path = args.input_file

    #output_dir = '/share/ad/yanghang07/sft_infer/infer_output'
#     output_dir = '/share/ad/yanghang07/sft_infer/infer_output/download_10_1'
#     data_path = '/share/ad/yanghang07/sft_infer/input_data/download_10_1/result.json'
    log_file_path = os.path.join(output_dir, 'processing_log.txt')
    os.makedirs(output_dir, exist_ok=True)
    
    # 配置日志记
    configure_logging(log_file_path)
    
    cpu_count = os.cpu_count()
    max_workers = cpu_count
    #max_workers = cpu_count if cpu_count <=48 else 48
    
    logging.info(f'CPU 核心数：{cpu_count}')
    logging.info(f'Max workers: {max_workers}')
    client = configure_grpc_client()
    
    # 读取所有数据
    json_data_list = read_json_file(data_path)
   # json_data_list = json_data_list[:150000]
    logging.info(f'推理数据量：{len(json_data_list)}')
    
    batch_size = 1000
    total_batches = (len(json_data_list) + batch_size - 1) // batch_size
    
    start = time.time()
    output_file_path = os.path.join(output_dir, 'result_all.jsonl')
    for batch_num in tqdm(range(total_batches)):
        batch_start_time = time.time()
        
        batch_data = json_data_list[batch_num * batch_size : (batch_num + 1) * batch_size]
        results = process_data_in_parallel(client, batch_data, max_workers=48)
        
        # 将结果逐行追加到文件中
        with open(output_file_path, 'a', encoding='utf8') as file:
            for result in results:
                file.write(json.dumps(result, ensure_ascii=False) + '\n')
        
        batch_end_time = time.time()
        batch_duration = batch_end_time - batch_start_time
        logging.info(f"Batch {batch_num + 1}/{total_batches} processed and saved in {batch_duration:.2f} seconds.")
    
    end = time.time()
    total_duration = end - start
    logging.info(f"总耗时：{total_duration:.2f} seconds.")

if __name__ == "__main__":
    main()
