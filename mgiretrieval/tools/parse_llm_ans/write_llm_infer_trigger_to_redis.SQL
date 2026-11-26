set spark.sql.files.ignoreCorruptFiles=true;
set spark.sql.files.ignoreMissingFiles=true;
set spark.hadoop.dfs.ignore.block.missing.in.decompress=true;
SET mapreduce.map.failures.maxpercent=5;
SET mapreduce.reduce.failures.maxpercent=5;
SET dfs.ignore.block.missing.in.decompress=true;
set hive.support.concurrency=false;
set mapreduce.map.memory.mb=4432;
set mapreduce.reduce.memory.mb=4432;
set hive.exec.script.maxerrsize=10000000;
add file viewfs:///home/ad/ad_train/udf/libzstd.so.1.3.8;
add file viewfs:///home/ad/ad_train/udf/libgcc_s.so.1;
add file viewfs:///home/ad/ad_train/udf/libmkl_rt.so;
add file viewfs:///home/ad/ad_train/udf/extract_feature.flags;
add file viewfs:///home/ad/ad_train/udf/ip_region.dat;
add file viewfs:///home/ad/ad_train/udf/region_dict.dat;

add file viewfs:///home/ad_algo_feature/lijupan/interest_recall/dsp_feature_20231129;
add file viewfs:///home/ad_algo_feature/lijupan/interest_recall/product_feat.txt;

create table if not exists model_strategy_e.write_llm_infer_trigger_to_redis (
    visitor_id bigint,
    pdct_seq string
) partitioned by (p_date string)
stored as textfile;
insert overwrite table model_strategy_e.write_llm_infer_trigger_to_redis partition(p_date = '{{ ds_nodash }}')

select
    transform(user_id, target_product_names)
    using './dsp_feature_20231129 -flagfile=extract_feature.flags -cmd=ExtractFeatureUPModelLtr -num_threads=10 -is_cal_seqlen=false -is_extract_upinf=false -pdct_feat_num=7 -up_cluster_name=adRecallResultInspector -pdct_feat_file=product_feat.txt -histact_pdct_prefix=userLLMCrossSftTrigger -is_check_write=false -is_rm_click_ped=false -is_dedup_pdct=false -is_pdctseq2redis=true'
    as (visitor_id, pdct_seq)
from model_strategy_e.cross_sft_outerloop_user_infer_rst_daily
where
    p_date='{{ ds_nodash }}'