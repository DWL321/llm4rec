#pragma once

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <cblas.h>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <string>
#include <set>
#include <utility>
#include <random>
#include <thread>
#include <mutex>
#include <sstream>
#include "teams/ad/ad_base/src/kconf/kconf_node.h"
#include "teams/ad/ad_algorithm/log_preprocess/log_preprocess.h"
#include "ks/krp/c++/kuiba/reco_ad_model_server/reco_feature/enricher/live_author_cluster_enricher.h"
#include "ks/krp/c++/kuiba/reco_ad_model_server/reco_feature/enricher/live_reco_cluster_enricher.h"
#include "base/common/gflags.h"
#include "base/common/base.h"
#include "base/common/logging.h"
#include "base/time/timestamp.h"
#include "base/strings/string_split.h"
#include "base/strings/string_printf.h"
#include "base/strings/string_number_conversions.h"
#include "base/encoding/base64.h"
#include "teams/ad/ad_proto/kuaishou/ad/algorithm/model/ad_sample.pb.h"
#include "teams/ad/ad_proto/kuaishou/ad/algorithm/model/ad_feature.pb.h"
#include "base/encoding/line_escape.h"
#include "base/hash_function/md5.h"
#include "kess/rpc/grpc/grpc_client_builder.h"
#include "teams/ad/ad_nn/feature/ad_field_feature.h"
#include "teams/ad/ad_nn/feature/feature_extractor_bundle.h"
#include "teams/ad/ad_algorithm/reco_with_ad_log/log_parser.h"
#include "teams/ad/ad_algorithm/reco_with_ad_log/raw_sample_log_parser.h"

#include "teams/ad/ad_algorithm/mr/cmd_base.h"
#include "teams/ad/ad_algorithm/mr/tool.h"
#include "teams/reco-arch/colossus/client/common_item_client.h"
#include "infra/redis_proxy_client/src/redis_proxy_client/redis_proxy_client.h"
#include "teams/ad/picasso/sdk/picasso_client/picasso_client.h"
#include "teams/ad/ad_algorithm/feature/fast/frame/fast_feature.h"
#include "teams/ad/ad_algorithm/mr/up_model_dataset_construct/seq_dataset_constructor.h"
#include "teams/ad/ad_algorithm/mr/up_model_dataset_construct/product_feat.h"
#include "teams/ad/ad_algorithm/mr/request_redis/redis_embedding_manager.h"
#include "teams/ad/ad_nn/kconf_manager/trigger_config.h"

#ifdef BUILD_WITH_DRAGON
#include "dragon/src/module/common_reco_pipeline_executor.h"
#endif

namespace ks {
namespace ad_algorithm {

using kuaishou::ad::algorithm::AdPersiaFeature;
using ks::ad_nn::Type2ExtractResultsPtr;
using ks::ad_nn::AdFeatureExtractorBundle;
using ks::ad_nn::FeatureFileInfo;
using ks::ad_nn::AdFieldFeature;
using ks::ad_nn::Type2ExtractResultsPtr;
using ks::ad_nn::Type2RetIndices;
using ks::ad_nn::ExtractType;
using ks::ad_nn::FeatureOrgLayout;
using ks::ad_nn::CacheInfo;
using kuaishou::retr::Embeddings;
using kuaishou::retr::CommonSpec;
using ks::ad_algorithm::AdLog;
using ks::ad_twin_towers::RedisEncodeType;
using HashFunc = std::function<uint64_t(uint64_t)>;

AdlogDataType guess_adlog_data_type(const std::string& line, const std::string& sep);

enum PreprocessType {No, All, Detail, DetailCount};

class FeatureProcessor;

class AdlogParser {
 public:
  Flags flags;
  std::unique_ptr<colossus::CommonItemClient> colossus_client_;
  std::unique_ptr<colossus::CommonItemClient> reco_colossus_client_;
  std::unique_ptr<colossus::CommonItemClient> reco_colossus_v1_client_;
  std::unique_ptr<LiveAuthorClusterEnricher> author_enricher_;
  std::unique_ptr<LiveRecoClusterEnricher> live_cluster_enricher_;
  std::unique_ptr<colossus::CommonItemClient> ad_goods_colossus_client_;
  std::unique_ptr<colossus::CommonItemClient> ad_goods_colossus_new_client_;
  std::unique_ptr<FeatureProcessor> feature_processor_;
  std::unique_ptr<colossus::CommonItemClient> ad_colossus_client_;
  AdlogParser();

  AdlogDataType adlog_data_type;
  std::unique_ptr<ks::ad_algorithm::LogPreprocess> processor = nullptr;
  RecoWithAdLogParser reco_with_ad_log_parser;
  RawSamplePackageLogParser raw_sample_log_parser;

  bool CallColossusWithRetry(
      const std::unique_ptr<colossus::CommonItemClient>& colossus_client,
      const uint64_t user_id, colossus::CommonItemResponse* resp);

  std::pair<bool, std::unique_ptr<AdLog>>
  parse_adlog(const std::string &line,
              PreprocessType proprocess_type,
              tensorflow::Tensor *t,
              std::vector<std::string> *debug_info_strings = nullptr);

  std::pair<bool, std::unique_ptr<AdLog>>
  parse_adlog(const std::string &line,
              PreprocessType proprocess_type,
              tensorflow::Tensor *t,
              std::vector<std::string> *debug_info_strings,
              int move_item_mode);  // move_item_mode - 0: move 1: copy 2: do nothing
};

class ExtractFeature: public Worker {
 public:
  ExtractFeature();

  std::unique_ptr<AdFieldFeature> feature_extractor = nullptr;
  std::shared_ptr<FeatureFileInfo> feature_file_info = nullptr;
  std::unique_ptr<AdFeatureExtractorBundle> ad_fea_ext_bundle_ptr = nullptr;

  std::vector<std::vector<float>> dense_field_value;

  ks::ad_nn::ItemModelFieldKeySign sparse_results;
  ks::ad_nn::ItemModelFieldKeySign dense_results;

  AdlogDataType adlog_data_type;
  ExtendField extend_field;
  bool is_need_label = true;
  Flags flags;

  AdlogParser adlog_parser;

  std::random_device rd;
  std::mt19937 gen;
  std::uniform_int_distribution<int> dist;

