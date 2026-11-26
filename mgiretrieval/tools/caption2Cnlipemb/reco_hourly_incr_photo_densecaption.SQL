CREATE TABLE IF NOT EXISTS model_strategy_e.reco_hourly_incr_photo_densecaption (
    `photo_id` BIGINT COMMENT '视频ID',
    `dense_caption` STRING COMMENT '视频描述'
) COMMENT '外循环小时级新增推理视频描述'
partitioned by (p_date string, p_hourmin string)
ROW FORMAT DELIMITED
FIELDS TERMINATED BY '\t'
LINES TERMINATED BY '\n'
STORED AS TEXTFILE
TBLPROPERTIES (
  'kuaishou.dp-mdc.lifecycle.day' = '7'
);

with t1 as (
  select
    photo_id, dense_caption
  from reco.ad_outer_dense_caption_bgem3_emb_incre_hourly
  where p_date > '20241114'
),
t2 as (
  select
    distinctphoto_id
  from model_strategy_e.outerloop_dense_caption_emb_hourly_inc
  where p_date > '20241114'
)

insert overwrite table model_strategy_e.reco_hourly_incr_photo_densecaption partition(p_date = '{{ ds_nodash }}', p_hourmin = '{{ hourmin }}')
select
  t1.photo_id as photo_id,
  t1.dense_caption as dense_caption
from t1
left join t2 on t1.photo_id = t2.photo_id
where t2.photo_id is null