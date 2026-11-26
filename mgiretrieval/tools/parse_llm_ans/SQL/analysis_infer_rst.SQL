WITH exploded_products AS (
  SELECT
    -- user_id,
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
),
in_products AS (
  SELECT
    COUNT(*) AS count,
    1 as flag
  FROM
    exploded_products
    JOIN total_product
    ON exploded_products.product_name = total_product.product_name
),
total_products AS (
  SELECT
    COUNT(*) AS count,
    2 as flag
  FROM
    exploded_products
)

select
count, flag
from in_products
union all
select
count, flag
from total_products