  std::vector<FeatureType> sparse_feature_type_sorted;
  std::vector<FeatureType> dense_feature_type_sorted;

#ifdef BUILD_WITH_DRAGON
  std::vector<std::string> run_pipelines_;
  std::vector<std::string> item_dragon_slot_keys_;
  std::vector<std::string> item_dragon_sign_keys_;
  std::vector<std::string> user_dragon_slot_keys_;
  std::vector<std::string> user_dragon_sign_keys_;
  std::unordered_map<int64_t, std::string> user_dragon_features_;
  std::unordered_map<int64_t, int> user_dragon_fields_;
  std::unordered_map<std::string, int> dense_user_dragon_fields_;
  std::unordered_map<int, int> dense_user_dragon_sizes_;
  std::unordered_map<int64_t, std::string> item_dragon_features_;
  std::unordered_map<int64_t, int> item_dragon_fields_;
  std::unordered_map<std::string, int> dense_item_dragon_fields_;
  std::unordered_map<int, int> dense_item_dragon_sizes_;

  std::unique_ptr<base::Json> dragon_config;
#endif
  int total_feature_count = 0;

  void init();

  bool is_nohash(const std::vector<ks::ad_nn::SlotConfig>& slots);

  virtual std::vector<std::vector<std::string>> process_line(std::string* line);

  std::vector<AdPersiaFeature> extract_feature_label(const std::string& line,
                                                     AdlogDataType adlog_data_type);

  std::vector<AdPersiaFeature> extract_feature_label_bundle(const std::string& line,
                                                            AdlogDataType adlog_data_type);

  virtual std::vector<std::vector<std::string>> extract_feature_frame(const std::string& line,
                                                                      AdlogDataType adlogDataType);

  std::vector<std::vector<std::string>> extract_feature_sequence(const std::string& line,
                                                                 AdlogDataType adlogDataType);

  AdPersiaFeature extract_persia_feature(const ks::ad_algorithm::AdLog& adlog, size_t pos);

  AdPersiaFeature extract_persia_feature_bundle(const ks::ad_algorithm::AdLog& adlog,
                                                Type2ExtractResultsPtr* sparse_results_bundle,
                                                Type2ExtractResultsPtr* dense_results_bundle,
                                                const Type2RetIndices& sparse_indices,
                                                const Type2RetIndices& dense_indices,
                                                size_t pos,
                                                int item_size);
  bool ExtractFeatureWithDragon(const ks::ad_algorithm::AdLog& adlog,
        std::vector<std::unordered_map<uint32_t, std::vector<int64>>> *dragon_signs,
        std::unordered_map<uint32_t, std::vector<int64>> *user_dragon_signs,
        std::unordered_map<std::string, std::vector<double>> *dense_user_dragon_signs,
        std::vector<std::unordered_map<std::string, std::vector<double>>> *dense_item_dragon_signs);
};

class ExtractFeatureNew: public ExtractFeature {
 public:
  std::vector<std::vector<std::string>> process_line(std::string* line);
};

class ExtractFeatureLtr: public ExtractFeature {
 public:
  std::vector<std::vector<std::string>> process_line(std::string* line);
};

class ExtractFeatureUPModelLtr: public ExtractFeature {
 public:
  ExtractFeatureUPModelLtr();
  bool LoadPdctEmb();
  std::pair<std::string, int> CalculateSeqLen(const std::string& line);
  std::vector<std::vector<std::string>> WriteU2UtoRedis(const std::string& line);
  std::vector<std::vector<std::string>> WritePdctEmbtoRedis(const std::string& line);
  std::vector<std::vector<std::string>> WritePdctClusterEmbtoRedis(const std::string& line);
  std::vector<std::vector<std::string>> WritePdctSeqtoRedis(const std::string& line);
  std::vector<std::vector<std::string>> PdctEmbNormalize(const std::string& line);
  bool GetBatchEmbeddings(std::vector<std::string>& mgi_keys, std::string log_flag);
  bool PrepareEmbKeys(std::vector<uint64_t> ids, std::vector<std::string>& mgi_keys,
                      int id_type, bool enable_mgi_int64_trans);
  bool ParseIdEmbVector(std::vector<std::vector<float>> &ad_log_mgi_embeddings_, int id_type, uint64_t user_id);
  float dot_product(const std::vector<float>& v1, const std::vector<float>& v2);
  std::vector<float> dot_product_all(const std::vector<std::vector<float>>& v1, const std::vector<std::vector<float>>& v2, int hist_pdct_num);
  std::string vector_to_string(const std::vector<float>& vec);
  std::vector<std::vector<std::string>> GetHistoryEmbedding(const std::string& line);
  std::vector<std::vector<std::string>> ExtractUPForInfDaily(const std::string& line);
  std::vector<std::vector<std::string>> ExtractUPForInf(const std::string& line);
  bool WriteEmb2Redis(const Embeddings& embeddings, std::string key);
  std::vector<std::string> CalculateDeliPdctNum(const std::string& line);
  void AllPdctsFillAdlogProto(std::vector<std::string>& results, std::vector<std::string> &rst);
  std::vector<std::string> FillAdlogProto(const std::string& line);
  std::string apply_hash(const std::string &in_feat, bool is_multi);
  std::vector<std::vector<std::string>> apply_hash_on_pdct_feat(const std::string &line);
  std::vector<std::vector<std::string>> process_line(std::string* line);

  std::unordered_map<std::string, ProductFeat> mp_product_feature;
  std::unordered_map<std::string, float> mp_product_name_freq;
  std::unordered_map<std::string, std::vector<float>> product_emb_map;
  std::unordered_map<std::string, int> product_count;
  std::unordered_set<std::string> all_outloop_product_set; 
  SeqPointwiseDatasetConstructor constructor;
  SeqPointwiseDatasetValidConstructor constructor_valid;
  ks::infra::RedisClient* redis_client_ = nullptr;
  ks::infra::RedisPipelineClient* redis_async_client_ = nullptr;
  std::unique_ptr<RedisManager> redis_embedding_u2p_;
  std::unique_ptr<RedisManager> redis_embedding_p2e_;
  Embeddings interest_photo_id_;
  std::vector<uint64_t> photo_ids_;
  std::vector<Embeddings> interest_photo_embeddings_;
  std::vector<std::vector<float>> ad_log_hist_mgi_embeddings_;
  std::vector<std::vector<float>> ad_log_sys_pdct_embeddings_;
};

class ExtractSequenceFeature: public ExtractFeature {
 public:
  std::vector<std::vector<std::string>> process_line(std::string* line);
};

class ExtractFeatureJoinId: public ExtractFeature {
 public:
  std::vector<std::vector<std::string>> process_line(std::string* line);
};

class ExtractFeatureFromUserInfo: public Worker {
  public:
    inline uint64_t GetFeature(FeaturePrefix prefix, uint64_t left) {
      return GetFeature(prefix, left, 0, 0);
    }

