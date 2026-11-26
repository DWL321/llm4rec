#!/bin/bash

ts=`date +%Y_%m_%d_%H_%M`
VOCAB_SIZE=128000
ALLOC_CONF=1
RECOMPUTE=1
FULL_CKPT=0
OFFLOAD_ALPHA=0

if [ $ALLOC_CONF == "1" ]; then
    ALLOC_CONF_ARGS="-x PYTORCH_CUDA_ALLOC_CONF=max_split_size_mb:21"
fi

if [ $RECOMPUTE == "1" ]; then
    RECOMPUTE_ARGS="--kaimm-recompute-mlp-activation-func --kaimm-recompute-norm"
fi
if [ $FULL_CKPT == "1" ]; then
    FULL_CKPT_ARGS="--recompute-granularity full --recompute-method uniform --recompute-num-layers 1"
    [ $SYNC_LEVEL -le 2 ] || SYNC_LEVEL=2
fi

TP=4
PP=4
SP='--sequence-parallel'
HIDDEN_SIZE=5120
FFN_HIDDEN_SIZE=13824
LAYERS=40
NUM_ATTENTION_HEADS=40

SEQ_LENGTH=8192
MICRO_BATCH=1
GLOBAL_BATCH=1024
SYNC_LEVEL=3
[ $SEQ_LENGTH -le 8192 ] && SYNC_LEVEL=2
[ $SEQ_LENGTH -le 4096 ] && SYNC_LEVEL=0
M_NAME=llama13b
num_epoch=1
# 请注意：alibi_max=12!!!
alibi_max=12

base_path=$1
exp_name=$2
run_name=$3
tensorboard_path=$4

OUTPUT_PATH="${base_path}/${exp_name}/${run_name}/"
CHECKPOINT_PATH=${OUTPUT_PATH}/save
TENSORBOARD_DIR="${tensorboard_path}/${exp_name}/${run_name}/"


TRAIN_DATA_PATH=$5

mkdir -p ${CHECKPOINT_PATH}
mkdir -p ${OUTPUT_PATH}/log

LOG_PATH=${OUTPUT_PATH}/log

PRETRAIN_MODEL=$6

ln -s ${PRETRAIN_MODEL} ${OUTPUT_PATH}/save/iter_0000000

if [ -f ${OUTPUT_PATH}/save/latest_checkpointed_iteration.txt ]; then
   echo "文件存在"
else
    echo 0 > ${OUTPUT_PATH}/save/latest_checkpointed_iteration.txt
fi

TOKENIZER=/mmu_vcg_wjc_hdd/duyong/kuaiyi/tokenizer_configs/tokenizer.128k.data_ratio

# ! 计算并获取相关结果
TOTAL_STEP=$(python train_data_info.py "$TRAIN_DATA_PATH"_sample_index.bin "$num_epoch" "$GLOBAL_BATCH")
echo "Tatol steps number: $TOTAL_STEP"

save_interval=$((TOTAL_STEP / num_epoch))  # 默认每个epoch保存一次
echo $save_interval
echo $save_interval > num_steps_for_epoch11
echo $((save_interval + save_interval)) > num_steps_for_epoch21
echo $TOTAL_STEP > num_steps_for_epoch31
LR=1e-6


echo -e "$LR\n$SEQ_LENGTH\n$TRAIN_DATA_PATH\n$PRETRAIN_MODEL" > train_config_info_for_script
uri_path='mluri'
save_dir=$CHECKPOINT_PATH
echo -e "$uri_path\n$exp_name\n$run_name\n$save_dir" > mlflow_config_info_for_script

ML_FLOW_ARGS="
    --exp_name $exp_name
    --run_name $run_name
"

GPT_ARGS="
	--tensor-model-parallel-size $TP \
	--pipeline-model-parallel-size $PP \
    ${SP} \
    --use-distributed-optimizer \
    --optimizer adam \
    --num-layers $LAYERS \
    --hidden-size $HIDDEN_SIZE \
    --ffn-hidden-size $FFN_HIDDEN_SIZE \
    --num-attention-heads $NUM_ATTENTION_HEADS \
    --seq-length $SEQ_LENGTH \
    --max-position-embeddings $SEQ_LENGTH \
    --micro-batch-size $MICRO_BATCH \
    --global-batch-size $GLOBAL_BATCH \
    --train-iters $TOTAL_STEP \
    --lr-warmup-iters 10 \
    --lr $LR \
    --min-lr 0 \
    --warmup-init-lr 0 \
    --lr-decay-style cosine \
    --weight-decay 0 \
    --start-weight-decay 0 \
    --end-weight-decay 0 \
    --weight-decay-incr-style linear \
    --clip-grad 1.0 \
    --adam-beta1 0.9 \
    --adam-beta2 0.95 \
    --adam-eps 1.e-8 \
    --bf16 \
    --use-flash-attn \
    --use-alibi \
    --seed 9527 \
    --no-load-rng \
    --init-method-std 0.02 \
    --num-layers-per-virtual-pipeline-stage 2 \
    --overlap-p2p-communication \
"

DATA_ARGS="
    --sft-dataset \
    --train-data-path $TRAIN_DATA_PATH \
    --data-impl mmap \
    --tokenizer-model $TOKENIZER \
    --ignore-index -100 \
    --dataloader-type cyclic \
    --variable-seq-lengths \
    --sft-padding \
