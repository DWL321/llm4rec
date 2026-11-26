import os

os.environ['BUILD_AD_WITH_DRAGON'] = 'TRUE'
os.environ['ENABLE_KUIBA'] = 'TRUE'

use_tf_15 = (os.getenv("USE_TF_1_15", 'FALSE') == 'TRUE')
tf_lib = '//third_party/tensorflow/BUILD:tensorflow_cc'
if use_tf_15:
  tf_lib = '//third_party/tensorflow-1.15/BUILD:tensorflow_cc'

build_type = os.getenv('TF_BUILD_TYPE', 'CPU')
if build_type == 'GPU':
  tf_lib = '//third_party/tensorflow-1.15/BUILD:tensorflow_cc_gpu'

cc_library(name = "cmd_base",
  srcs = ["cmd_base.cc", "common.cc", "kafka_cmd.cc"],
  excludes = [],
  cppflags = [
          "-D GOOGLE_LOGGING=1",
          "-D DMLC_USE_GLOG",
          ],
  deps = [
        "//base/strings/BUILD:strings",
        "//ks/util/BUILD:util",
        "//teams/ad/ad_base/src/kafka/BUILD:kafka",
        "//teams/ad/ad_nn/feature_extract/BUILD:ad_extract_framework",
        "//teams/ad/ad_nn/flatten_raw_feature/BUILD:flatten_helper",
        ] + [tf_lib]
)
# dragon pipeline 对应配置
dragon_deps = []
dragon_flag = []

build_with_dragon = os.getenv("BUILD_AD_WITH_DRAGON", 'TRUE').lower() in ['true', '1']
build_ad_offline = os.getenv('BUILD_AD_OFFLINE', 'FALSE').lower() in ['true', '1']

if build_with_dragon:
  dragon_flag = ["-D BUILD_WITH_DRAGON"]
  dragon_deps = [
    "//dragon/src/BUILD:common_reco_pipeline_executor",
  ]

  if not build_ad_offline:
    dragon_exclude_processors = [
        'merchant', 'tdm', 'pdn', # gpu_retr 与 twin_tower 冲突
        'snack_predict', 'snack', 'oversea', 'oversea_lr', 'partlive', # 海外 pb 冲突
        'subdivision', 'ks_mix_rank',  # reco_proto 中 copy 的 ad_proto 部分冲突
        'tf_serving',  # tf 版本冲突
        'mmu', 'eyeshot', # 使用了 search 代码
        'nearby', "video_cold_start",  # link error,临时关闭
    ]
    os.environ['COMMON_RECO_LEAF_EXCLUDE_PROCESSORS'] = ' '.join(dragon_exclude_processors)
    default_extra_processors = [ # start - end 标识的区域拷贝自 dragon/src/BUILD 的同名配置
        "ann_client", # start
        "cofea",
        "video_cold_start",
        "eyeshot",
        "gsu",
        "nearby",
        "kkd",
        "ks_mix_rank",
        "kuiba",
        "livestream",
        "merchant",
        "mmu",
        "news",
        "nr",
        "oversea_lr",
        "oversea",
        "partlive",
        "pdn",
        "push",
        "related",
        "relation_photo",
        "retrieval",
        "rfm",
        "slide",
        "snack_predict",
        "snack",
        "subdivision",
        "tdm",
        "tf_serving",
        "web",
        "yuzhou",
        "embed_calc",
        "mem_retrieval", # end
        "ad_union_online",  # 额外的配置
    ]
    os.environ["COMMON_RECO_LEAF_EXTRA_PROCESSORS"] = ' '.join(default_extra_processors)

