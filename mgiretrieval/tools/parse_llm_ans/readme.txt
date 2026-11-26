跨域SFT推理返回结果：/home/mmu/duyong/llm4rec/outer_loop/cross_json
数据格式：
{"user_id": "3839639693", "product_names": ["逆袭一刻", "开局太难了", "吃不到神鲲吧", "找茬大师傅", "脑力大咖秀", "疯狂飞刀转", "一决胜负", "超能防���", "鲲之召唤", "迷你闯关"]}

# 创建存储结果的 hive 表
create table if not exists model_strategy_e.cross_sft_outerloop_user_infer_rst_daily (
    user_id bigint,
    target_product_names string
) partitioned by (p_date string)
stored as textfile
TBLPROPERTIES (
  'kuaishou.dp-mdc.lifecycle.day' = '7'
);

# 将推理结果写入 hive 表
见 write_llm_infer_trigger_to_redis.SQL

# 分析推理结果中的 product_name 位于当前外循环 product 中的比例
in:4481
total:14349
in_ratio = in / total = 31.23%

# 由于 trigger 的覆盖度仅为 31.23%，需要研究使用 qwen 推理 product 描述的方法
/share/lijupan/llm4rec_routing/code/llm4rec_routine_sft/product2info_outerloop.py

提供的数据格式：{"product": "飞雁传书APP快应用应用app程序"}
准备一个 hive 表，存储未覆盖待推理的 product 们


