#!/bin/bash

while true; do
    # 获取昨天的日期
    yesterday=$(date -d "yesterday" +%Y%m%d)
    # yesterday=20241014
    day_bf_yesterday=$(date -d "2 days ago" +%Y%m%d)
    echo curr_date:${yesterday}
    
    # 构建目录路径
    directory="dataset/p_date=${yesterday}"
    day_bf_directory="dataset/p_date=${day_bf_yesterday}"
    
    success_file="/home/model_strategy_e/dw/model_strategy_e.db/outerloop_daily_increacing_product/p_date=${yesterday}/_SUCCESS"
    echo success_file:${success_file}
    # 步骤1: 检测 HDFS 上的目录是否存在 _SUCCESS 文件
    hdfs dfs -test -e ${success_file} 
    if [ $? -eq 0 ]; then
        if [ -f "${directory}/product_java_hash.txt" ]; then
            echo "${directory}/product_java_hash.txt alrealy exists. continue"
            sleep 1800
            continue
        else
            # 步骤1: 创建本地目录
            if [ -d "$directory" ]; then
                # 目录存在，删除目录
                echo "Directory $directory exists."
            else
                # 目录不存在，创建目录
                echo "Directory $directory does not exist. Creating..."
                mkdir -p "$directory"
            fi

            # 步骤2: 从 HDFS 拉取 product_result.json 至本地执行目录
            hdfs dfs -cat "/home/model_strategy_e/dw/model_strategy_e.db/outerloop_daily_increacing_product/p_date=${yesterday}/*" > ${directory}/daily_inc_product_name       
            python extract_product.py ${directory}/daily_inc_product_name > ${directory}/daily_inc_product_name_format
            sh run.sh ${directory}/daily_inc_product_name_format > ${directory}/product_java_hash.txt

            cat ${directory}/product_java_hash.txt | tr ':' '\t' > ${directory}/product_java_hash_tab_sep.txt
    
            # 步骤4: 将结果载入 hive 表
            hive_table=model_strategy_e.java_hash_product_name_and_id_inc_daily
            local_file_path=${directory}/product_java_hash_tab_sep.txt
            # 载入数据到 Hive 表的指定分区中
            hive -e "LOAD DATA LOCAL INPATH '$local_file_path' INTO TABLE $hive_table PARTITION (p_date='${yesterday}')"
            
            # 删除本地文件（如果不再需要）
            # rm -f "$local_file_path"
            echo "Generating embedding success! p_date=${yesterday}. Checking again in 60 minutes."
            sleep 3600
        fi
    else
        echo "The file $success_file does not exist. Checking again in 30 minutes."
        # 文件不存在，等待一段时间后再次检查
        sleep 1800 
    fi
done