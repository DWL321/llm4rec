set -xv
gpu_nums=100
# 输入文件
input_path=$1
input_file=${input_path##*/}
# 输出文件
output_file=$2
# 处理数据的脚本
code=$3
# 临时文件存储目录
tmp_dir=$4
# 最大长度
max_len=$5
# 数据组装后处理脚本
merge_post_process=$6

num=`wc -l $input_path|cut -d' ' -f1`
per_num=`expr $num / $gpu_nums + 1`
rm -rf $tmp_dir
mkdir $tmp_dir
split -l $per_num  -a 3 $input_path $tmp_dir/${input_file}.part -d
echo ">>>>max_len: $max_len"
echo ">>>>num: $num"

for i in `seq 1 $gpu_nums`
do
    i=`expr $i - 1`
    index=$i
    if [ $i -lt 10 ]
    then
        index=00$i
    elif [ $i -lt 100 ]
    then
        index=0$i
    else
        index=$i
    fi
    python $code  --input_data_dir $tmp_dir/${input_file}.part$index   --output_data_dir $tmp_dir/${input_file}.out${index} --model_max_length $max_len  --model_name_or_path /mmu_vcg_wjc_hdd/duyong/kuaiyi/tokenizer_configs/tokenizer.128k.data_ratio --tokenizer_version v1 &

done

wait

# 订制一个拼接函数吧
python $merge_post_process --input_file $input_path --tmp_dir $tmp_dir/ --output_file $output_file --tok_max_len $max_len --tokenizer_version v1

