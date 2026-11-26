import storage 
import numpy as np
import base64
import EmbeddingModel_pb2
import ctypes
import sys
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--user_id', type=str, default='1365886720')
parser.add_argument('--file_hist', type=str, default='histact_content_20240923/history_action_content_1365886720')
parser.add_argument('--file_hist_detail', type=str, default='histact_content_20240923/history_action_content_1365886720')
parser.add_argument('--cluster_name', type=str, default='adRecallResultInspector')
parser.add_argument('--cluster_name_emb', type=str, default='adRecallResultInspector')
parser.add_argument('--prefix', type=str, default='urdap')
parser.add_argument('--prefix_emb', type=str, default='e2ephotoembv2_')
FLAGS, unparsed = parser.parse_known_args()


#redis_cluster_name = "adInterestRecall"
redis_cluster_name = FLAGS.cluster_name
redis = storage.get_kcc_redis_resource(redis_cluster_name)

redis_cluster_name_pdctemb = FLAGS.cluster_name_emb 
redis_pdctemb = storage.get_kcc_redis_resource(redis_cluster_name_pdctemb)

#user_prefix = 'userRealLongtimeProductTest'
user_prefix = FLAGS.prefix 
user_key = user_prefix + '_' + FLAGS.user_id

pdctemb_prefix = FLAGS.prefix_emb 
wgt_mp = {128:5.0,64:1.0,32:2.0,16:5.0,8:5.0,4:2.0,2:10.0,1:10.0,48:5.0,40:5.0,36:2.0,34:10.0,33:10.0}

# write history action product to file
f = open(FLAGS.file_hist, 'w')
f1 = open(FLAGS.file_hist_detail, 'w')

c = redis.get(user_key)
if c is not None:
    mt = EmbeddingModel_pb2.Embeddings()
    d = mt.ParseFromString(c)
    a = mt.embeddings
    cnt = 0
    all_pdct_emb = []
    act_types = []
    for i in a:
        id_list = i.id_list
        if cnt == 0:
            #print(user_key+':')
            #print(id_list)
            id_list_str = ','.join([str(ele) for ele in id_list])
            f1.writelines(user_key+':\n')
            f1.writelines(id_list_str+'\n')
            for id_ in id_list:
                id_ = ctypes.c_longlong(id_)
                f.writelines("'"+str(id_.value)+"'\n")
                pdctemb_key = pdctemb_prefix + '_' + str(id_.value)
                pdct_emb = redis_pdctemb.get(pdctemb_key)
                #print("pdctemb_key_"+pdctemb_key);
                f1.writelines("pdctemb_key_"+pdctemb_key+":")
                
                if pdct_emb is not None:
                    mt1 = EmbeddingModel_pb2.Embeddings()
                    pdct_emb_parse = mt1.ParseFromString(pdct_emb)
                    pdct_emb_emb = mt1.embeddings
                    for spec in pdct_emb_emb:
                        pdct_emb_str_list = [str(hh) for hh in spec.embedding]
                        emb = np.array(spec.embedding)
                        all_pdct_emb.append(emb)
                        #print(emb)
                        f1.writelines(",".join(pdct_emb_str_list)+"\n")
                else:
                    #print(pdctemb_key+" is None")
                    f1.writelines("0,0,0,0,0,0,0,0\n")
                    all_pdct_emb.append(np.array([0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]))
        else:
            act_types = id_list
        cnt += 1

f.close()
f1.close()
