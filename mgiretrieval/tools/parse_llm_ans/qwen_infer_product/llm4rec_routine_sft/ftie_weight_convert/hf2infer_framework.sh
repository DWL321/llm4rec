set -e
export PYTHONPATH=${PWD}/python:$PYTHONPATH
touch log_convert_hf2infer_framework
export EVAL_LOG_PATH=log_convert_hf2infer_framework

# docker image: registry.corp.kuaishou.com/kml/llm-chat-service:a800-v0.2.0-202309082038

#### nvidia
input_dir=$1
output_dir=$2

python $3 --model-type "kwaiyi_13b" \
          --input-dir $input_dir \
          --output-dir $output_dir \
          --infer-framework "ftie_nv" \
          --target-weight-dtype "bf16" \
          --target-tp-num 1 \
          --target-pp-num 1 \
          --process-thread-num 64 \
          --int8-mode 5 \
          --group-size 128

echo "hf2infer_framework done!"
