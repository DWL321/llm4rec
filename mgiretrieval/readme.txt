# 编译双塔服务
export USE_TF_1_15=TRUE
export CUDA_VERSION=11.8
export TF_BUILD_TYPE=GPU
export ENABLE_JEMALLOC=true
export USE_TF_15=TRUE
export ENABLE_AD_BS_BOTH=True
export TENSORRT_VERSION=9
export COMMON_RECO_LEAF_EXTRA_PROCESSORS=ann_client cofea gsu kkd kuiba news nr push related relation_photo retrieval rfm slide web yuzhou mem_retrieval embed_calc
export BUILD_AD_WITH_DRAGON=TRUE
export FEATURE_DEBUG_INFO_LEVEL=1
export DEBUG_TYPE_SECTION=true
export ENABLE_GCC10_3=true
export ENABLE_GCC_LLD=true
kbuild build teams/ad/ad_nn/twin_towers_ps --base_tag=HEAD_PB319 2>&1 | tee build_log


# 编译 producer 
export USE_TF_15=TRUE
export USE_TF_1_15=TRUE
export CUDA_VERSION=11.4
export TF_BUILD_TYPE=CPU
export ENABLE_AD_BS_BOTH=TRUE
export BUILD_AD_WITH_DRAGON=true
export COMMON_RECO_LEAF_EXTRA_PROCESSORS=ann_client cofea gsu kkd kuiba news nr push related relation_photo retrieval rfm slide web yuzhou mem_retrieval embed_calc
export FEATURE_DEBUG_INFO_LEVEL=0
export DEBUG_TYPE_SECTION=true
export ENABLE_JEMALLOC=true
export ENABLE_GCC10_3=true
export ENABLE_GCC_LLD=true

kbuild build teams/ad/ad_nn/embedding_producer --base_tag=HEAD_PB319 2>&1 | tee build_log
