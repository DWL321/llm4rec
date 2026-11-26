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
    
    # 步骤3: 执行 Python 脚本
    python send_btq.py -i ${directory} -b dsp_lps_ljp_casQv1
done
