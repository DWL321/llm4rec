CREATE TABLE IF NOT EXISTS model_strategy_e.outerloop_daily_increacing_product (
  `product_name` STRING COMMENT '产品名'
) COMMENT '外循环天级别新增曝光产品名表'
partitioned by (p_date string)
ROW FORMAT DELIMITED
FIELDS TERMINATED BY '\t'
LINES TERMINATED BY '\n'
STORED AS TEXTFILE
TBLPROPERTIES (
  'kuaishou.dp-mdc.lifecycle.day' = '7'
);

with t1 as (
select
    product_name
from model_strategy_e.outerloop_all_imp_product_name
where p_date = '{{ds_nodash}}'
and (not (product_name REGEXP '\\\\'))
),
t2 as(
select
    product_name
from model_strategy_e.outerloop_all_imp_product_name
where p_date = '{{ds_nodash - 1}}'
),
exploded_products AS (
  SELECT
    distinct product_name
  FROM model_strategy_e.cross_sft_outerloop_user_infer_rst_daily
    LATERAL VIEW explode(split(target_product_names, ',')) exploded_table AS product_name
    WHERE p_date = '20241114'
),
total_product as (
  select 
    distinct product_name
  from model_strategy_e.outerloop_llm_product_emb_all_daily
  where p_date = '20241114'
)

INSERT OVERWRITE TABLE model_strategy_e.outerloop_daily_increacing_product partition(p_date = '{{ ds_nodash }}')
SELECT concat('{"product": "', t1.product_name, '"}') as product_name
FROM t1
LEFT OUTER JOIN t2 ON t1.product_name = t2.product_name
WHERE  t2.product_name IS NULL

union all

SELECT
    concat('{"product": "', exploded_products.product_name, '"}') as product_name
FROM exploded_products
left JOIN total_product
ON exploded_products.product_name = total_product.product_name
where total_product.product_name is null