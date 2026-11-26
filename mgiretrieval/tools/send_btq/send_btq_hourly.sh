#!/bin/bash

#python3 get_embedding.py -p 50009
# hive 表：/home/model_strategy_e/dw/model_strategy_e.db/outerloop_incr_imp_photo_hf

while true; do
    # 获取昨天的日期
    yesterday=$(date -d "yesterday" +%Y%m%d)
    # yesterday=20241014
    day_bf_yesterday=$(date -d "2 days ago" +%Y%m%d)
    echo curr_date:${yesterday}

    # 获取今天的日期，格式为 YYYY-MM-DD
    today=$(date +%Y%m%d)
    # 打印今天的日期
    echo "今天的日期是：$today"
    # 获取两个小时以前的时刻
    two_hours_ago=$(date --date='-2 hours' +'%H00')

    # 构建目录路径
    directory="photo_data/p_date=${today}/p_hourmin=${two_hours_ago}"
    day_bf_directory="photo_data/p_date=${today}/p_hourmin=${two_hours_ago}"
    
    success_file="/home/model_strategy_e/dw/model_strategy_e.db/outerloop_incr_imp_photo_hf/p_date=${today}/p_hourmin=${two_hours_ago}/_SUCCESS"
    echo success_file:${success_file}
    # 步骤1: 检测 HDFS 上的目录是否存在 _SUCCESS 文件
    hdfs dfs -test -e ${success_file} 
    if [ $? -eq 0 ]; then
        if [ -f "${directory}/_SUCCESS" ]; then
            echo "${directory}/_SUCCESS alrealy exists. continue"
            sleep 1800
            continue
        else
            # 步骤1: 创建本地目录
            if [ -d "$directory" ]; then
                # 目录存在
                echo "Directory $directory exists."
            else
                # 目录不存在，创建目录
                echo "Directory $directory does not exist. Creating..."
                mkdir -p "$directory"
            fi

            # 步骤2: 从 HDFS 拉取 product_result.json 至本地执行目录
            hdfs dfs -get "/home/model_strategy_e/dw/model_strategy_e.db/outerloop_incr_imp_photo_hf/p_date=${today}/p_hourmin=${two_hours_ago}/*" ${directory}/ 
    
            # 步骤3: 执行 Python 脚本
            python send_btq.py -i ${directory} -b dsp_lps_ljp_casQv1
    
            echo "Generating embedding success! p_date=${yesterday}. Checking again in 60 minutes."
            sleep 3600
        fi
    else
        echo "The file $success_file does not exist. Checking again in 30 minutes."
        # 文件不存在，等待一段时间后再次检查
        sleep 1800 
    fi
done
