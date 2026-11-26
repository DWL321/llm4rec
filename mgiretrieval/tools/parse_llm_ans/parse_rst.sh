input_dir=llm_data/cross_json/
output_dir=llm_data/cross_json_parse/

# 步骤1: 创建本地目录
if [ -d "$output_dir" ]; then
    # 目录存在
    echo "Directory $output_dir exists."
else
    # 目录不存在，创建目录
    echo "Directory $output_dir does not exist. Creating..."
    mkdir -p "$output_dir"
fi

python parse_rst.py -i ${input_dir} -o ${output_dir}

# 步骤4: 将结果载入 hive 表
yesterday=$(date -d "yesterday" +%Y%m%d)
hive_table=model_strategy_e.cross_sft_outerloop_user_infer_rst_daily
local_file_path=${$output_dir}
# 载入数据到 Hive 表的指定分区中
hive -e "LOAD DATA LOCAL INPATH '$local_file_path' INTO TABLE $hive_table PARTITION (p_date='${yesterday}')"