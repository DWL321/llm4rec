drop table model_strategy_e.outerloop_dense_caption_emb_hourly_inc;
create table if not exists model_strategy_e.outerloop_dense_caption_emb_hourly_inc 
(`photo_id` bigint, `embs` array<float>) 
PARTITIONED BY (`p_date` string, `p_hourmin` string)
row FORMAT SERDE 'org.apache.hadoop.hive.serde2.lazy.LazySimpleSerDe'
with SERDEPROPERTIES (
    'colelction.delim' = ',',
    'field.delim' = '\t',
    'serialization.format' = '\t'
) STORED as INPUTFORMAT 'org.apache.hadoop.mapred.TextInputFormat' OUTPUTFORMAT 'org.apache.hadoop.hive.ql.io.HiveIgnoreKeyTextOutputFormat'
tblproperties('kuaishou.dp-mdc.lifecycle.day' = '7');
