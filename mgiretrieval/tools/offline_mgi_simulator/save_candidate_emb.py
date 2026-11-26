import sys
import numpy as np
sys.path.append("../cluster_matrix_parsing/")
from parse_matrix import parse_item, topk_similar_items, get_indices_above_threshold
import pickle

file_path = '/share/ad/lijupan/dataset/ljp_bs_feature%2Fcluster_matrix_pdctemb_1121'
item2_lst, product_lst = parse_item(file_path)
item2 = np.stack(item2_lst)

# 将NumPy数组存储到文件
with open('item2.pkl', 'wb') as f:
    pickle.dump(item2, f)

with open('product.pkl', 'wb') as f:
    pickle.dump(product_lst, f)