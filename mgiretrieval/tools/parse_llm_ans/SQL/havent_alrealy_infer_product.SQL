
CREATE TABLE IF NOT EXISTS model_strategy_e.havent_alrealy_infer_product (
  `product_name` STRING COMMENT '产品名'
) COMMENT '跨域SFT结果中未推理的产品名'
partitioned by (p_date string)
ROW FORMAT DELIMITED
FIELDS TERMINATED BY '\t'
LINES TERMINATED BY '\n'
STORED AS TEXTFILE
TBLPROPERTIES (
  'kuaishou.dp-mdc.lifecycle.day' = '7'
);

WITH exploded_products AS (
  SELECT
    distinct product_name
  FROM model_strategy_e.cross_sft_outerloop_user_infer_rst_daily
    LATERAL VIEW explode(split(target_product_names, ',')) exploded_table AS product_name
    WHERE p_date = '{{ ds_nodash }}'
    and (not (product_name REGEXP '\\\\'))
),
total_product as (
  select 
    distinct product_name
  from model_strategy_e.outerloop_llm_product_emb_all_daily
  where p_date = '{{ ds_nodash }}'
)
insert overwrite table model_strategy_e.havent_alrealy_infer_product partition(p_date = '{{ ds_nodash }}')
SELECT
  concat('{"product": "', exploded_products.product_name, '"}') as product_name
FROM exploded_products
left JOIN total_product
ON exploded_products.product_name = total_product.product_name
where total_product.product_name is null