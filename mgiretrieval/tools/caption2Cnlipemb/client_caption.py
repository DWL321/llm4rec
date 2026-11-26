import grpc
from optparse import OptionParser
import numpy as np
from protos import model_serving_pb2
from protos import model_serving_pb2_grpc
import torch
import time
import multiprocessing
from protos.model_serving_pb2 import PredictRequest, Feature, FloatArray, BatchPredictRequest
import cn_clip.clip as clip
from cn_clip.clip import load_from_name, available_models
def run(text_features, photos, len_answer, out_file):
    # 连接到 gRPC 服务器
    
    channel = grpc.insecure_channel('localhost:50009')  # 替换为你的服务器地址和端口
    stub = model_serving_pb2_grpc.ModelServingStub(channel)
    
    # 创建 BatchPredictRequest
    request = model_serving_pb2.BatchPredictRequest()
    request.id = "batch_request_1"
    start_time = time.time()


    # 创建多个 PredictRequest
    for i in range(len_answer):  # 假设批量请求包含5个单独的请求
        predict_request = model_serving_pb2.PredictRequest()
        predict_request.id = f"request_{i}"
        # 创建 Feature
        feature = model_serving_pb2.Feature()
        float_array = model_serving_pb2.FloatArray()
        # 将text_features转换为FloatArray
        float_array.float_elems.extend(text_features[i])  # 假设text_features是一个列表，每个元素是一个特征向量
        feature.float_arrays.append(float_array)
        predict_request.feature.CopyFrom(feature)
        # 添加到 BatchPredictRequest
        request.requests.append(predict_request)


    request_time = time.time()
    request_time_cost = request_time-start_time
    print("request_time_cost",request_time_cost)
    # 调用 BatchPredict 方法
    print("request")
    response = stub.BatchPredict(request)
    all_tensor=[]
    response_time = time.time()
    response_time_cost = response_time- request_time
    print("response_time_cost", response_time_cost)
    # 解析响应
    for i in range(len_answer):
        res1 = [torch.tensor(i.float_elems) for i in response.result['request_{}'.format(i)].feature.float_arrays]
        all_tensor.append(res1[0])
    all_tensor = torch.stack(all_tensor)
    print(all_tensor.shape)
    jiexi_time = time.time()
    jiexi_time_cost = jiexi_time - response_time
    print("jiexi_time_cost",jiexi_time_cost)
    answer = all_tensor.numpy().tolist()
    rst = []
    # 写入txt
    for i in range(len_answer):
        lst = []
        line = ','.join(map(str, answer[i]))
        lst.append(photos[i])
        lst.append(line)
        rst.append('\t'.join(lst))
    
    with open(out_file, 'a', encoding='utf-8') as fo:
        for r in rst:
            fo.write(r + '\n')
    write_time = time.time()
    write_time_cost = write_time - jiexi_time
    print("write_time", write_time)
    return response



def parse_param():
    usage = "usage: %prog [input_file] [output_file]"
    parser = OptionParser(usage=usage, version="%prog 1.0")
    parser.add_option("-i",action="store",dest="input_file",default="",type=str,help='input_file')
    parser.add_option("-o",action="store",dest="output_file",default="",type=str,help='output_file name')

    options, args = parser.parse_args()
    return options, args, usage


if __name__ == '__main__':
    # 解析参数
    options, args, usage = parse_param()
    print(options, args, usage)

    print("Available models:", available_models())  
    # Available models: ['ViT-B-16', 'ViT-L-14', 'ViT-L-14-336', 'ViT-H-14', 'RN50']

    # device = "cuda" if torch.cuda.is_available() else "cpu"
    # torch.cuda.set_device(1)
    device = torch.device("cuda:5" if torch.cuda.is_available() else "cpu")
    print("device:")
    print(device)
    model, preprocess = load_from_name("ViT-L-14", device=device, download_root='./')
    model.eval()
    
    
    # import json
    # 读取answer

    # file_name = 'models/outerloop_pdct_data_product_v0_result.txt'
    file_name = options.input_file 
    print("input_file:"+file_name)

    # 初始化两个列表
    photos = []
    captions = []

    # 读取文件并解析 JSON 数据
    with open(file_name, 'r', encoding='utf-8') as file:
        for line in file:
            data = line.strip().replace('\n', '').split('\t')
            if len(data) == 2:
                photos.append(data[0])
                captions.append(data[1])

    # 打印结果
    print("photos:", len(photos))
    print("captions:", len(captions))
    out_file = options.output_file

    def process_in_batches(captions, batch_size, photos):
        len_answer = len(captions)
        for i in range(0, len_answer, batch_size):
            start_time = time.time()
            batch_captions = captions[i:i + batch_size]
            batch_photos = photos[i:i + batch_size]
            # Tokenize the batch
            text = clip.tokenize(batch_captions,context_length = 320).to(device)
            print(f"Tokenize batch {i // batch_size + 1} done")
            tokenize_time = time.time()
            token_time = tokenize_time - start_time
            print(f"token time: {token_time} seconds")
            # Encode the batch
            print("text", text.shape)
            with torch.no_grad():
                text_features = model.encode_text(text)
                text_features /= text_features.norm(dim=-1, keepdim=True)
                print(f"Text features shape for batch {i // batch_size + 1}: {text_features.shape}")
            encoder_time = time.time()
            enco_time = encoder_time - tokenize_time
            print(f"encoder_time: {enco_time} seconds")
            # Process the batch
            print(text_features.shape, len(batch_photos), len(batch_captions))
            response = run(text_features, batch_photos, len(batch_captions), out_file)
            # 你可以在这里处理 response，例如保存结果或继续其他操作
            end_time = time.time()
            elapsed_time = end_time - encoder_time
            total_time = end_time - start_time
            print(f"Elapsed time: {elapsed_time} seconds")
            print(f"Total time: {total_time} seconds")
    # 设置批量大小
    batch_size = 1000  # 根据你的显存和需求调整

    # 调用分批处理函数
    process_in_batches(captions, batch_size, photos)