    inline uint64_t GetFeature(FeaturePrefix prefix, uint64_t left, uint64_t mid, uint64_t right) {
      return ((uint64_t)(prefix) << prefix_shift_bits_) |
        ((left & left_mask_) << left_shift_bits_) |
        ((mid & mid_mask_) << mid_shift_bits_) |
        ((right & right_mask_) << right_shift_bits_);
    }

    std::vector<AdPersiaFeature> extract_feature_from_userinfo(const std::string& line);
    std::vector<std::vector<std::string>> process_line(std::string* line);
    HashFunc hash_func = ks::ad_base::djb2_hash64;
    int bucket_size = 1000001;
    size_t prefix_shift_bits_ = 52;

    uint64_t left_mask_ = (1ULL << 52) - 1;
    size_t left_shift_bits_ = 52 - 52;

    uint64_t right_mask_ = 0;
    size_t right_shift_bits_ = 0;

    uint64_t mid_mask_ = 0;
    size_t mid_shift_bits_ = 0;
};

class ExtractFeatureFromUserInfoFill: public ExtractFeature {
 public:
  std::string FillAdlogProto(const std::string& line);
  std::vector<std::vector<std::string>> process_line(std::string* line);
};

class ExtractFeatureFromInterestProduct: public Worker {
  public:
    ExtractFeatureFromInterestProduct();
    std::vector<AdPersiaFeature> extract_feature_from_product(const std::string& line);
    std::vector<std::vector<std::string>> process_line(std::string* line);
    HashFunc hash_func = ks::ad_base::djb2_hash64;
    int bucket_size = 1000001;
    std::unordered_map<std::string, float> product_name_id_map;
    float default_product_id = 1000000;
    std::unordered_map<std::string, float> product_name_freq;
    std::unordered_map<std::string, std::vector<float>> product_emb_map;
};

class ExtracProductCityHash64Id: public Worker {
  public:
    std::vector<std::vector<std::string>> process_line(std::string* line);
};

class FilterLog: public Worker {
 public:
  FilterLog() = default;

  AdlogParser adlog_parser;
  std::vector<std::vector<std::string>> process_line(std::string* line);
};

class FilterLogPvId: public Worker {
 public:
  FilterLogPvId() = default;

  AdlogParser adlog_parser;
  std::vector<std::vector<std::string>> process_line(std::string* line);
};

class PosSetFix {
 public:
  std::set<uint64> click_set;
  Flags flags;

  static PosSetFix * GetInstance() {
    static PosSetFix instance;
     return &instance;
  }

  PosSetFix() {
    string &pos_file = flags.pos_file;
    std::ifstream is_pos(pos_file);
    LOG(ERROR) << "pos_file = " << pos_file << std::endl;
    uint64 key;

    while ( is_pos >> key ) {
       click_set.insert(key);
    }
    LOG(ERROR) << "Total click num = " << click_set.size();
  }

  bool contains(uint64 key) {
    return click_set.find(key) != click_set.end();
  }
};

class TargetFixFeature: public ExtractFeature {
 public:
  std::vector<std::vector<std::string>> process_line(std::string* line);
  unsigned int seed = time(NULL);
};

class PosSetFixMtl {
 public:
  std::set<uint64> click_set;
  std::set<uint64> click2_set;
  Flags flags;

  static PosSetFixMtl * GetInstance() {
     static PosSetFixMtl instance;
     return &instance;
  }

  PosSetFixMtl() {
    string &pos_file = flags.pos_file;
    std::ifstream is_pos(pos_file);
    LOG(ERROR) << "pos_file = " << pos_file << std::endl;
    uint64 key;
    int item_imp, item_click;
    while (is_pos >> key >> item_imp >> item_click) {
      if (item_imp) {
        click_set.insert(key);
      }
      if (item_click) {
        click2_set.insert(key);
      }
    }
    LOG(ERROR) << "Total click num = " << click_set.size();
    LOG(ERROR) << "Total click2 num = " << click2_set.size();
  }

  bool contains(uint64 key) {
    return click_set.find(key) != click_set.end();
  }

  bool contains2(uint64 key) {
    return click2_set.find(key) != click2_set.end();
  }
};

class TargetFixMTLFeature: public ExtractFeature {
 public:
  std::vector<std::vector<std::string>> process_line(std::string* line);
  std::vector<AdPersiaFeature> extract_feature_label(const std::string& line,
                                                     AdlogDataType adlog_data_type,
                                                     std::vector<int>* delivery_vec);
  unsigned int seed = time(NULL);
};

class ExtractFeatureFederate: public ExtractFeature {
 public:
  std::vector<std::vector<std::string>> process_line(std::string* line);
};

class ExtractSimpleFeatures: public ExtractFeature {
 public:
  std::vector<std::vector<std::string>> process_line(std::string* line);
};

class FeatureProcessor {
 public:
  FeatureProcessor() = default;
  virtual ~FeatureProcessor() {}
 public:
  void Init(const std::vector<std::string>& processor_names);
  void Process(ks::ad_algorithm::AdLogWrapper* adlog);  // NOLINT