"


OUTPUT_ARGS="
    --log-interval 3 \
    --save-interval $save_interval  \
    --eval-interval 10000 \
    --eval-iters 0 \
    --timing-log-level 0 \
    --tensorboard-dir $TENSORBOARD_DIR \
    --tensorboard-log-interval 1
"
# --tensorboard-dir $TENSORBOARD_DIR \
# --tensorboard-log-interval 1 \
# --log-timers-to-tensorboard \
# --log-batch-size-to-tensorboard \
# --log-memory-to-tensorboard \
# --log-world-size-to-tensorboard \
# --log-validation-ppl-to-tensorboard \
STABILITY_ARGS="
    --loss_median_window 200 \
    --stability_protection \
    --anomaly_times 2 \
    --skip_steps 200 \
    --consecutive_anomalies_steps 10 \
"
# llama
LLAMA_ARGS="
    --rms-norm \
    --swiglu \
    --prefetch-factor 256 \
    --hidden-dropout 0. \
    --attention-dropout 0. \
    --no-bias-dropout-fusion \
    --untie-embeddings-and-output-weights \
    --kaimm-async-dataloader \
    --disable-bias-linear \
    --no-position-embedding \
    --accumulate-allreduce-grads-in-fp32 \
    --kaimm-overlap-optimizer-communication \
    --kaimm-overlap-reduce-ratio 0.125 \
    --kaimm-overlap-gather-ratio 0.125 \
    --kaimm-overlap-optimizer-slow-ctas 2 \
    --kaimm-cuda-synchronize-level $SYNC_LEVEL \
    --kaimm-offload-activation-ratio $OFFLOAD_ALPHA \
    --kaimm-gc-interval 8 \
    --no-masked-softmax-fusion \
    --kaimm-overlap-optimizer-no-barrier \
    --kaimm-overlap-cp-slow-ctas 4\
    --use-fast-rms-norm \
    --alibi-bias-max $alibi_max
"

    # --kaimm-overlap-optimizer-communication \
    # --kaimm-overlap-reduce-ratio 0.125 \
    # --kaimm-overlap-gather-ratio 0.125 \
    # --kaimm-overlap-optimizer-slow-ctas 2 \

YOUR_MEGATRON_PATH=/mmu_vcg_wjc_hdd/duyong/megatron/Megatron-LM_13b

hostfile=/etc/mpi/hostfile_seq
Port=$(cat /etc/ssh/ssh_config | grep 'Port' | cut -d'"' -f2)
np=$(cat $hostfile | cut -d'=' -f2 | awk '{sum += $0} END {print sum}')
set -x
mpirun --allow-run-as-root -np $np \
        -mca plm_rsh_args "-F $TARGET_IP_PORT_FILE" \
        -hostfile $hostfile \
        -x HOROVOD_MPI_THREADS_DISABLE=1 \
        -x MPI_THREAD_SINGLE=1 \
        -x CUDA_DEVICE_MAX_CONNECTIONS=1 \
        -mca btl self,tcp \
        -mca pml ob1 \
        -mca plm_rsh_num_concurrent 600 \
        -mca routed_radix 600 \
        -mca btl_if_include bond0,eth01 \
        -mca oob_tcp_if_include bond0,eth01 \
        -mca btl_openib_allow_ib false \
        -x PYTHONPATH=${YOUR_MEGATRON_PATH} \
        -x NCCL_IB_QPS_PER_CONNECTION=8 \
        -x NCCL_IB_DISABLE=0 \
        -x NCCL_IB_GID_INDEX=3 \
        -x NCCL_IB_HCA=mlx5 \
        -x NCCL_DEBUG=WARN \
        -x LD_LIBRARY_PATH -x PATH \
        -x UB_AG_SM_MARGIN=0 \
        -x UB_RS_SM_MARGIN=1 \
        -x UB_MAX_STREAMS=2 \
        -x NVTE_FWD_LAYERNORM_SM_MARGIN=8 \
        -x NVTE_BWD_LAYERNORM_SM_MARGIN=8 \
        -x NCCL_IB_TIMEOUT=22 \
        -x TORCH_NCCL_AVOID_RECORD_STREAMS=1 \
        $ALLOC_CONF_ARGS \
        -x NCCL_NET_OVERHEAD=1 \
        -x LD_PRELOAD=/mmu_vcg_wjc_hdd/duyong/kuaiyi/tokenizer_configs/libnccl.so.2.19.3:$LD_PRELOAD \
        python ./sft_llama_13B_easy_config.py \
            --save_path ${save_dir} \
            --experiment_name ${exp_name} \
            --run_name ${run_name} \
            $GPT_ARGS \
            $DATA_ARGS \
            $OUTPUT_ARGS \
            $LLAMA_ARGS \
            $STABILITY_ARGS \
            $RECOMPUTE_ARGS \
            $FULL_CKPT_ARGS \
            --load ${CHECKPOINT_PATH}  \
            --save ${CHECKPOINT_PATH} \
            --distributed-backend nccl \
            --make-vocab-size-divisible-by 128 \
            --master-addr ${MY_NODE_IP}:8389 2>&1 | tee $LOG_PATH/llama_$ts.log

exit_code=${PIPESTATUS[0]}  # capture the exit code of mpirun
exit $exit_code
# 这把改得应该差不多.
