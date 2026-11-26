import storage 
import numpy as np
import base64
import EmbeddingModel_pb2
import ctypes
import sys
import argparse
import heapq
import pickle

sys.path.append("../cluster_matrix_parsing/")
from parse_matrix import parse_item, topk_similar_items, get_indices_above_threshold

parser = argparse.ArgumentParser()
parser.add_argument('--user_id', type=str, default='2863392057')
parser.add_argument('--file_hist', type=str, default='histact_product/history_action_product_2863392057')
parser.add_argument('--file_hist_detail', type=str, default='histact_product/history_action_product_2863392057')
parser.add_argument('--cluster_name', type=str, default='adU2URelation')
parser.add_argument('--cluster_name_emb', type=str, default='adRecallExplainData')
parser.add_argument('--prefix', type=str, default='userLongtimeProductFix')
parser.add_argument('--prefix_emb', type=str, default='llm_pdct_emb')
FLAGS, unparsed = parser.parse_known_args()

#redis_cluster_name = "adInterestRecall"
redis_cluster_name = FLAGS.cluster_name
redis = storage.get_kcc_redis_resource(redis_cluster_name)

redis_cluster_name_pdctemb = FLAGS.cluster_name_emb 
redis_pdctemb = storage.get_kcc_redis_resource(redis_cluster_name_pdctemb)

def get_histact_pdct_emb(user_key, pdctemb_prefix):
    c = redis.get(user_key)
    all_pdct_id = []
    all_pdct_emb = []
    if c is not None:
        mt = EmbeddingModel_pb2.Embeddings()
        d = mt.ParseFromString(c)
        a = mt.embeddings
        cnt = 0
        act_types = []
        for i in a:
            id_list = i.id_list
            if cnt == 0:
                id_list_str = ','.join([str(ele) for ele in id_list])
                # f1.writelines(user_key+':\n')
                # f1.writelines(id_list_str+'\n')
                for id_ in id_list:
                    id_ = ctypes.c_longlong(id_)
                    # f.writelines("'"+str(id_.value)+"'\n")
                    pdctemb_key = pdctemb_prefix + '_' + str(id_.value)
                    pdct_emb = redis_pdctemb.get(pdctemb_key)
                    #print("pdctemb_key_"+pdctemb_key);
                    # f1.writelines("pdctemb_key_"+pdctemb_key+":")
                    
                    if pdct_emb is not None:
                        mt1 = EmbeddingModel_pb2.Embeddings()
                        pdct_emb_parse = mt1.ParseFromString(pdct_emb)
                        pdct_emb_emb = mt1.embeddings
                        for spec in pdct_emb_emb:
                            pdct_emb_str_list = [str(hh) for hh in spec.embedding]
                            emb = np.array(spec.embedding)
                            all_pdct_id.append(id_)
                            all_pdct_emb.append(emb)
                            #print(emb)
                            # f1.writelines(",".join(pdct_emb_str_list)+"\n")
                    # else:
                        #print(pdctemb_key+" is None")
                        # f1.writelines("0,0,0,0,0,0,0,0\n")
                        # all_pdct_emb.append(np.array([0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]))
            else:
                act_types = id_list
            cnt += 1
    
    return all_pdct_id, all_pdct_emb


file_path = '/share/ad/lijupan/dataset/ljp_bs_feature%2Fcluster_matrix_pdctemb_1121'
item2_lst, product_lst = parse_item(file_path)


threshold = 0.1
#user_prefix = 'userRealLongtimeProductTest'
user_prefix = FLAGS.prefix 
pdctemb_prefix = FLAGS.prefix_emb 
pdct_num_lst = []
query_emb_lst = []
user_id_lst = []
pdct_id_lst = []

with open('long_act_user_for_test/part-00000-fba3ea37-d154-475d-8fcb-47be7fbd17f8-c000', 'r') as f:
    for line in f:
        user_id = line.strip()
        user_key = user_prefix + '_' + user_id
        all_pdct_id, all_pdct_emb = get_histact_pdct_emb(user_key, pdctemb_prefix)
        # print(all_pdct_id)
        if len(all_pdct_emb) > 0:
            query_emb = np.stack(all_pdct_emb)
            query_emb_lst.append(query_emb)
            user_id_lst.append(user_id)
            pdct_id_lst.append(all_pdct_id)
            
        # score = get_indices_above_threshold(query_emb, item2_lst)
        # score_max = np.amax(score, axis=1)
        # topk_indices = np.where(score_max > threshold)[0]
        # selected_items = [product_lst[i] for i in topk_indices]
        # pdct_num = len(set(selected_items))
        # pdct_num_lst.append(pdct_num)

# 创建一个字典来存储user_id和对应的query_emb
user_emb_dict = {user_id: emb for user_id, emb in zip(user_id_lst, query_emb_lst)}
user_pdct_dict = {user_id: pdct_id for user_id, pdct_id in zip(user_id_lst, pdct_id_lst)}

# 将字典写入文件
with open('user_query_emb_data.pkl', 'wb') as f:
    pickle.dump(user_emb_dict, f)

with open('user_query_pdct_data.pkl', 'wb') as f:
    pickle.dump(user_pdct_dict, f)
