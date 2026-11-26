# 创建表格

drop table model_strategy_e.outerloop_dense_caption_emb_hourly_inc;
create table if not exists model_strategy_e.outerloop_dense_caption_emb_hourly_inc (
    photo_id bigint,
    caption_emb array<float>
) partitioned by (p_date string, p_hourmin string)
ROW FORMAT DELIMITED
FIELDS TERMINATED BY '\t'
LINES TERMINATED BY '\n'
STORED AS TEXTFILE
TBLPROPERTIES (
  'kuaishou.dp-mdc.lifecycle.day' = '7'
);