 public:
  virtual bool DoInit() {
    LOG(ERROR) << "FeatureProcessor DoInit, not implemented";
    return false;
  }
  virtual bool DoProcess(ks::ad_algorithm::AdLogWrapper* adlog) {  // NOLINT
    LOG(ERROR) << "FeatureProcessor DoProcess, not implemented";
    return false;
  }
 private:
  std::vector<std::unique_ptr<FeatureProcessor>> valid_processors_;
};

typedef FeatureProcessor* (*PtrObject)(void);
class ClassFactory {
 private:
  std::unordered_map<std::string, PtrObject> class_map_;
  ClassFactory() {}
 public:
  std::unordered_map<std::string, PtrObject> GetAllClass() {
    return class_map_;
  }
  FeatureProcessor* GetClassByName(std::string class_name) {
    std::unordered_map<std::string, PtrObject>::const_iterator iter;
    iter = class_map_.find(class_name);
    if (iter == class_map_.end()) {
      return NULL;
    } else {
      return iter->second();
    }
  }
  void RegisterClass(const std::string& class_name, PtrObject object) {
    class_map_.insert(std::pair<std::string, PtrObject>(class_name, object));
  }
  static ClassFactory& GetInstance() {
    static ClassFactory factory;
    return factory;
  }
};

class ClassRegister {
 public:
  ClassRegister(const std::string& class_name, PtrObject object) {
    ClassFactory::GetInstance().RegisterClass(class_name, object);
  }
};

#define REGISTER_PROCESSOR(CLASS_NAME)                                             \
  CLASS_NAME* Create##CLASS_NAME() {                                               \
    return new CLASS_NAME;                                                         \
  }                                                                                \
  static ClassRegister global_##CLASS_NAME(#CLASS_NAME, (PtrObject)Create##CLASS_NAME);   \

using ItemType = kuaishou::ad::algorithm::ItemType;

enum class ColossusType {
  AD_LIVE_ITEM = 1,
  RECO_LIVE_ITEM = 2,
  RECO_LIVE_ITEM_V1 = 3
};

enum class GsuHardSearchKeyType {
  GLOBAL_CLUSTER = 1,
  ECOMM_CLUSTER = 2,
  AUTHOR_ID = 3,
  AUTHOR_CLUSTER = 4,
  REMOTE_CLUSTER = 5,
  ITEM_SPU = 6,
  ITEM_CID3 = 7
};

template<ColossusType colossus_type, GsuHardSearchKeyType search_key_type,
         int max_list_len, int max_neighbor>
class HardSearchGsuProcessor : public FeatureProcessor {
 public:
  HardSearchGsuProcessor():FeatureProcessor() {
  }
  ~HardSearchGsuProcessor() {}
  bool DoInit() override {
    LOG(INFO) << "HardSearchGsuProcessor DoInit";
    InitNeighborMap();
    return true;
  }
  bool DoProcess(ks::ad_algorithm::AdLogWrapper* adlog) override {
    std::unordered_map<int64_t, std::vector<int>> cluster_index;
    if (colossus_type == ColossusType::AD_LIVE_ITEM) {
      GetClusterIndex(adlog->colossus_ad_live_item(), adlog, &cluster_index);
    } else if (colossus_type == ColossusType::RECO_LIVE_ITEM) {
      GetClusterIndex(adlog->colossus_reco_live_item(), adlog, &cluster_index);
    } else if (colossus_type == ColossusType::RECO_LIVE_ITEM_V1) {
      GetClusterIndex(adlog->colossus_reco_live_v1_item(), adlog, &cluster_index);
    }
    auto p_gsu_result = GetGsuResult(adlog);
    if (p_gsu_result == nullptr) {
      return true;
    }
    auto& target_result_idx = *p_gsu_result;
    auto author_cluster_map = adlog->author_cluster_map();
    auto live_cluster_map = adlog->live_cluster_map();
    for (int i = 0; i < adlog->item_size(); i++) {
      int64_t target_cluster_id = -1;
      const auto live_info = GetLiveInfo(adlog->item(i));
      const auto author_info = GetAuthorInfo(adlog->item(i));
      switch (search_key_type) {
        case GsuHardSearchKeyType::GLOBAL_CLUSTER:
          if (live_info == nullptr) {
            continue;
          }
          for (const ::kuaishou::ad::CommonInfoAttr &itemAttr : live_info->common_info_attr()) {
            if (itemAttr.name_value()
                == kuaishou::ad::CommonInfoAttr_NameExtendOne_AD_MATERIAL_LIVE_MMU_CLUSTER_ID) {
              target_cluster_id = itemAttr.int_value();
              break;
            }
          }
          break;
        case GsuHardSearchKeyType::ECOMM_CLUSTER:
          if (live_info == nullptr) {
            continue;
          }
          for (const ::kuaishou::ad::CommonInfoAttr &itemAttr : live_info->common_info_attr()) {
            if (itemAttr.name_value()
                == kuaishou::ad::CommonInfoAttr_NameExtendOne_LIVE_REALTIME_MMU_UE_2C_CLUSTER_ID) {
              target_cluster_id = itemAttr.int_value();
              break;
            }
          }
          break;
        case GsuHardSearchKeyType::AUTHOR_ID:
          if (author_info == nullptr || author_info->id() <= 0) {
            continue;
          }
          target_cluster_id = author_info->id();
          break;
        case GsuHardSearchKeyType::AUTHOR_CLUSTER:
          if (author_info == nullptr || author_info->id() <= 0) {
            continue;
          }
          if (author_cluster_map != nullptr) {
            auto it = author_cluster_map->find(author_info->id());
            if (it != author_cluster_map->end()) {
              target_cluster_id = it->second;
            }
          }
          break;
        case GsuHardSearchKeyType::REMOTE_CLUSTER:
          if (live_info == nullptr || live_info->id() <= 0) {
            continue;
          }
          if (live_cluster_map != nullptr) {
            auto it = live_cluster_map->find(live_info->id());
            if (it != live_cluster_map->end()) {
              target_cluster_id = it->second;
            }
          }
          break;
        case GsuHardSearchKeyType::ITEM_SPU:
          if (live_info == nullptr) {
            continue;
          }
          for (const auto& attr : live_info->common_info_attr()) {
            if (attr.name_value() ==
                kuaishou::ad::CommonInfoAttr_NameExtendOne_LIVE_REALTIME_YELLOWTROLLEY_SPU1 ||
                attr.name_value() ==
                kuaishou::ad::CommonInfoAttr_NameExtendOne_LIVE_REALTIME_YELLOWTROLLEY_SPU2 ||
                attr.name_value() ==
                kuaishou::ad::CommonInfoAttr_NameExtendOne_LIVE_REALTIME_EXPLAINING_SPU ||
                attr.name_value() ==
                kuaishou::ad::CommonInfoAttr_NameExtendOne_LIVE_REALTIME_MMU_EXPLAINING_SPU ||
                attr.name_value() ==
                kuaishou::ad::CommonInfoAttr_NameExtendOne_LIVE_REALTIME_HOT_POP_SPU ||
                attr.name_value() ==
                kuaishou::ad::CommonInfoAttr_NameExtendOne_LIVE_REALTIME_SEC_KILL_SPU) {
              target_result_idx[attr.int_value()].clear();
            }
          }
          break;
        case GsuHardSearchKeyType::ITEM_CID3:
          if (live_info == nullptr) {
            continue;
          }
          for (const auto& attr : live_info->common_info_attr()) {
            if (attr.name_value() ==
                kuaishou::ad::CommonInfoAttr_NameExtendOne_LIVE_REALTIME_YELLOWTROLLEY_ITEM_CATE_3_LIST) {
              for (int ci = 0; ci < attr.int_list_value_size() && ci < 6; ++ci) {
                target_result_idx[attr.int_list_value(ci)].clear();
              }
            }
          }
          break;
      }
      if (target_cluster_id == -1) {
        continue;
      }
      target_result_idx[target_cluster_id].clear();
    }

    for (const auto& it : target_result_idx) {
      auto target_cluster_id = it.first;
      auto &result_idx = target_result_idx[target_cluster_id];
      int result_cnt = 0;
      for (int k = 0; k < max_neighbor; k++) {
        int64_t search_key = -1;
        if (!cluster_neighbor_map.count(target_cluster_id)) {
          if (k == 0) {
            search_key = target_cluster_id;
          }
        } else if (k < cluster_neighbor_map[target_cluster_id].size()) {
          search_key = cluster_neighbor_map[target_cluster_id][k];
        }
        if (search_key == -1) {
          break;
        }
        if (!cluster_index[search_key].empty()) {
          for (auto item_idx : cluster_index[search_key]) {
            result_idx.emplace_back(item_idx);
            result_cnt++;
            if (result_cnt >= max_list_len) {
              break;
            }
          }
          if (result_cnt >= max_list_len) {
            break;
          }
        }
      }
    }
    SetProcessorReady(adlog);
    return true;
  }

