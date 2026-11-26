#!/bin/bash

#python3 get_embedding.py -p 50009

while true; do
    # 获取昨天的日期
    yesterday=$(date -d "yesterday" +%Y%m%d)
    # yesterday=20241007
    day_bf_yesterday=$(date -d "2 days ago" +%Y%m%d)
    echo curr_date:${yesterday}
    today=$(date +%Y%m%d)
    echo "今天的日期是：$today"
    # 获取两个小时以前的时刻
    one_hours_ago=$(date --date='-1 hours' +'%H00')

    # 构建目录路径
    directory="caption_data/p_date=${today}/p_hourmin=${one_hours_ago}/"
    success_file=/home/ad/lijupan/llm/realtime_dense_caption/photo_dense_caption_${today}${one_hours_ago}.txt
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
                # 目录存在，删除目录
                echo "Directory $directory exists."
            else
                # 目录不存在，创建目录
                echo "Directory $directory does not exist. Creating..."
                mkdir -p "$directory"
            fi

            # 步骤2: 从 HDFS 拉取 product_result.json 至本地执行目录
            hdfs dfs -get ${success_file} ${directory}/ 
    
            # 步骤3: 执行 Python 脚本
            python3 client_caption.py -i ${directory}/photo_dense_caption_${today}${one_hours_ago}.txt -o ${directory}/photo_dense_caption_result_embedding.txt
            touch _SUCCESS
            # 步骤4: 将结果载入 hive 表
            hive_table=model_strategy_e.outerloop_dense_caption_emb_hourly_inc
            local_file_path=${directory}/photo_dense_caption_result_embedding.txt
            # 载入数据到 Hive 表的指定分区中
            hive -e "LOAD DATA LOCAL INPATH '$local_file_path' INTO TABLE $hive_table PARTITION (p_date='${today}', p_hourmin='${one_hours_ago}')"
            
            hdfs dfs -put _SUCCESS /home/model_strategy_e/dw/model_strategy_e.db/outerloop_dense_caption_emb_hourly_inc/p_date=${today}/p_hourmin=${one_hours_ago}/
            # 删除本地文件（如果不再需要）
            rm -f $local_file_path
            echo "Generating embedding success! p_date=${today}, p_hourmin=${one_hours_ago}. Checking again in 10 minutes."
            sleep 600
        fi
    else
        echo "The file $success_file does not exist. Checking again in 10 minutes."
        # 文件不存在，等待一段时间后再次检查
        sleep 1800 
    fi
done