export HADOOP_USER_NAME=ad
directory=caption_data/
# 步骤1: 创建本地目录
if [ -d "$directory" ]; then
    # 目录存在
    echo "Directory $directory exists."
else
    # 目录不存在，创建目录
    echo "Directory $directory does not exist. Creating..."
    mkdir -p "$directory"
fi
python btq_process.py -o ${directory} -b dsp_lps_ljp_casQ