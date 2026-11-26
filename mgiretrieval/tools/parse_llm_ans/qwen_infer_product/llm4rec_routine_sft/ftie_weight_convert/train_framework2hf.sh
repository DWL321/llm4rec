set -e
export PYTHONPATH=${PWD}/python:/root/Megatron-LM/:$PYTHONPATH
touch log_convert_train_framework2hf
export EVAL_LOG_PATH=log_convert_train_framework2hf

input_dir=$1
output_dir=$2
code_dir=$3
echo ">>>input_dir: $input_dir"
echo ">>>output_dir: $output_dir"
echo ">>>code_dir: $code_dir"

python $code_dir --model-type "kwaiyi_13b" \
                 --train-framework "megatron" \
                 --input-dir $input_dir \
                 --output-dir $output_dir
