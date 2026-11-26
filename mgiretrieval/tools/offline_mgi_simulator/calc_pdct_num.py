import sys
import numpy as np
sys.path.append("../cluster_matrix_parsing/")
from parse_matrix import parse_item, topk_similar_items, get_indices_above_threshold
import pickle
from optparse import OptionParser

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

# 访问特定user_id的query_emb

options, args, usage = parse_param()
print(options, args, usage)
threshold = options.inte_thres

pdct_num_lst = []
for user_id_to_lookup in user_emb_dict.keys():
    query_emb = user_emb_dict[user_id_to_lookup]
    #print(query_emb)
    # print(f"Query embeddings for {user_id_to_lookup}:\n{query_emb}")
    score = np.dot(query_emb, item2.T)
    #print(score.shape)
    score_max = np.amax(score, axis=0)
    #print(score_max)
    topk_indices = np.where(score_max > threshold)[0]
    #print(score_max[topk_indices])
    # print(topk_indices)
    selected_items = [product_lst[i] for i in topk_indices]
    pdct_num = len(set(selected_items))
    pdct_num_lst.append(pdct_num)
    
# with open('long_act_user_for_test/user_pdct_num', 'a') as fo:
#     for pdct_num in pdct_num_lst:
#         fo.write('{}\n'.format(pdct_num))

with open('long_act_user_for_test/user_pdct_num_thres_{}.pkl'.format(threshold), 'wb') as f:
    pickle.dump(pdct_num_lst, f)