 private:
  std::unordered_map<int64_t, std::vector<int64_t>> cluster_neighbor_map;

  void SetProcessorReady(ks::ad_algorithm::AdLogWrapper* adlog) {
    if (colossus_type == ColossusType::AD_LIVE_ITEM) {
      if (search_key_type == GsuHardSearchKeyType::GLOBAL_CLUSTER) {
        adlog->ad_live_global_gsu_result_ready_ = true;
        return;
      }
      if (search_key_type == GsuHardSearchKeyType::ECOMM_CLUSTER) {
        adlog->ad_live_ecomm_gsu_result_ready_ = true;
        return;
      }
      if (search_key_type == GsuHardSearchKeyType::AUTHOR_ID) {
        adlog->ad_live_author_gsu_result_ready_ = true;
        return;
      }
      if (search_key_type == GsuHardSearchKeyType::AUTHOR_CLUSTER) {
        adlog->ad_live_author_cluster_gsu_result_ready_ = true;
        return;
      }
      if (search_key_type == GsuHardSearchKeyType::REMOTE_CLUSTER) {
        adlog->ad_live_remote_cluster_gsu_result_ready_ = true;
        return;
      }
      if (search_key_type == GsuHardSearchKeyType::ITEM_SPU) {
        adlog->ad_live_spu_gsu_result_ready_ = true;
        return;
      }
      if (search_key_type == GsuHardSearchKeyType::ITEM_CID3) {
        adlog->ad_live_cid3_gsu_result_ready_ = true;
        return;
      }
      return;
    }
    if (colossus_type == ColossusType::RECO_LIVE_ITEM) {
      if (search_key_type == GsuHardSearchKeyType::GLOBAL_CLUSTER) {
        adlog->reco_live_global_gsu_result_ready_ = true;
        return;
      }
      if (search_key_type == GsuHardSearchKeyType::AUTHOR_ID) {
        adlog->reco_live_author_gsu_result_ready_ = true;
        return;
      }
      if (search_key_type == GsuHardSearchKeyType::AUTHOR_CLUSTER) {
        adlog->reco_live_author_cluster_gsu_result_ready_ = true;
        return;
      }
      if (search_key_type == GsuHardSearchKeyType::REMOTE_CLUSTER) {
        adlog->reco_live_remote_cluster_gsu_result_ready_ = true;
        return;
      }
      return;
    }
    if (colossus_type == ColossusType::RECO_LIVE_ITEM_V1) {
      if (search_key_type == GsuHardSearchKeyType::REMOTE_CLUSTER) {
        adlog->reco_live_v1_remote_cluster_gsu_result_ready_ = true;
        return;
      }
    }
  }

  std::shared_ptr<std::unordered_map<int64_t, std::vector<int64_t>>> GetGsuResult(
      ks::ad_algorithm::AdLogWrapper* adlog) {
    if (colossus_type == ColossusType::AD_LIVE_ITEM) {
      if (search_key_type == GsuHardSearchKeyType::GLOBAL_CLUSTER) {
        return adlog->mutable_ad_live_global_gsu_result();
      } else if (search_key_type == GsuHardSearchKeyType::ECOMM_CLUSTER) {
        return adlog->mutable_ad_live_ecomm_gsu_result();
      } else if (search_key_type == GsuHardSearchKeyType::AUTHOR_ID) {
        return adlog->mutable_ad_live_author_gsu_result();
      } else if (search_key_type == GsuHardSearchKeyType::AUTHOR_CLUSTER) {
        return adlog->mutable_ad_live_author_cluster_gsu_result();
      } else if (search_key_type == GsuHardSearchKeyType::REMOTE_CLUSTER) {
        return adlog->mutable_ad_live_remote_cluster_gsu_result();
      } else if (search_key_type == GsuHardSearchKeyType::ITEM_SPU) {
        return adlog->mutable_ad_live_spu_gsu_result();
      } else if (search_key_type == GsuHardSearchKeyType::ITEM_CID3) {
        return adlog->mutable_ad_live_cid3_gsu_result();
      }
    } else if (colossus_type == ColossusType::RECO_LIVE_ITEM) {
      if (search_key_type == GsuHardSearchKeyType::GLOBAL_CLUSTER) {
        return adlog->mutable_reco_live_global_gsu_result();
      } else if (search_key_type == GsuHardSearchKeyType::AUTHOR_ID) {
        return adlog->mutable_reco_live_author_gsu_result();
      } else if (search_key_type == GsuHardSearchKeyType::AUTHOR_CLUSTER) {
        return adlog->mutable_reco_live_author_cluster_gsu_result();
      } else if (search_key_type == GsuHardSearchKeyType::REMOTE_CLUSTER) {
        return adlog->mutable_reco_live_remote_cluster_gsu_result();
      }
    } else if (colossus_type == ColossusType::RECO_LIVE_ITEM) {
      if (search_key_type == GsuHardSearchKeyType::REMOTE_CLUSTER) {
        return adlog->mutable_reco_live_v1_remote_cluster_gsu_result();
      }
    }
    return nullptr;
  }

