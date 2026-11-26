import sys
import numpy as np
sys.path.append("../cluster_matrix_parsing/")
from parse_matrix import parse_item, topk_similar_items, get_indices_above_threshold
import pickle
from optparse import OptionParser
import random

def parse_param():
    usage = "usage: %prog [threshold]"
    parser = OptionParser(usage=usage, version="%prog 1.0")
    parser.add_option("-t", action="store", dest="inte_thres", default=0.2, type=float, help='inte_cut_score')
    # parser.add_option("-b",action="store",dest="target_btq",default="",type=str,help='target_btq')
    options, args = parser.parse_args()
    return options, args, usage

# 从文件中读取字典
with open('user_query_emb_data.pkl', 'rb') as f:
    user_emb_dict = pickle.load(f)

with open('item2.pkl', 'rb') as f:
    item2 = pickle.load(f)
print(item2[0:10,:])
print(item2.shape)

with open('product.pkl', 'rb') as f:
    product_lst = pickle.load(f)

with open('user_query_pdct_data.pkl', 'rb') as f:
    user_query_pdct_data = pickle.load(f)

user_query_pdct_lst = []
for uid in user_query_pdct_data.keys():
    query_pdct = ','.join([str(pdct).split('(')[1].split(')')[0] for pdct in user_query_pdct_data[uid]])
    user_query_pdct_lst.append('{}\t{}'.format(uid, query_pdct))
print(user_query_pdct_lst[0:10])
# 访问特定user_id的query_emb

options, args, usage = parse_param()
print(options, args, usage)
threshold = options.inte_thres

pdct_id_lst = []
part_user_query_pdct_lst = []
idx = 0
for user_id_to_lookup in user_emb_dict.keys():

    rand_number = random.random()
    # 如果随机数小于0.01，则跳出循环
    if rand_number > 0.01:
        idx = idx + 1
        continue

    query_emb = user_emb_dict[user_id_to_lookup]
    score = np.dot(query_emb, item2.T)
    score_max = np.amax(score, axis=0)
    topk_indices = np.where(score_max > threshold)[0]
    selected_items = [product_lst[i] for i in topk_indices]
    pdct_id_lst.append(set(selected_items))
    part_user_query_pdct_lst.append(user_query_pdct_lst[idx])
    idx = idx + 1

    
# with open('long_act_user_for_test/user_pdct_num', 'a') as fo:
#     for pdct_num in pdct_num_lst:
#         fo.write('{}\n'.format(pdct_num))

query_pdct_topk = {query_pdct: pdct_id for query_pdct, pdct_id in zip(user_query_pdct_lst, pdct_id_lst)}

# 将字典写入文件
with open('query_pdct_topk_thres_{}.pkl'.format(threshold), 'wb') as f:
    pickle.dump(query_pdct_topk, f)