cc_binary(name = "dsp_feature",
  srcs = ["*.cc", "feature/*.cc", "universe/*.cc", "recall/*.cc", "outer_recall/*.cc", "utils/*.cc", "native/*.cc", "extract_bs/extract_feature_adlog.cc", "tag_center_btq/*.cc", "request_redis/*.cc"],

  excludes = ["test_*.cc", "dragon.cc", "adlog_field_type.cc", "dsp_feature_bs.cc"],

  cppflags = [
          "-D GOOGLE_LOGGING=1",
          "-I teams/ad/ad_algorithm/prebuild/zstd/include/",
          "-I teams/ad/ad_algorithm/prebuild/msgpack-c/include/",
          "-D DMLC_USE_GLOG",
          "-D USE_HDFS=1",
          "-D DMLC_USE_HDFS=1",
          "-Ithird_party/zookeeper/include/",
          "-Ithird_party/zeromq/include/",
          '-Ithird_party/nlohmann_json/include',
          "-std=c++17",
        ] + dragon_flag,
  ldflags=[
      '-Lteams/ad/ad_algorithm/prebuild/zstd/lib/',
      '-lzstd',
      "-lstdc++fs",
  ],
  deps = [
        "//teams/ad/ad_nn/flatten_raw_feature/BUILD:flatten_helper",
        "//teams/ad/ad_algorithm/feature/BUILD:fast_feature",
        "//teams/ad/ad_algorithm/bs_feature/BUILD:bs_fast_feature",
        "//base/strings/BUILD:strings",
        "//base/file/BUILD:file",
        "//third_party/jsoncpp/BUILD:jsoncpp",
        "//base/hash_function/BUILD:hash_function",
        "//base/encoding/BUILD:encoding",
        "//teams/ad/ad_algorithm/log_preprocess/BUILD:log_preprocess",
        "//teams/ad/ad_algorithm/reco_with_ad_log/BUILD:log_parser",
        "//base/container/BUILD:container",
        "//teams/ad/ad_algorithm/reco_with_ad_log/BUILD:raw_sample_log_parser",
        "//teams/ad/ad_proto/kuaishou/BUILD:algo",
        "//ks/util/BUILD:util",
        "//teams/ad/ad_nn/feature/BUILD:feature_extract_bundle",
        "//teams/ad/ad_nn/feature_extract/BUILD:ad_feature_extract_handler",
        "//teams/ad/ad_nn/feature/BUILD:feature_preprocessor",
        "//teams/ad/ad_base/src/kafka/BUILD:kafka",
        "//third_party/zeromq/BUILD:zeromq",
        "//third_party/boost/BUILD:boost",
        "//teams/ad/ad_nn/gpu_ps/proto/BUILD:gpu_ps",
        "//teams/ad/ad_nn/kafka_feature/BUILD:kafka_feature_proto",
        "//teams/ad/ad_algorithm/sample_util/BUILD:sample_converter",
        "//teams/ad/ad_algorithm/model_online/BUILD:model_util",
        '//serving_base/jansson/BUILD:json',
        "//teams/ad/ad_proto/kuaishou/BUILD:ad_algorithm_model",
        "//teams/ad/ad_proto/kuaishou/BUILD:eds_proto",
        "//teams/ad/picasso/BUILD:picasso_sdk",
        "//teams/ad/picasso/BUILD:picassobatch_sdk",
        "//third_party/abseil/BUILD:abseil",
        "//teams/ad/ad_base/src/kafka/BUILD:kafka",
        "//third_party/flatbuffers/BUILD:flatbuffers",
        "//ks/common_reco/index/BUILD:proto",
        "//teams/reco-arch/colossus/BUILD:client",
        "//ks/cofea/BUILD:proto",
        "//ks/reco_proto/feature_pipe/BUILD:proto",
        "//teams/reco-arch/colossus/BUILD:proto",
        "//serving_base/retrieval/BUILD:retrieval",
        "//ks/reco_proto/proto/BUILD:proto",
        "//ks/photo_store/BUILD:proto",
	    "//teams/ad/ad_algorithm/label_centre/BUILD:data_generate",
        "//teams/ad/ad_algorithm/feature_info/proto_parser/BUILD:proto_parser",
        "//third_party/nlohmann_json/BUILD:nlohmann_json",
        "//teams/ad/ad_algorithm/dragon/training/BUILD:util",
        "//third_party/apache-arrow/arrow-8.0.1/cpp/BUILD:arrow",
        "//third_party/folly/BUILD:folly",
        ] + dragon_deps + [tf_lib],
)