  // 不同数据类型的处理函数
  void GetClusterIndex(const std::shared_ptr<std::vector<colossus::AdLiveItemT>> colossus_item_list,
                       ks::ad_algorithm::AdLogWrapper* adlog,
                       std::unordered_map<int64_t, std::vector<int>>* cluster_index) {
    // get history live info from colossus_resp
    if (colossus_item_list == nullptr) {
      return;
    }
    const auto& items = *(colossus_item_list);

    for (int i = items.size() - 1; i >= 0; i--) {
      auto& item = items[i];
      if (item.timestamp > adlog->Get().time() / 1000 - 6000) {
        continue;
      }
      int64_t cluster_id = -1;
      auto author_cluster_map = adlog->author_cluster_map();
      auto colossus_ad_live_spu = adlog->colossus_ad_live_spu();
      auto colossus_ad_live_cid3 = adlog->colossus_ad_live_cid3();
      switch (search_key_type) {
        case GsuHardSearchKeyType::GLOBAL_CLUSTER:
          cluster_id = (item.cluster_id >> 0) & ((1 << 16) - 1);
          break;
        case GsuHardSearchKeyType::ECOMM_CLUSTER:
          cluster_id = (item.cluster_id >> 16) & ((1 << 16) - 1);
          break;
        case GsuHardSearchKeyType::AUTHOR_ID:
          cluster_id = item.author_id;
          break;
        case GsuHardSearchKeyType::AUTHOR_CLUSTER:
          if (author_cluster_map != nullptr) {
            auto it = author_cluster_map->find(item.author_id);
            if (it != author_cluster_map->end()) {
              cluster_id = it->second;
            }
          }
          break;
        case GsuHardSearchKeyType::ITEM_SPU:
          if (colossus_ad_live_spu != nullptr) {
            auto it = colossus_ad_live_spu->find("ltss_" + base::Uint64ToString(item.live_id) + "_"
              + base::Uint64ToString(item.timestamp / 60));
            if (it != colossus_ad_live_spu->end()) {
              cluster_id = it->second;
            }
          }
          break;
        case GsuHardSearchKeyType::ITEM_CID3:
          if (colossus_ad_live_cid3 != nullptr) {
            auto it = colossus_ad_live_cid3->find("ltsc_" + base::Uint64ToString(item.live_id) + "_"
              + base::Uint64ToString(item.timestamp / 60));
            if (it != colossus_ad_live_cid3->end()) {
              cluster_id = it->second;
            }
          }
          break;
        case GsuHardSearchKeyType::REMOTE_CLUSTER:
          cluster_id = (item.cluster_id >> 0) & ((1 << 16) - 1);
          break;
        default:
          return;
      }
      if (cluster_id != -1) {
        (*cluster_index)[cluster_id].emplace_back(i);
      }
    }
  }

  // 不同数据类型的处理函数
  void GetClusterIndex(const std::shared_ptr<std::vector<colossus::LiveItemT>> colossus_item_list,
                       ks::ad_algorithm::AdLogWrapper* adlog,
                       std::unordered_map<int64_t, std::vector<int>>* cluster_index) {
    // get history live info from colossus_resp
    if (colossus_item_list == nullptr) {
      return;
    }
    const auto& items = *(colossus_item_list);

    for (int i = items.size() - 1; i >= 0; i--) {
      auto& item = items[i];
      if (item.timestamp > adlog->Get().time() / 1000 - 1860) {
        continue;
      }
      int64_t cluster_id = -1;
      switch (search_key_type) {
        case GsuHardSearchKeyType::GLOBAL_CLUSTER:
          cluster_id = 0x03ff & (item.page_frame_cluster_id);
          break;
        case GsuHardSearchKeyType::AUTHOR_ID:
          cluster_id = item.author_id;
          break;
        case GsuHardSearchKeyType::AUTHOR_CLUSTER:
          if (adlog->author_cluster_map() != nullptr) {
            auto it = adlog->author_cluster_map()->find(item.author_id);
            if (it != adlog->author_cluster_map()->end()) {
              cluster_id = it->second;
            }
          }
          break;
        case GsuHardSearchKeyType::REMOTE_CLUSTER:
          cluster_id = 0x03ff & (item.page_frame_cluster_id);
          break;
        default:
          return;
      }
      if (cluster_id != -1) {
        (*cluster_index)[cluster_id].emplace_back(i);
      }
    }
  }
  // 不同数据类型的处理函数
  void GetClusterIndex(const std::shared_ptr<std::vector<colossus::LiveItemV2T>> colossus_item_list,
                       ks::ad_algorithm::AdLogWrapper* adlog,
                       std::unordered_map<int64_t, std::vector<int>>* cluster_index) {
    // get history live info from colossus_resp
    if (colossus_item_list == nullptr) {
      return;
    }
    const auto& items = *(colossus_item_list);

    for (int i = items.size() - 1; i >= 0; i--) {
      auto& item = items[i];
      if (item.timestamp > adlog->Get().time() / 1000 - 1860) {
        continue;
      }
      int64_t cluster_id = -1;
      switch (search_key_type) {
        case GsuHardSearchKeyType::GLOBAL_CLUSTER:
          cluster_id = 0x03ff & (item.page_frame_cluster_id);
          break;
        case GsuHardSearchKeyType::AUTHOR_ID:
          cluster_id = item.author_id;
          break;
        case GsuHardSearchKeyType::AUTHOR_CLUSTER:
          if (adlog->author_cluster_map() != nullptr) {
            auto it = adlog->author_cluster_map()->find(item.author_id);
            if (it != adlog->author_cluster_map()->end()) {
              cluster_id = it->second;
            }
          }
          break;
        case GsuHardSearchKeyType::REMOTE_CLUSTER:
          cluster_id = 0x03ff & (item.page_frame_cluster_id);
          break;
        default:
          return;
      }
      if (cluster_id != -1) {
        (*cluster_index)[cluster_id].emplace_back(i);
      }
    }
  }

  void InitNeighborMap() {
    std::string kconf_key = "";
    bool reverse_cluster_neighbor = false;
    if (search_key_type == GsuHardSearchKeyType::GLOBAL_CLUSTER ||
        search_key_type == GsuHardSearchKeyType::REMOTE_CLUSTER) {
      kconf_key = "reco.model2.liveHetuEmbeddingIndex100";
      reverse_cluster_neighbor = true;
    } else if (search_key_type == GsuHardSearchKeyType::ECOMM_CLUSTER) {
      kconf_key = "ad.model.ecommMmuClusterNeighbors";
    } else if (search_key_type == GsuHardSearchKeyType::AUTHOR_CLUSTER) {
      kconf_key = "ad.model.authorMmuClusterNeighbors";
      reverse_cluster_neighbor = true;
    }
    if (kconf_key.empty()) {
      return;
    }
    ks::ad_base::kconf::KconfNode<ks::ad_base::kconf::KconfJson> instance(kconf_key);
    std::shared_ptr<base::Json> neighbor_kconf = instance.Get()->data;
    if (neighbor_kconf == nullptr) {
      LOG(ERROR) << "not find kconf key: " << kconf_key;
      return;
    }
    for (const auto& iter : neighbor_kconf->objects()) {
      int64_t cluster_id;
      if (!base::StringToInt64(iter.first, &cluster_id)) {
        LOG(ERROR) << "non-integer cluster id in kconf: " << iter.first;
        return;
      }
      if (!iter.second->IsArray()) {
        LOG(ERROR) << "non-list cluster neighbors in kconf: " << iter.first;
        return;
      }
      for (const auto& p_id : iter.second->array()) {
        int64 id;
        if (!p_id->IntValue(&id)) {
          LOG(ERROR) << "non-integer cluster id in neighbors: " << iter.first;
          return;
        }
        cluster_neighbor_map[cluster_id].push_back(static_cast<int64_t>(id));
      }
    }
    for (auto& p : cluster_neighbor_map) {
      if (reverse_cluster_neighbor) {
        std::reverse(p.second.begin(), p.second.end());
      }
      if (p.second.empty() || p.second[0] != p.first) {
        LOG(ERROR) << "invalid neighbor list: " << p.first;
        return;
      }
    }
  }

