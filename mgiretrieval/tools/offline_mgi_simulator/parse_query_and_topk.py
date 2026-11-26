import pickle

with open('query_pdct_topk_thres_0.15.pkl', 'rb') as f:
    d = pickle.load(f)

# 分开成为两个表
# 表1: user_id 以及历史行为的 product_id
# 表2: user_id 以及检索返回的 product_id

# /share/ad/lijupan/dataset/
f1 = open('user_hist_pdct.txt', 'w')
f2 = open('user_top_pdct.txt', 'w')

for key in d.keys():
    user_id = key.split('\t')[0]
    hist_pdct = key.split('\t')[1].split(',')
    top_product_id = d[key]
    for pdct in hist_pdct:
        f1.writelines('{}\t{}\n'.format(user_id, pdct))
    for pdct in top_product_id:
        f2.writelines('{}\t{}\n'.format(user_id, pdct))
    # print(type(d[key]))
    # break

f1.close()
f2.close()
