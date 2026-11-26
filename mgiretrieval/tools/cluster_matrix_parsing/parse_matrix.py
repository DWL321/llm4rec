import numpy as np
import heapq

def find_target(in_str, target):
    parts = in_str.strip().split(target)
    values = parts[1].split('.')[0].split(',')
    return values[0]

def cosine_similarity(a, b):
    """计算两个向量的余弦相似度"""
    dot_product = np.dot(a, b)
    return dot_product
    # norm_a = np.linalg.norm(a)
    # norm_b = np.linalg.norm(b)
    # # print(dot_product)
    # if norm_a == 0 or norm_b == 0:
    #     return 0
    # else:
    #     return dot_product / (norm_a * norm_b)

def topk_similar_items(item2_lst, query_emb, k):
    """找出与查询embedding最相似的top k个候选物料的索引"""
    similarities = []
    for i, item_emb in enumerate(item2_lst):
        sim = cosine_similarity(query_emb, item_emb)
        similarities.append((i, sim))
    
    # 使用heapq.nlargest找出最大的k个元素
    topk_idx = heapq.nlargest(k, similarities, key=lambda x: x[1])
    # print(topk_idx)
    
    # 返回top k个元素的索引
    return [idx for idx, sim in topk_idx]

def get_indices_above_threshold(item2_lst, query_emb):
    """找出与查询embedding相似度大于给定阈值的候选物料的索引"""
    indices_above_threshold = []
    item2_arr = np.array(item2_lst)
    query_arr = np.array(query_emb)
    score = np.dot(query_arr, item2_arr.T)
    # for i, item_emb in enumerate(item2_lst):
    #     sim = cosine_similarity(query_emb, item_emb)
    #     if sim > threshold:
    #         indices_above_threshold.append(i)
    
    return score

def find_tensor(in_str, target):
    parts = in_str.strip().split(', tensor name')
    for part in parts:
        if target in part:
            value = [float(ele) for ele in part.split(target)[1].split(',')[:-1]]
            return value
    return None

def parse_item(file_path):
    item2_lst = []
    product_lst = []
    with open(file_path, 'r') as f:
        for line in f:
            photo_id = find_target(line, 'photo id: ')
            unit_id = find_target(line, 'unit id: ')
            cluster_id = find_target(line, 'cluster id: ')
            live_id = find_target(line, 'live id: ')
            cpa_bid = find_target(line, 'cpa bid: ')
            diversity_cluster_id = find_target(line, 'diversity_cluster_id:')
            city_product_id = find_target(line, 'city product id: ')
            product_lst.append(city_product_id)
            item1 = find_tensor(line, 'model/item_nn, value: [FloatVector]')
            item2 = find_tensor(line, 'model/t1/item_nn, value: [FloatVector]')
            item2_lst.append(item2)
    return item2_lst, product_lst

# file_path = '/Users/lijupan/Desktop/ad_algorithm/dataset/cluster_matrix_pdctemb_1121_10line'
# item2_lst, product_lst = parse_item(file_path)
# # print(item2_lst)
# query_emb = item2_lst[2]  # 查询embedding
# k = 20  # 想要的top k

# # topk_indices = topk_similar_items(item2_lst, query_emb, k)
# topk_indices = get_indices_above_threshold(item2_lst, query_emb, 0.2)
# print("Top", k, "similar item indices:", topk_indices)