  const kuaishou::ad::algorithm::LiveInfo* GetLiveInfo(const ad_algorithm::ItemAdaptorBase& item) {
    if (item.type() == ItemType::FANS_TOP_LIVE &&
        item.has_fans_top_live_info()) {
      return &item.fans_top_live_info().live_info();
    }
    if (item.type() == ItemType::FANS_TOP_LIVE_MERCHANT &&
        item.has_fans_top_live_info()) {
      return &item.fans_top_live_info().live_info();
    }
    if (item.type() == ItemType::FANS_TOP && item.has_fans_top_info() &&
        item.fans_top_info().has_live_info()) {
      return &item.fans_top_info().live_info();
    }
    if (item.type() == ItemType::AD_DSP && item.has_ad_dsp_info()) {
      return &item.ad_dsp_info().live_info();
    }
    if (item.type() == ItemType::NATIVE_AD && item.has_ad_dsp_info()) {
      return &item.ad_dsp_info().live_info();
    }
    if (item.type() == ItemType::AD_BRAND && item.has_ad_dsp_info()
        && item.ad_dsp_info().has_live_info()) {
      return &item.ad_dsp_info().live_info();
    }
    return nullptr;
  }

  const kuaishou::ad::algorithm::AuthorInfo* GetAuthorInfo(const ad_algorithm::ItemAdaptorBase& item) {
    if (item.type() == ItemType::FANS_TOP_LIVE && item.has_fans_top_live_info() &&
        item.fans_top_live_info().has_live_info()) {
      return &item.fans_top_live_info().live_info().author_info();
    }
    if (item.type() == ItemType::FANS_TOP && item.has_fans_top_info() &&
        item.fans_top_info().has_photo_info()) {
      return &item.fans_top_info().photo_info().author_info();
    }
    if ((item.type() == ItemType::AD_DSP || item.type() == ItemType::NATIVE_AD) &&
        item.has_ad_dsp_info()) {
      if (item.ad_dsp_info().has_campaign() && item.ad_dsp_info().has_creative()) {
        auto& campaign = item.ad_dsp_info().campaign().base();
        auto& creative = item.ad_dsp_info().creative().base();
        //  21/22/23 是粉条物料
        if ((campaign.type() == 14 || campaign.type() == 21 || campaign.type() == 22 ||
             campaign.type() == 23) && creative.live_creative_type() == 1) {
          // 只有直播直投的取 live_info 下的，作品引流和作品都取 photo_info 下的
          if (item.ad_dsp_info().has_live_info()) {
            return &item.ad_dsp_info().live_info().author_info();
          }
        } else {
          if (item.ad_dsp_info().has_photo_info()) {
            return &item.ad_dsp_info().photo_info().author_info();
          }
        }
      }
    }
    if (item.type() == ItemType::AD_BRAND && item.has_ad_dsp_info() &&
        item.ad_dsp_info().has_live_info()) {
      return &item.ad_dsp_info().live_info().author_info();
    }
    return nullptr;
  }
};

enum class RedisItemAttrType {
  ITEM_SPU = 1,
  ITEM_CID3 = 2
};

template<RedisItemAttrType redis_item_attr_type>
class RedisItemAttrProcessor : public FeatureProcessor {
 public:
  RedisItemAttrProcessor():FeatureProcessor() {
  }
  ~RedisItemAttrProcessor() {}
  bool DoInit() override {
    LOG(INFO) << "RedisItemAttrProcessor DoInit";
    std::string kcc_name;
    if (redis_item_attr_type == RedisItemAttrType::ITEM_SPU ||
        redis_item_attr_type == RedisItemAttrType::ITEM_CID3) {
      kcc_name = "adLiveSimSpu";
    }
    if (!kcc_name.empty()) {
      redis_client_ =
          ks::infra::RedisProxyClient::GetRedisClientByKcc(
              kcc_name,
              20,
              2, /*io_threds*/
              "", /*biz_name*/
              "", /*service_name*/
              false /*need_replica*/);
    }
    return true;
  }
  bool DoProcess(ks::ad_algorithm::AdLogWrapper* adlog) override {
    std::vector<std::string> request_keys;
    uint64_t time = adlog->Get().time() / 1000;
    if (adlog->colossus_ad_live_item()) {
      for (auto& item : *(adlog->colossus_ad_live_item())) {
        if (redis_item_attr_type == RedisItemAttrType::ITEM_SPU) {
          request_keys.push_back("ltss_" + base::Uint64ToString(item.live_id) + "_"
              + base::Uint64ToString(item.timestamp / 60));
        } else if (redis_item_attr_type == RedisItemAttrType::ITEM_CID3) {
          request_keys.push_back("ltsc_" + base::Uint64ToString(item.live_id) + "_"
              + base::Uint64ToString(item.timestamp / 60));
        }
      }
    }
    if (request_keys.empty()) {
      return true;
    }
    std::vector<std::string> values;
    auto code = redis_client_->MGet(request_keys, &values);
    std::shared_ptr<std::unordered_map<std::string, int64_t>> val_list = nullptr;
    if (redis_item_attr_type == RedisItemAttrType::ITEM_SPU) {
      val_list = adlog->mutable_colossus_ad_live_spu();
    } else if (redis_item_attr_type == RedisItemAttrType::ITEM_CID3) {
      val_list = adlog->mutable_colossus_ad_live_cid3();
    }
    if (val_list != nullptr) {
      for (int i = 0; i < values.size(); i++) {
        int64_t val = -1;
        base::StringToInt64(values[i], &val);
        (*val_list)[request_keys[i]] = val;
      }
    }
    if (redis_item_attr_type == RedisItemAttrType::ITEM_SPU) {
      adlog->colossus_ad_live_spu_ready_ = true;
    } else if (redis_item_attr_type == RedisItemAttrType::ITEM_CID3) {
      adlog->colossus_ad_live_cid3_ready_ = true;
    }
    return true;
  }

