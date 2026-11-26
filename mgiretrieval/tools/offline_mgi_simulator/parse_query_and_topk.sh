python parse_query_and_topk.py

directory=.
# 步骤4: 将结果载入 hive 表
hive_table=model_strategy_e.user_hist_pdct_0p1
local_file_path=${directory}/user_hist_pdct.txt
# 载入数据到 Hive 表的指定分区中
hive -e "LOAD DATA LOCAL INPATH '$local_file_path' INTO TABLE $hive_table"

hive_table=model_strategy_e.user_top_pdct_0p1
local_file_path=${directory}/user_top_pdct.txt
# 载入数据到 Hive 表的指定分区中
hive -e "LOAD DATA LOCAL INPATH '$local_file_path' INTO TABLE $hive_table"