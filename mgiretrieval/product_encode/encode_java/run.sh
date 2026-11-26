#!/bin/bash

input_file=$1

# 检查文件是否存在
if [ ! -f "$1" ]; then
    echo "文件 $1 不存在。"
    exit 1
fi

# 读取文件的每一行，去掉首尾空白字符，并打印
while IFS= read -r line
do
    java -cp . ProductHashCode "${line%$'\n'}" 
done < "$1" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//'  # 去掉首尾空白字符