 private:
  ks::infra::RedisClient* redis_client_ = nullptr;
};

enum class ColossusFilterKeyType {
  LIVE_PLAY_TIME = 1,
  LABEL = 2
};

template<ColossusType colossus_type, ColossusFilterKeyType filter_key_type,
         int max_list_len, int start_bit = 0, int bits = 0, int playtime_threshold = 0>
class ColossusFilterProcessor : public FeatureProcessor {
 public:
  ColossusFilterProcessor():FeatureProcessor() {
  }

  ~ColossusFilterProcessor() {}

  bool DoInit() override {
    LOG(INFO) << "ColossusFilterProcessor DoInit";
    return true;
  }

  bool DoProcess(ks::ad_algorithm::AdLogWrapper* adlog) override {
    auto colossus_result = GetColossusResult(adlog);
    if (colossus_result == nullptr) {
      return true;
    }
    auto& selected_item_idx = *colossus_result;
    if (colossus_type == ColossusType::AD_LIVE_ITEM) {
      GetFilteredItemIndex(adlog->colossus_ad_live_item(), adlog, &selected_item_idx);
    } else if (colossus_type == ColossusType::RECO_LIVE_ITEM) {
      GetFilteredItemIndex(adlog->colossus_reco_live_item(), adlog, &selected_item_idx);
    }
    SetProcessorReady(adlog);
    return true;
  }

 private:
    void SetProcessorReady(ks::ad_algorithm::AdLogWrapper* adlog) {
        if (colossus_type == ColossusType::AD_LIVE_ITEM) {
           if (filter_key_type == ColossusFilterKeyType::LIVE_PLAY_TIME) {
               adlog->ad_live_colossus_idx_filtered_by_playtime_ready_ = true;
               return;
           } else if (filter_key_type == ColossusFilterKeyType::LABEL) {
               adlog->ad_live_colossus_idx_filtered_by_label_ready_ = true;
               return;
           }
           return;
        }
        if (colossus_type == ColossusType::RECO_LIVE_ITEM) {
          if (filter_key_type == ColossusFilterKeyType::LIVE_PLAY_TIME) {
               adlog->reco_live_colossus_idx_filtered_by_playtime_ready_ = true;
               return;
          }
          return;
        }
    }

    void GetFilteredItemIndex(const std::shared_ptr<std::vector<colossus::AdLiveItemT>> colossus_item_list,
                             ks::ad_algorithm::AdLogWrapper* adlog,
                             std::vector<int>* selected_item_idx) {
        switch (filter_key_type) {
            case ColossusFilterKeyType::LIVE_PLAY_TIME:
                 FilterColossusItemByLivePlaytime(colossus_item_list, adlog, selected_item_idx);
                 break;
            case ColossusFilterKeyType::LABEL:
                 FilterColossusItemByLabel(colossus_item_list, adlog, selected_item_idx);
                 break;
            default:
                 return;
        }
    }

    void GetFilteredItemIndex(const std::shared_ptr<std::vector<colossus::LiveItemV2T>> colossus_item_list,
                             ks::ad_algorithm::AdLogWrapper* adlog,
                             std::vector<int>* selected_item_idx) {
        switch (filter_key_type) {
            case ColossusFilterKeyType::LIVE_PLAY_TIME:
                 FilterColossusItemByLivePlaytime(colossus_item_list, adlog, selected_item_idx);
                 break;
            default:
                 return;
        }
    }

    void FilterColossusItemByLivePlaytime(
      const std::shared_ptr<std::vector<colossus::AdLiveItemT>> colossus_item_list,
      ks::ad_algorithm::AdLogWrapper* adlog,
      std::vector<int>* selected_item_idx) {
        if (colossus_item_list == nullptr) {
          return;
        }
        const auto& items = *(colossus_item_list);
        int count = 0;
        for (int i = items.size() - 1; i >= 0; i--) {
           auto& item = items[i];
           if (item.timestamp > adlog->Get().time() / 1000 - 6000) {
                continue;
           }
           if (item.live_play_time < playtime_threshold) {
            continue;
           }
           selected_item_idx->emplace_back(i);
           count += 1;
           if (count >= max_list_len) break;
        }
    }

    void FilterColossusItemByLivePlaytime(
      const std::shared_ptr<std::vector<colossus::LiveItemV2T>> colossus_item_list,
      ks::ad_algorithm::AdLogWrapper* adlog,
      std::vector<int>* selected_item_idx) {
        if (colossus_item_list == nullptr) {
          return;
        }
        const auto& items = *(colossus_item_list);
        int count = 0;
        for (int i = items.size() - 1; i >= 0; i--) {
           auto& item = items[i];
           if (item.timestamp > adlog->Get().time() / 1000 - 6000) {
                continue;
           }
           if (item.play_time < playtime_threshold) {
            continue;
           }
           selected_item_idx->emplace_back(i);
           count += 1;
           if (count >= max_list_len) break;
        }
    }

    void FilterColossusItemByLabel(
      const std::shared_ptr<std::vector<colossus::AdLiveItemT>> colossus_item_list,
      ks::ad_algorithm::AdLogWrapper* adlog,
      std::vector<int>* selected_item_idx) {
        if (colossus_item_list == nullptr) {
          return;
        }
        const auto& items = *(colossus_item_list);
        int count = 0;
        for (int i = items.size() - 1; i >= 0; i--) {
           auto& item = items[i];
           if (item.timestamp > adlog->Get().time() / 1000 - 6000) {
                continue;
           }
           int label = (item.label >> start_bit) & ((1 << bits) - 1);
           if (label <= 1) {
               continue;
           }
           selected_item_idx->emplace_back(i);
           count += 1;
           if (count >= max_list_len) break;
        }
    }

    std::shared_ptr<std::vector<int>> GetColossusResult(
        ks::ad_algorithm::AdLogWrapper* adlog) {
    if (colossus_type == ColossusType::AD_LIVE_ITEM) {
      if (filter_key_type == ColossusFilterKeyType::LIVE_PLAY_TIME) {
        return adlog->mutable_ad_live_colossus_idx_filtered_by_playtime();
      } else if (filter_key_type == ColossusFilterKeyType::LABEL) {
        return adlog->mutable_ad_live_colossus_idx_filtered_by_label();
      }
    } else if (colossus_type == ColossusType::RECO_LIVE_ITEM) {
      if (filter_key_type == ColossusFilterKeyType::LIVE_PLAY_TIME) {
        return adlog->mutable_reco_live_colossus_idx_filtered_by_playtime();
      }
    }
    return nullptr;
  }
};

}  // namespace ad_algorithm
}  // namespace ks
