#!/bin/bash

# HDFS目录路径
HDFS_DIR="/home/ad_algo_g/users/xutianyu/user_cross_info_all_except_infered_user_id"
# 本地存储路径
LOCAL_DIR="/share_new/ad/yanghang07/sft_infer/infer_data"
# 每次获取的文件数量
BATCH_SIZE=30
# 批次范围
BATCH_START=$1
BATCH_END=$2

# 获取HDFS目录下的所有文件列表
file_list=$(hdfs dfs -ls "$HDFS_DIR" | awk 'NR>1 {print $8}')

# 创建本地存储目录
mkdir -p $LOCAL_DIR

# 遍历批次范围
for BATCH_NUMBER in $(seq $BATCH_START $BATCH_END); do
  # 初始化计数器
  count=0
  batch_start=$(( (BATCH_NUMBER - 1) * BATCH_SIZE ))
  batch_end=$(( BATCH_NUMBER * BATCH_SIZE ))

  # 创建当前批次的存储目录
  BATCH_DIR="$LOCAL_DIR/download_${BATCH_SIZE}_${BATCH_NUMBER}"
  mkdir -p $BATCH_DIR

  # 遍历文件列表
  for file in $file_list; do
    if [ -n "$file" ]; then
      # 检查当前文件是否在当前批次范围内
      if [ $count -ge $batch_start ] && [ $count -lt $batch_end ]; then
        # 获取文件到当前批次的存储目录
        hdfs dfs -get $file $BATCH_DIR
      fi

      # 增加计数器
      count=$((count + 1))

      # 如果已经获取到当前批次的所有文件，退出循环
      if [ $count -ge $batch_end ]; then
        python3 data_deal2.py -i $BATCH_DIR
	echo "Batch $BATCH_NUMBER: data prompt prepare finish"
	python3 multi_infer_with_log_sh.py -i"$BATCH_DIR/result.json" -o $BATCH_DIR
	echo "Batch $BATCH_NUMBER:llm infer finish"
        break
      fi
    fi
  done

  echo "Batch $BATCH_NUMBER: Retrieved files from $batch_start to $((count - 1)) and saved to $BATCH_DIR."
done