cc_binary(name = "dsp_feature_bs",
  srcs = ["dsp_feature_bs.cc",
          "cmd_base.cc",
          "common.cc",
          "extract_bs/*.cc",
          "feature/print_feature.cc",
          "faster_mapjoin/*.cc",
          "traffic/*.cc"],
  excludes = [],
  cppflags = [
          "-D GOOGLE_LOGGING=1",
          "-I teams/ad/ad_algorithm/prebuild/zstd/include/",
          "-I teams/ad/ad_algorithm/prebuild/msgpack-c/include/",
          "-D DMLC_USE_GLOG",
          "-D USE_HDFS=1",
          "-D DMLC_USE_HDFS=1",
              ],
  ldflags=[
      '-Lteams/ad/ad_algorithm/prebuild/zstd/lib/',
      '-lzstd',
      "-lstdc++fs",
  ],
  deps = [
        "//teams/ad/ad_nn/flatten_raw_feature/BUILD:flatten_helper",
        "//base/strings/BUILD:strings",
        "//teams/ad/ad_nn/feature/BUILD:feature_extract_bundle",
        "//teams/ad/ad_base/src/kafka/BUILD:kafka",
        "//teams/ad/ad_base/src/kconf/BUILD:kconf_node",
        "//teams/ad/ad_algorithm/log_preprocess/BUILD:log_preprocess",
        "//teams/ad/ad_algorithm/bs_log_preprocess/BUILD:bs_log_preprocess",
        "//teams/ad/ad_algorithm/feature/BUILD:fast_feature",
        "//teams/ad/ad_algorithm/bs_feature/BUILD:bs_fast_feature",
        "//base/file/BUILD:file",
        "//base/hash_function/BUILD:hash_function",
        "//base/encoding/BUILD:encoding",
        "//teams/ad/ad_nn/data_converter/BUILD:rawfea_convert",
        "//base/container/BUILD:container",
        "//infra/redis_proxy_client/BUILD:redis_client",
        "//teams/ad/ad_proto/kuaishou/BUILD:algo",
        "//ks/util/BUILD:util",
        '//serving_base/jansson/BUILD:json',
        "//third_party/abseil/BUILD:abseil",
        "//third_party/flatbuffers/BUILD:flatbuffers",
        "//teams/ad/ad_proto/kuaishou/BUILD:ad_algorithm_model",
        "//teams/ad/ad_nn/kafka_feature/BUILD:kafka_feature_proto",
        "//ks/cofea/BUILD:proto",
        "//ks/reco_proto/feature_pipe/BUILD:proto",
        "//teams/reco-arch/colossus/BUILD:proto",
        "//serving_base/retrieval/BUILD:retrieval",
        "//ks/reco_proto/proto/BUILD:proto",
        "//ks/photo_store/BUILD:proto",
        "//teams/ad/ad_algorithm/feature_info/proto_parser/BUILD:proto_parser",
        "//third_party/nlohmann_json/BUILD:nlohmann_json",
        "//third_party/folly/BUILD:folly",
        ] + [tf_lib],
)

