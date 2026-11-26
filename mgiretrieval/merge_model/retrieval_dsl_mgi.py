#!/usr/bin/env python3
# coding=utf-8

import sys, copy, os
import json

### 1. Config Dragon Path
dragon_path = '/home/root/a/dragon'
env_path = os.getenv('DRAGON_PATH')
if env_path is not None:
    dragon_path = env_path
if not os.path.exists(dragon_path):
    print("Dragon path not exists, please check env DRAGON_PATH")
sys.path.append(dragon_path)
sys.path.append('..')

from dragonfly.common_leaf_dsl import LeafService, LeafFlow
from tools import *

### 2. Dragon config.
# Change CMD.
cmd = '/ad/dsp/retrieval:ljp_lcron_ltr_mgillmpdct_real_act_v1'
service_meta = ServiceMeta(cmd)
flow = AdFlowBase(name='twin_towers_retrieval_flow')
# Change Processors.
flow.ad_twin_towers_initializer(
    load_model=True, skip=True).twin_towers_prepare_context(
        input_common_attrs=['request'],
        recall_type=1,
        enable_target_filter=False,
        enable_region_filter=False).twin_towers_result_from_cache(
            result_cache_ttl=80,
            result_cache_capacity=150000,
            result_cache_max_num=30000,
            result_count=2,
            return_score=False,
            result_cache_keys=''
        ).if_('hit_cache == 1').twin_towers_handle_result(
            output_common_attrs=['response']).else_().twin_towers_trigger_init(
                trigger_type="redis_interest_by_material").construct_bslog(
                ).extract_bs_feature(
                    enable_remote_feature_validate=False).construct_dnn_input(
                    ).calculate_embedding().ad_twin_towers_retrieval(
                        return_score=False,
                        result_count=2).twin_towers_handle_result(
                            output_common_attrs=['response']).end_if_()

### 3. Original kconf config.
# Config from old kconf, should move these to dragon dsl with reco kai-serving.
# Ignore it if you are using kconf.
kconf_dict = {'max_batch_size': 24}

if __name__ == '__main__':
    config_dict = {}
    # Merge user info cache config, MUST NOT add config to it.
    merge_json(config_dict, 'expired_config.json')

    service = LeafService(
        kess_name=service_meta.kess_name,
        common_attrs_from_request=["request"],
    )
    service.return_common_attrs(['response'])
    service.add_leaf_flows(request_type="twin_towers",
                           leaf_flows=[flow],
                           as_default=True)

    filename = os.path.basename(__file__).replace(".py", "") + ".json"
    service.build(output_file=os.path.join('./', filename))

    merge_json(config_dict, filename)
    config_dict.update(kconf_dict)

    with open(filename, 'w') as f:
        json.dump(config_dict,
                  f,
                  indent=2,
                  sort_keys=True,
                  allow_nan=False,
                  ensure_ascii=False)
