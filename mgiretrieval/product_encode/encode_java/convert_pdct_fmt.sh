#!/bin/bash

# 确保提供了文件名作为参数
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <filename>"
    exit 1
fi

# 检查文件是否存在
if [ ! -f "$1" ]; then
    echo "File not found: $1"
    exit 1
fi

# 使用awk提取product对应的字符串
awk -F'[{}]' '{print $2}' | sed 's/"product": "//g' | sed 's/"//g' "$1"