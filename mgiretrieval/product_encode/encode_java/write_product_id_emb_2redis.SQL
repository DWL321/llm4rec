set hive.exec.script.maxerrsize=10000000;
add file viewfs:///home/ad/ad_train/udf/libzstd.so.1.3.8;
add file viewfs:///home/ad/ad_train/udf/libgcc_s.so.1;
add file viewfs:///home/ad/ad_train/udf/libmkl_rt.so;
add file viewfs:///home/ad/ad_train/udf/extract_feature.flags;
add file viewfs:///home/ad/ad_train/udf/ip_region.dat;
add file viewfs:///home/ad/ad_train/udf/region_dict.dat;

add file viewfs:///home/ad_algo_feature/lijupan/interest_recall/dsp_feature_20231129;

add file viewfs://hadoop-lt-cluster/home/ad/lijupan/llm/udf/convArrFp2ArrStrV1.py;

with t1 as (
    select
        transform(photo_id, text_embedding) using 'python convArrFp2ArrStrV1.py' as photo_id, embs
    from model_strategy_e.outer_photo_dense_caption_text_embedding_daily
    where p_date = '20241013'
)

select
    transform(photo_id, embs)
    using './dsp_feature_20231129 -flagfile=extract_feature.flags -cmd=ExtractFeatureUPModelLtr -num_threads=1 -is_cal_seqlen=false -is_extract_upinf=false -is_u2uemb2redis=false -is_pdctcluster_emb2redis=true -pdct_emb_size=256 -up_cluster_name=adRecallResultInspector -pdct_emb_prefix=dense_cap'
    as (photo_id, photo_emb)
from t1