cc_binary(name = "test_dsp_feature",
  srcs = ["*.cc",
          "feature/*.cc",
          "universe/*.cc",
          "recall/*.cc",
          "utils/*.cc",
          "native/*.cc",
          "faster_mapjoin/*.cc",
          "traffic/*.cc"],
  excludes = ["dsp_feature.cc",
              "dragon.cc",
              "adlog_field_type.cc",
              "dsp_feature_bs.cc"],
  cppflags = [
          "-D GOOGLE_LOGGING=1",
          "-I teams/ad/ad_algorithm/prebuild/zstd/include/",
          "-I teams/ad/ad_algorithm/prebuild/msgpack-c/include/",
          "-D DMLC_USE_GLOG",
          "-D USE_HDFS=1",
          "-D DMLC_USE_HDFS=1",
          "-Ithird_party/zookeeper/include/",
          "-Ithird_party/zeromq/include/",
              ],
  ldflags=[
      '-Lteams/ad/ad_algorithm/prebuild/zstd/lib/',
      '-lzstd',
      "-lstdc++fs",
  ],
  deps = [
        "//teams/ad/ad_nn/flatten_raw_feature/BUILD:flatten_helper",
        "//teams/ad/ad_algorithm/feature/BUILD:fast_feature",
        "//teams/ad/ad_algorithm/bs_feature/BUILD:bs_fast_feature",
        "//base/strings/BUILD:strings",
        "//base/file/BUILD:file",
        "//base/hash_function/BUILD:hash_function",
        "//base/encoding/BUILD:encoding",
        "//teams/ad/ad_algorithm/log_preprocess/BUILD:log_preprocess",
        "//teams/ad/ad_algorithm/reco_with_ad_log/BUILD:log_parser",
        "//teams/ad/ad_algorithm/reco_with_ad_log/BUILD:raw_sample_log_parser",
        "//teams/ad/ad_proto/kuaishou/BUILD:algo",
        "//ks/util/BUILD:util",
        "//third_party/boost/BUILD:boost",
        "//teams/ad/ad_nn/feature/BUILD:feature_extract_bundle",
        "//base/container/BUILD:container",
        "//third_party/zeromq/BUILD:zeromq",
        "//teams/ad/ad_base/src/kafka/BUILD:kafka",
        "//teams/ad/ad_nn/gpu_ps/proto/BUILD:gpu_ps",
        "//teams/ad/ad_nn/kafka_feature/BUILD:kafka_feature_proto",
        "//teams/ad/ad_algorithm/sample_util/BUILD:sample_converter",
        "//teams/ad/ad_algorithm/model_online/BUILD:model_util",
        '//serving_base/jansson/BUILD:json',
        "//teams/ad/ad_proto/kuaishou/BUILD:eds_proto",
        "//teams/ad/ad_proto/kuaishou/BUILD:ad_algorithm_model",
        "//teams/ad/picasso/BUILD:picasso_sdk",
        "//teams/ad/picasso/BUILD:picassobatch_sdk",
        "//third_party/abseil/BUILD:abseil",
        "//ks/common_reco/index/BUILD:proto",
        "//third_party/folly/BUILD:folly",
        ],
)

cc_binary(name = "adlog_field_type",
  srcs = ["adlog_field_type.cc", "cmd_base.cc"],
  excludes = [],
  cppflags = [
          "-D GOOGLE_LOGGING=1",
          "-D DMLC_USE_GLOG",
          ],
  ldflags=[
  ],
  deps = [
        "//base/strings/BUILD:strings",
        "//ks/util/BUILD:util",
        "//teams/ad/ad_proto/kuaishou/BUILD:algo",
        '//serving_base/jansson/BUILD:json',
        "//teams/ad/ad_proto/kuaishou/BUILD:user_info"
        ],
)

cc_binary(name = "test_tf_hdfs",
  srcs = ["test/test_tf_hdfs.cc", "cmd_base.cc"],
  deps = [
        "//base/strings/BUILD:strings",
        "//ks/util/BUILD:util",
        "//base/file/BUILD:file",
        "//base/common/BUILD:base",
        "//third_party/gtest/BUILD:gtest",
        ] + [tf_lib],
  cppflags = [
          "-D GOOGLE_LOGGING=1",
          ],
  ldflags=[
  ]
)

cc_binary(name = "test_shell_hdfs",
  srcs = ["test/test_shell_hdfs.cc", "cmd_base.cc"],
  deps = [
        "//base/strings/BUILD:strings",
        "//ks/util/BUILD:util",
        "//base/file/BUILD:file",
        "//base/common/BUILD:base",
        "//third_party/gtest/BUILD:gtest",
        "//serving_base/hdfs_read/BUILD:hdfs_read",
        ],
  cppflags = [
          "-D GOOGLE_LOGGING=1",
          ],
  ldflags=[
  ]
)

cc_binary(name = "map_join",
  srcs = ["cmd_base.cc", "map_join/*.cc"],
  cppflags = [
          "-D GOOGLE_LOGGING=1",
          ],
  ldflags=[
  ],
  deps = [
        "//base/strings/BUILD:strings",
        "//ks/util/BUILD:util",
        "//base/file/BUILD:file",
        "//base/common/BUILD:base",
        ] + [tf_lib]
)
