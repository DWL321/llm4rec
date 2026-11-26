#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <cstdint>
#include <fstream>
#include <vector>
#include <utility>
#include <string>
#include <random>
#include <thread>
#include <mutex>
#include <unordered_set>

#include "teams/ad/ad_nn/feature/ad_field_feature.h"
#include "teams/ad/ad_nn/feature/extractor_utils.h"
#include "teams/ad/ad_algorithm/log_preprocess/log_preprocess.h"
#include "base/common/gflags.h"
#include "base/common/base.h"
#include "base/common/logging.h"
#include "base/time/timestamp.h"
#include "base/strings/string_split.h"
#include "base/file/file_util.h"
#include "base/strings/string_printf.h"
#include "base/strings/string_number_conversions.h"
#include "base/encoding/base64.h"
#include "teams/ad/ad_proto/kuaishou/ad/algorithm/model/ad_joint_labeled_log.pb.h"
#include "teams/ad/ad_proto/kuaishou/ad/algorithm/model/ad_sample.pb.h"
#include "teams/ad/ad_proto/kuaishou/ad/algorithm/model/ad_feature.pb.h"
#include "base/encoding/line_escape.h"
#include "absl/strings/str_join.h"
#include "base/hash_function/md5.h"
#include "teams/ad/ad_algorithm/mr/tool.h"
#include "teams/ad/ad_algorithm/mr/common.h"
#include "teams/ad/ad_algorithm/mr/extract_feature.h"
#include "teams/ad/ad_nn/feature_extract/processors/constant.h"
#include "teams/ad/ad_nn/feature/preprocess/async_pdn_processor.h"
#include "teams/ad/ad_base/src/hash/hash.h"
#include "teams/ad/ad_algorithm/label_centre/label_centre_core.h"
#include "teams/ad/ad_algorithm/label_centre/label_centre_util.h"
#include "teams/ad/ad_algorithm/label_centre/label_centre_wrapper.h"

namespace ks {
namespace ad_algorithm {

using ks::ad_algorithm::LogPreprocess;
using ks::ad_algorithm::label_centre::LabelCentreMapping;

AdlogDataType guess_adlog_data_type(const std::string& line, const std::string& sep) {
  std::vector<std::string> results;
  base::SplitString(line, sep, &results);
  if (results.size() == 1) {
    return AdlogDataType::Whole;
  } else if (results.size() == 5 || results.size() == 6) {
    return AdlogDataType::Info;
  } else if (results.size() == 2) {
    LOG(FATAL) << "cannot guess adlog_data_type with two columns, "
               << " must specify adlog_data_type to UserExtend or ItemExtend";
  } else {
    LOG(FATAL) << "not supported, columns number must be 1, 5, 6, 2";
  }
  return AdlogDataType::Guess;
}

// ============= AdlogParser ===========
AdlogParser::AdlogParser() {
  LOG(INFO) << "tab: " << flags.tab << ", item_type: " << flags.item_type
            << ", item_filter: " << flags.item_filter
            << ", label_extractor: " << flags.label_extractor;
  if (flags.use_gsu) {
    colossus_client_.reset(
        new colossus::CommonItemClient("grpc_colossusSimAdLiveItemOffline"));
  }
  if (flags.use_reco_live_v1_gsu) {
    reco_colossus_v1_client_.reset(new colossus::CommonItemClient("grpc_colossusLiveItem"));
  }
  if (flags.use_reco_live_gsu) {
    reco_colossus_client_.reset(new colossus::CommonItemClient("grpc_colossusLiveItemV3"));
  }
  if (flags.use_remote_author_cluster) {
    author_enricher_.reset(new LiveAuthorClusterEnricher());
  }
  if (flags.use_remote_live_cluster) {
    live_cluster_enricher_.reset(new LiveRecoClusterEnricher());
  }
  if (flags.use_ad_goods_gsu) {
    ad_goods_colossus_client_.reset(
        new colossus::CommonItemClient("grpc_colossusSimAdGoodsItem"));
  }
  if (flags.use_ad_goods_gsu_new) {
    ad_goods_colossus_new_client_.reset(new colossus::CommonItemClient(
        "grpc_colossusSimAdGoodsItemNewOffline"));
  }
  std::vector<std::string> processor_names;
  if (flags.use_ad_live_global_gsu) {
    processor_names.push_back("AdLiveGlobalGsuProcessor");
  }
  if (flags.use_ad_live_redis_spu) {
    processor_names.push_back("RedisItemSpuProcessor");
  }
  if (flags.use_ad_live_redis_cid3) {
    processor_names.push_back("RedisItemCid3Processor");
  }
  if (flags.use_ad_live_ecomm_gsu) {
    processor_names.push_back("AdLiveEcommGsuProcessor");
  }
  if (flags.use_ad_live_author_gsu) {
    processor_names.push_back("AdLiveAuthorGsuProcessor");
  }
  if (flags.use_ad_live_author_cluster_gsu) {
    processor_names.push_back("AdLiveAuthorClusterGsuProcessor");
  }
  if (flags.use_ad_live_remote_cluster_gsu) {
    processor_names.push_back("AdLiveRemoteClusterGsuProcessor");
  }
  if (flags.use_reco_live_global_gsu) {
    processor_names.push_back("RecoLiveGlobalGsuProcessor");
  }
  if (flags.use_reco_live_author_gsu) {
    processor_names.push_back("RecoLiveAuthorGsuProcessor");
  }
  if (flags.use_reco_live_author_cluster_gsu) {
    processor_names.push_back("RecoLiveAuthorClusterGsuProcessor");
  }
  if (flags.use_reco_live_remote_cluster_gsu) {
    processor_names.push_back("RecoLiveRemoteClusterGsuProcessor");
  }
  if (flags.use_reco_live_v1_remote_cluster_gsu) {
    processor_names.push_back("RecoLiveV1RemoteClusterGsuProcessor");
  }
  if (flags.use_ad_live_spu_gsu) {
    processor_names.push_back("AdLiveSpuGsuProcessor");
  }
  if (flags.use_ad_live_cid3_gsu) {
    processor_names.push_back("AdLiveCid3GsuProcessor");
  }
  if (flags.use_ad_photo_gsu) {
    ad_colossus_client_.reset(new colossus::CommonItemClient(
        "grpc_colossusSimAdStateItemNewOffline"));
  }
  if (flags.use_ad_live_colossus_playtime_filter) {
    processor_names.push_back("AdLiveColossusPlaytimeFilterProcessor");
  }
  if (!processor_names.empty()) {
    feature_processor_ = std::make_unique<FeatureProcessor>();
    feature_processor_->Init(processor_names);
    LOG(INFO) << "feature_process_config init success";
  } else {
    LOG(INFO) << "feature_process_config not found";
  }

  processor = std::move(std::make_unique<LogPreprocess>(flags.tab, flags.item_type,
                               flags.item_filter, flags.label_extractor, flags.debug_info));
  adlog_data_type = parse_adlog_data_type(flags.adlog_data_type);
}

std::pair<bool, std::unique_ptr<AdLog>> AdlogParser::parse_adlog(
    const std::string &line, PreprocessType preprocess_type, tensorflow::Tensor *t,
    std::vector<std::string> *debug_info_string) {
  return parse_adlog(line, preprocess_type, t, debug_info_string, 0);
}

bool AdlogParser::CallColossusWithRetry(
    const std::unique_ptr<colossus::CommonItemClient>& colossus_client,
    const uint64_t user_id, colossus::CommonItemResponse* resp) {
  int retry = 0;
  bool has_valid_resp = false;
  static const int kMaxSleepCount = 30;
  while (retry++ < flags.rpc_max_retry_count ||
         flags.rpc_max_retry_count <= 0) {
    auto pms = std::make_shared<std::promise<colossus::CommonItemResponse*>>();
    // 1. async request
    auto ok = colossus_client->AsyncBrpcQueryItem(user_id, resp, pms);
    if (!ok) {
      LOG_EVERY_N(WARNING, 1000) << "get_user_sim_failed: " << user_id;
      const int sleep_count = retry > kMaxSleepCount ? kMaxSleepCount : retry;
      std::this_thread::sleep_for(std::chrono::seconds(1) * sleep_count);
      continue;
    }

    // 2. judge response
    auto fut = pms->get_future();
    colossus::CommonItemResponse* common_item_response_ptr = fut.get();
    if (common_item_response_ptr == nullptr ||
        common_item_response_ptr->err_code() ==
            ::colossus::SimErrorCode::SIM_ERR_SERVER_BUSY) {
      LOG_EVERY_N(WARNING, 1000)
          << "colossus future get is nullptr, user_id: " << user_id;
      const int sleep_count = retry > kMaxSleepCount ? kMaxSleepCount : retry;
      std::this_thread::sleep_for(std::chrono::seconds(1) * sleep_count);
      continue;
    } else {
      has_valid_resp = true;
      break;
    }
  }
  if (flags.fatal_if_rpc_retry_error && false == has_valid_resp) {
    LOG(FATAL) << "fatal after retry too many times";
    return false;
  }
  return has_valid_resp;
}

std::pair<bool, std::unique_ptr<AdLog>> AdlogParser::parse_adlog(const std::string &line,
                                                                 PreprocessType preprocess_type,
                                                                 tensorflow::Tensor *t,
                                                                 std::vector<std::string> *debug_info_string,
                                                                 int move_item_mode) {
  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);

  std::pair<bool, std::unique_ptr<ks::ad_algorithm::AdLog>> res(false, nullptr);
  if (results.size() == 1) {
  // if (results.size() == 2) {
    ks::ad_algorithm::AdLogWrapper adlog_wrapper;
    // adlog_wrapper.live_segment_action_list = results[1];

    string bline;
    if (!base::Base64Decode(line, &bline)) {
    // if (!base::Base64Decode(results[0], &bline)) {
      LOG(ERROR) << "parser base64 failed";
      return res;
    }
    if (adlog_data_type == AdlogDataType::AdJointReco) {
      if (!reco_with_ad_log_parser.parse(bline, &adlog_wrapper)) {
        LOG_EVERY_N(ERROR, 100) << "parser reco_with_ad_log failed";
        return res;
      }
    } else if (adlog_data_type == AdlogDataType::AdJointRecoAndMerchant) {
      if (!raw_sample_log_parser.parse(bline, &adlog_wrapper)) {
        LOG_EVERY_N(ERROR, 100) << "parser raw_sample_package log failed";
        return res;
      }
    } else {
      if (!adlog_wrapper.parse_from(bline)) {
        LOG(ERROR) << "parser adlog failed";
        return res;
      }
    }

    auto reco_user_info = adlog_wrapper.mutable_reco_user_info();
    if (adlog_wrapper.Get().serialized_reco_user_info().empty() ||
        !reco_user_info->ParseFromString(adlog_wrapper.Get().serialized_reco_user_info())) {
      LOG_EVERY_N(WARNING, 10000) << "reco_user_info empty or parse reco_user_info error";
    }

    auto user_sample_attrs = adlog_wrapper.mutable_merchant_user_attrs();
    if (!adlog_wrapper.Get().has_user_info()
        || adlog_wrapper.user_info().serialized_merchant_user_attrs().empty()
        || !user_sample_attrs->ParseFromString(
            adlog_wrapper.Get().user_info().serialized_merchant_user_attrs())) {
      LOG_EVERY_N(WARNING, 10000) << "merchant_user_info empty or parse error";
    }

    for (size_t i = 0; i < adlog_wrapper.Get().item_size(); ++i) {
      auto& item = adlog_wrapper.Get().item(i);
      if (item.has_merchant_info() && !item.merchant_info().serialized_merchant_item_attrs().empty()) {
        auto item_sample_attrs = adlog_wrapper.mutable_merchant_item_attrs(i);
        if (!item_sample_attrs->ParseFromString(item.merchant_info().serialized_merchant_item_attrs())) {
          LOG_EVERY_N(WARNING, 10000) << "merchant_item_attrs parse error";
        }
      }
    }

    if (preprocess_type == PreprocessType::All) {
      if (!processor->preprocess(&adlog_wrapper, t, debug_info_string)) {
        return res;
      }
    } else if (preprocess_type == PreprocessType::Detail) {
      if (!processor->preprocess_detail(&adlog_wrapper, t, true)) {
        return res;
      }
    } else if (preprocess_type == PreprocessType::DetailCount) {
      if (!processor->preprocess_detail(&adlog_wrapper, t, false)) {
        return res;
      }
    }
    // move_item_mode - 0: move 1: copy 2: do nothing
    if (move_item_mode == 1) {
      adlog_wrapper.copy_items();
    } else if (move_item_mode == 0) {
      adlog_wrapper.move_items();
    }

    if (flags.use_gsu && colossus_client_) {
      if (adlog_wrapper.has_user_info()) {
        do {
          auto user_id = adlog_wrapper.user_info().id();
          colossus::CommonItemResponse common_item_response;

          // 1. request colossus with retry
          if (!CallColossusWithRetry(colossus_client_, user_id,
                                     &common_item_response)) {
            LOG_EVERY_N(WARNING, 1000)
                << "failed to request data from colosuss after retry. user_id="
                << user_id;
            break;
          }

          // 2. deal with response
          int item_num = 0;
          colossus::AdLiveItemT const *start_addr = nullptr;
          if (!colossus_client_->DecodeCommonItemResponse(&common_item_response, &start_addr, &item_num)) {
            LOG_EVERY_N(WARNING, 1000) << "decode colossus response failed, user_id: " << user_id;
            break;
          }
          auto ad_live_item = adlog_wrapper.mutable_colossus_ad_live_item();
          ad_live_item->resize(item_num);
          std::copy_n(start_addr, item_num, ad_live_item->begin());
          LOG_EVERY_N(INFO, 10000) << "ad live item colossus success, user_id: " << user_id
                                  << " item size: " << ad_live_item->size();
          adlog_wrapper.colossus_ad_live_item_ready_ = true;
        } while (false);
      }
    }
    if (flags.use_ad_goods_gsu && ad_goods_colossus_client_) {
      if (adlog_wrapper.has_user_info()) {
        do {
          auto user_id = adlog_wrapper.user_info().id();
          auto pms =
              std::make_shared<std::promise<colossus::CommonItemResponse*>>();
          colossus::CommonItemResponse common_item_response;
          // 1. async request
          auto ok = ad_goods_colossus_client_->AsyncBrpcQueryItem(
              user_id, &common_item_response, pms);
          if (!ok) {
            LOG_EVERY_N(WARNING, 1000) << "get_user_sim_failed: " << user_id;
            break;
          }

          // 2. judge response
          auto fut = pms->get_future();
          colossus::CommonItemResponse* common_item_response_ptr = fut.get();
          if (common_item_response_ptr == nullptr) {
            LOG_EVERY_N(WARNING, 1000)
                << "colossus future get is nullptr, user_id: " << user_id;
            break;
          }

          // 3. deal with response
          int item_num = 0;
          colossus::AdGoodsItemT const* start_addr = nullptr;
          if (!ad_goods_colossus_client_->DecodeCommonItemResponse(
                  &common_item_response, &start_addr, &item_num)) {
            LOG_EVERY_N(WARNING, 1000)
                << "decode colossus response failed, user_id: " << user_id;
            break;
          }
          auto ad_goods_item = adlog_wrapper.mutable_colossus_ad_goods_item();
          ad_goods_item->resize(item_num);
          std::copy_n(start_addr, item_num, ad_goods_item->begin());
          LOG_EVERY_N(INFO, 10000)
              << "ad goods item colossus success, user_id: " << user_id
              << " item size: " << ad_goods_item->size();

          // LOG(INFO) << "Query ok, item_key: " << user_id << " item_num: " <<
          // item_num; for (int i = 0; i < item_num; ++i) {
          //   LOG(INFO) << "item[" << i << "]: " << start_addr[i].DebugInfo();
          // }
          adlog_wrapper.colossus_ad_goods_item_ready_ = true;
        } while (false);
      }
    }
    if (flags.use_ad_goods_gsu_new && ad_goods_colossus_new_client_) {
      if (adlog_wrapper.has_user_info()) {
        do {
          auto user_id = adlog_wrapper.user_info().id();
          colossus::CommonItemResponse common_item_response;

          // 1. request colossus with retry
          if (!CallColossusWithRetry(ad_goods_colossus_new_client_, user_id,
                                     &common_item_response)) {
            LOG_EVERY_N(WARNING, 1000)
                << "failed to request data from colosuss after retry. user_id="
                << user_id;
            break;
          }

          // 2. deal with response
          int item_num = 0;
          colossus::AdGoodsItemNewT const* start_addr = nullptr;
          if (!ad_goods_colossus_new_client_->DecodeCommonItemResponse(
                  &common_item_response, &start_addr, &item_num)) {
            LOG_EVERY_N(WARNING, 1000)
                << "decode colossus response failed, user_id: " << user_id;
            break;
          }
          auto ad_goods_item_new = adlog_wrapper.mutable_colossus_ad_goods_item_new();
          ad_goods_item_new->resize(item_num);
          std::copy_n(start_addr, item_num, ad_goods_item_new->begin());
          LOG_EVERY_N(INFO, 10000)
              << "ad goods item new colossus success, user_id: " << user_id
              << " item size: " << ad_goods_item_new->size();

          // LOG(INFO) << "ad goods item new Query ok, item_key: " << user_id << " item_num: " <<
          // item_num; for (int i = 0; i < item_num; ++i) {
          //   LOG(INFO) << "item[" << i << "]: " << start_addr[i].DebugInfo();
          // }
          adlog_wrapper.colossus_ad_goods_item_new_ready_ = true;
          ks::ad_nn::PostProcessAdGoodsSimV3(&adlog_wrapper);
        } while (false);
      }
    }
    if (flags.use_reco_live_v1_gsu && reco_colossus_v1_client_) {
      if (adlog_wrapper.has_user_info()) {
        do {
          auto user_id = adlog_wrapper.user_info().id();
          auto pms = std::make_shared<std::promise<colossus::CommonItemResponse *>>();
          colossus::CommonItemResponse common_item_response;
          // 1. async request
          auto ok = reco_colossus_v1_client_->AsyncBrpcQueryItem(user_id, &common_item_response, pms);
          if (!ok) {
            LOG_EVERY_N(WARNING, 1000) << "get_user_sim_failed: " << user_id;
            break;
          }

          // 2. judge response
          auto fut = pms->get_future();
          colossus::CommonItemResponse *common_item_response_ptr = fut.get();
          if (common_item_response_ptr == nullptr) {
            LOG_EVERY_N(WARNING, 1000) << "colossus future get is nullptr, user_id: " << user_id;
            break;
          }

          // 3. deal with response
          int item_num = 0;
          colossus::LiveItemT const *start_addr = nullptr;
          if (!reco_colossus_v1_client_->DecodeCommonItemResponse(
                &common_item_response, &start_addr, &item_num)) {
            LOG_EVERY_N(WARNING, 1000) << "decode colossus response failed, user_id: " << user_id;
            break;
          }
          auto reco_live_item = adlog_wrapper.mutable_colossus_reco_live_v1_item();
          reco_live_item->resize(item_num);
          std::copy_n(start_addr, item_num, reco_live_item->begin());
          LOG_EVERY_N(INFO, 10000) << "ad live item colossus success, user_id: " << user_id
                                  << " item size: " << reco_live_item->size();
          adlog_wrapper.colossus_reco_live_v1_item_ready_ = true;
        } while (false);
      }
    }
    if (flags.use_reco_live_gsu && reco_colossus_client_) {
      if (adlog_wrapper.has_user_info()) {
        do {
          auto user_id = adlog_wrapper.user_info().id();
          auto pms = std::make_shared<std::promise<colossus::CommonItemResponse *>>();
          colossus::CommonItemResponse common_item_response;
          // 1. async request
          auto ok = reco_colossus_client_->AsyncBrpcQueryItem(user_id, &common_item_response, pms);
          if (!ok) {
            LOG_EVERY_N(WARNING, 1000) << "get_user_sim_failed: " << user_id;
            break;
          }

          // 2. judge response
          auto fut = pms->get_future();
          colossus::CommonItemResponse *common_item_response_ptr = fut.get();
          if (common_item_response_ptr == nullptr) {
            LOG_EVERY_N(WARNING, 1000) << "colossus future get is nullptr, user_id: " << user_id;
            break;
          }

          // 3. deal with response
          int item_num = 0;
          colossus::LiveItemV2T const *start_addr = nullptr;
          if (!reco_colossus_client_->DecodeCommonItemResponse(
                &common_item_response, &start_addr, &item_num)) {
            LOG_EVERY_N(WARNING, 1000) << "decode colossus response failed, user_id: " << user_id;
            break;
          }
          auto reco_live_item = adlog_wrapper.mutable_colossus_reco_live_item();
          reco_live_item->resize(item_num);
          std::copy_n(start_addr, item_num, reco_live_item->begin());
          LOG_EVERY_N(INFO, 10000) << "ad live item colossus success, user_id: " << user_id
                                  << " item size: " << reco_live_item->size();
          adlog_wrapper.colossus_reco_live_item_ready_ = true;
        } while (false);
      }
    }
    if (flags.use_ad_photo_gsu && ad_colossus_client_) {
      if (adlog_wrapper.has_user_info()) {
        do {
          auto user_id = adlog_wrapper.user_info().id();
          colossus::CommonItemResponse common_item_response;

          // 1. request colossus with retry
          if (!CallColossusWithRetry(ad_colossus_client_, user_id,
                                     &common_item_response)) {
            LOG_EVERY_N(WARNING, 1000)
                << "failed to request data from colosuss after retry. user_id="
                << user_id;
            break;
          }

          // 2. deal with response
          int item_num = 0;
          colossus::SimItemV2T const *start_addr = nullptr;
          if (!ad_colossus_client_->DecodeCommonItemResponse(&common_item_response, &start_addr, &item_num)) {
            LOG_EVERY_N(WARNING, 1000) << "ad photo decode colossus response failed, user_id: " << user_id;
            break;
          }
          auto ad_photo_item = adlog_wrapper.mutable_colossus_reco_photo();
          ad_photo_item->resize(item_num);
          std::copy_n(start_addr, item_num, ad_photo_item->begin());
          LOG_EVERY_N(INFO, 10000) << "ad photo item colossus success, user_id: " << user_id
                                  << " item size: " << ad_photo_item->size();
          adlog_wrapper.colossus_reco_photo_ready_ = true;
        } while (false);
      }
    }
    if (flags.use_remote_author_cluster && author_enricher_) {
      do {
        std::unordered_set<int64> request_aids;
        uint64_t time = adlog_wrapper.Get().time() / 1000;
        if (adlog_wrapper.colossus_ad_live_item()) {
          for (auto& item : *(adlog_wrapper.colossus_ad_live_item())) {
            if (item.timestamp > time) {
              continue;
            }
            request_aids.emplace(item.author_id);
          }
        }
        if (adlog_wrapper.colossus_reco_live_item()) {
          for (auto& item : *(adlog_wrapper.colossus_reco_live_item())) {
            if (item.timestamp > time) {
              continue;
            }
            request_aids.emplace(item.author_id);
          }
        }
        if (request_aids.empty()) {
          break;
        }
        for (int i = 0; i < adlog_wrapper.item_size(); i++) {
          int64 author_id = get_author_id_from_item(adlog_wrapper.item(i));
          if (author_id > 0) {
            request_aids.emplace(author_id);
          }
        }
        author_enricher_->FetchClusterIdsFromEmbeddingServer(request_aids,
            adlog_wrapper.mutable_author_cluster_map());
        adlog_wrapper.author_cluster_map_ready_ = true;
      } while (false);
    }

    if (flags.use_remote_live_cluster && live_cluster_enricher_) {
      do {
        std::unordered_set<int64> request_ids;
        for (int i = 0; i < adlog_wrapper.item_size(); i++) {
          int64 live_id = get_live_id_from_item(adlog_wrapper.item(i));
          if (live_id > 0) {
            request_ids.emplace(live_id);
          }
        }
        if (request_ids.empty()) {
          break;
        }
        live_cluster_enricher_->FetchClusterIdsFromEmbeddingServer(request_ids,
            adlog_wrapper.mutable_live_cluster_map());
        adlog_wrapper.live_cluster_map_ready_ = true;
      } while (false);
    }

    if (feature_processor_) {
      feature_processor_->Process(&adlog_wrapper);
    }
    res.first = true;
    res.second = std::move(std::make_unique<ks::ad_algorithm::AdLogWrapper>(adlog_wrapper));
    res.second->set_is_train(true);
    return res;

  } else if (results.size() >= 5) {
    ks::ad_algorithm::AdLogExtend adlog_extend;
    std::string user_bytes;
    std::string item_bytes;

    if (!base::Base64Decode(results[0], &user_bytes) ||
        !adlog_extend.Get().mutable_user_info()->ParseFromString(user_bytes)) {
      LOG(ERROR) << "parser user info failed, results.size(): " << results.size();
      return res;
    }

    if (!base::Base64Decode(results[1], &item_bytes)
        || !adlog_extend.Get().add_item()->ParseFromString(item_bytes)) {
      LOG(ERROR) << "parse item info failed";
      return res;
    }

    adlog_extend.SetUserExtend(results[2]);
    adlog_extend.AddItemExtend(results[3]);

    uint64_t time1 = 0;
    if (!base::StringToUint64(results[4], &time1)) {
      LOG(ERROR) << "parse time failed" << results[4];
      return res;
    }
    adlog_extend.Get().set_time(time1);

    std::string context_bytes;
    if (results.size() > 5 && !results[5].empty()) {
      if (!base::Base64Decode(results[5], &context_bytes)
          || !adlog_extend.Get().mutable_context()->ParseFromString(context_bytes)) {
        LOG(ERROR) << "parse context info failed";
        return res;
      }
    }

    if (preprocess_type == PreprocessType::All) {
      if (!processor->preprocess(&adlog_extend, t, debug_info_string)) {
        return res;
      }
    } else if (preprocess_type == PreprocessType::Detail) {
      if (!processor->preprocess_detail(&adlog_extend, t)) {
        return res;
      }
    }
    adlog_extend.move_items();

    res.first = true;
    res.second = std::move(std::make_unique<ks::ad_algorithm::AdLogExtend>(adlog_extend));
    res.second->set_is_train(true);
    return res;

  } else if (adlog_data_type == AdlogDataType::UserExtend) {
    // user_id, extend, p_date, p_hourmin

    ks::ad_algorithm::AdLogExtend adlog_extend;

    uint64 user_id = 0;
    if (!base::StringToUint64(results[0], &user_id)) {
      LOG(ERROR) << "parse user_id failed, user_id: " << results[0];
      return res;
    }

    adlog_extend.Get().mutable_user_info()->set_id(user_id);
    adlog_extend.SetUserExtend(results[1]);

    auto item_info = adlog_extend.Get().add_item();
    item_info->set_id(0);
    adlog_extend.move_items();

    parse_partition_time(results, &adlog_extend);

    res.first = true;
    res.second = std::move(std::make_unique<ks::ad_algorithm::AdLogExtend>(adlog_extend));
    res.second->set_is_train(true);
    return res;

  } else if (adlog_data_type == AdlogDataType::ItemExtend) {
    ks::ad_algorithm::AdLogExtend adlog_extend;

    uint64 item_id = 0;
    if (!base::StringToUint64(results[0], &item_id)) {
      LOG(ERROR) << "parse user_id failed, item_id: " << results[0];
      return res;
    }

    auto item_info = adlog_extend.Get().add_item();
    item_info->set_id(item_id);
    adlog_extend.AddItemExtend(results[1]);
    adlog_extend.move_items();

    parse_partition_time(results, &adlog_extend);

    res.first = true;
    res.second = std::move(std::make_unique<ks::ad_algorithm::AdLogExtend>(adlog_extend));
    res.second->set_is_train(true);
    return res;

  } else if (adlog_data_type == AdlogDataType::UnitExtend) {
    ks::ad_algorithm::AdLogExtend adlog_extend;
    uint64 unit_id = 0;
    if (!base::StringToUint64(results[0], &unit_id)) {
      LOG(ERROR) << "parse unit_id failed, unit_id: " << results[0];
      return res;
    }
    auto item_info = adlog_extend.Get().add_item();
    item_info->mutable_ad_dsp_info()->mutable_creative()->mutable_base()->set_unit_id(unit_id);
    adlog_extend.AddItemExtend(results[1]);

    parse_partition_time(results, &adlog_extend);

    adlog_extend.move_items();
    res.first = true;
    res.second = std::move(std::make_unique<ks::ad_algorithm::AdLogExtend>(adlog_extend));
    res.second->set_is_train(true);

    return res;

  } else if (adlog_data_type == AdlogDataType::PhotoExtend) {
    ks::ad_algorithm::AdLogExtend adlog_extend;

    uint64 photo_id = 0;
    if (!base::StringToUint64(results[0], &photo_id)) {
      LOG(ERROR) << "parse photo_id failed, photo_id: " << results[0];
      return res;
    }

    auto item_info = adlog_extend.Get().add_item();
    item_info->mutable_ad_dsp_info()->mutable_photo_info()->set_id(photo_id);

    adlog_extend.AddItemExtend(results[1]);

    parse_partition_time(results, &adlog_extend);

    adlog_extend.move_items();

    res.first = true;
    res.second = std::move(std::make_unique<ks::ad_algorithm::AdLogExtend>(adlog_extend));
    res.second->set_is_train(true);
    return res;

  } else if (adlog_data_type == AdlogDataType::AccountExtend) {
    ks::ad_algorithm::AdLogExtend adlog_extend;

    uint64 account_id = 0;
    if (!base::StringToUint64(results[0], &account_id)) {
      LOG(ERROR) << "parse account_id failed, account_id: " << results[0];
      return res;
    }

    auto item_info = adlog_extend.Get().add_item();
    item_info->mutable_ad_dsp_info()->mutable_creative()->mutable_base()->set_account_id(account_id);

    adlog_extend.AddItemExtend(results[1]);

    parse_partition_time(results, &adlog_extend);

    adlog_extend.move_items();

    res.first = true;
    res.second = std::move(std::make_unique<ks::ad_algorithm::AdLogExtend>(adlog_extend));
    res.second->set_is_train(true);
    return res;

  } else if (adlog_data_type == AdlogDataType::AuthorExtend) {
    ks::ad_algorithm::AdLogExtend adlog_extend;

    uint64 author_id = 0;
    if (!base::StringToUint64(results[0], &author_id)) {
      LOG(ERROR) << "parse author_id failed, author_id: " << results[0];
      return res;
    }

    auto item_info = adlog_extend.Get().add_item();
    item_info->mutable_ad_dsp_info()->mutable_photo_info()->mutable_author_info()->set_id(author_id);
    adlog_extend.AddItemExtend(results[1]);
    parse_partition_time(results, &adlog_extend);
    adlog_extend.move_items();

    res.first = true;
    res.second = std::move(std::make_unique<ks::ad_algorithm::AdLogExtend>(adlog_extend));
    res.second->set_is_train(true);
    return res;

  } else if (adlog_data_type == AdlogDataType::ContextExtend) {
    ks::ad_algorithm::AdLogExtend adlog_extend;

    uint64 llsid = 0;
    if (!base::StringToUint64(results[0], &llsid)) {
      LOG(ERROR) << "parse llsid failed, llsid: " << results[0];
      return res;
    }

    adlog_extend.Get().set_llsid(llsid);
    auto item_info = adlog_extend.Get().add_item();

    adlog_extend.AddItemExtend(results[1]);
    parse_partition_time(results, &adlog_extend);

    adlog_extend.move_items();

    res.first = true;
    res.second = std::move(std::make_unique<ks::ad_algorithm::AdLogExtend>(adlog_extend));
    res.second->set_is_train(true);
    return res;

  } else if (adlog_data_type == AdlogDataType::CombineExtend) {
    if (results.size() < 3) {
      LOG(FATAL) << "must provide user_id, item_id, extend";
    }

    ks::ad_algorithm::AdLogExtend adlog_extend;

    uint64 user_id = 0;
    if (!base::StringToUint64(results[0], &user_id)) {
      LOG(ERROR) << "parse user_id failed, user_id: " << results[0];
      return res;
    }

    adlog_extend.Get().mutable_user_info()->set_id(user_id);

    uint64 item_id = 0;
    if (!base::StringToUint64(results[1], &item_id)) {
      LOG(ERROR) << "parse user_id failed, item_id: " << results[0];
      return res;
    }

    auto item_info = adlog_extend.Get().add_item();
    item_info->set_id(item_id);
    adlog_extend.AddItemExtend(results[2]);

    parse_partition_time(results, &adlog_extend, 3);

    adlog_extend.move_items();


    res.first = true;
    res.second = std::move(std::make_unique<ks::ad_algorithm::AdLogExtend>(adlog_extend));
    res.second->set_is_train(true);
    return res;

  } else {
    LOG(FATAL) << "unsuported adlog_data_type: " << adlog_data_type;
  }

  res.second->set_is_train(true);

  return res;
}

// ============= ExtractFeature ===========
void ExtractFeature::init() {
  if (flags.ks_feature_config_path.size() == 0) {
    LOG(INFO) << "empty feature file, return";
    return;
  }

  feature_file_info = std::move(std::make_shared<FeatureFileInfo>());
  if (flags.ks_feature_config_path.size() > 0) {
    const base::FilePath file_path(flags.ks_feature_config_path);
    if (!base::file_util::PathExists(file_path)) {
      LOG(INFO) << "feature file: " << flags.ks_feature_config_path << " not exists!!!";
    } else {
      FeatureFileInfo::LoadFeatureFile(flags.ks_feature_config_path, feature_file_info.get(),
       true, true, flags.dragon_config_file);
    }
  }
  std::unique_ptr<AdFieldFeature> tmp_ptr(new AdFieldFeature({feature_file_info.get()}));
  feature_extractor = std::move(tmp_ptr);
  dense_field_value = std::vector<std::vector<float>>();
  for (int i = 0; i < feature_file_info->dense_slots_count; ++i) {
    dense_field_value.emplace_back(feature_file_info->dense_slots_size[i], 0.0);
  }

  gen = std::mt19937(rd());
  dist = std::uniform_int_distribution<int>(1, 100000);
  sparse_feature_type_sorted.push_back(FeatureType::RECO_USER);
  sparse_feature_type_sorted.push_back(FeatureType::USER);
  sparse_feature_type_sorted.push_back(FeatureType::ITEM);
  sparse_feature_type_sorted.push_back(FeatureType::RECO_ITEM);
  sparse_feature_type_sorted.push_back(FeatureType::COMBINE);

  dense_feature_type_sorted.push_back(FeatureType::DENSE_USER);
  dense_feature_type_sorted.push_back(FeatureType::DENSE_ITEM);
  dense_feature_type_sorted.push_back(FeatureType::DENSE_COMBINE);
  // construct_btq_config_and_init(); // just for debug

  bool is_nohash_value = is_nohash(feature_file_info->slots);
  std::unique_ptr<AdFeatureExtractorBundle> tmp_bundle_ptr =
    std::make_unique<AdFeatureExtractorBundle>(feature_file_info,
                                               ExtractType::ALL,
                                               FeatureOrgLayout::BY_FIELD,
                                               is_nohash_value);
  if (is_nohash_value) {
    auto nohash_convert = [](FeatureType type, uint64_t sign) {
      return sign;
    };
    tmp_bundle_ptr->SetSignConvert(nohash_convert);
  }
  ad_fea_ext_bundle_ptr = std::move(tmp_bundle_ptr);
  total_feature_count = feature_file_info->slots[feature_file_info->slots_count-1].slot_index + 1;
  if (!flags.dragon_config_file.empty()) {
#ifdef BUILD_WITH_DRAGON
    std::string json_str;
    if (!base::file_util::ReadFileToString(flags.dragon_config_file, &json_str)) {
      LOG(WARNING) << "read config from file " << flags.dragon_config_file << " failed";
      return;
    }
    dragon_config.reset(new base::Json(base::StringToJson(json_str)));
    FLAGS_logging_switch_uid_mod_divisor = dragon_config->GetInt("dragon_log_rate", 0);
    auto dragon_pipeline = dragon_config->Get("pipeline_manager_config");
    if (dragon_pipeline == nullptr) {
      LOG(FATAL) << "no dragon pipeline config found";
    }
    auto run_json = dragon_config->Get("run_pipelines");
    if (run_json != nullptr && run_json->IsArray() && run_json->size() > 0) {
      run_pipelines_.resize(run_json->size());
      for (int idx = 0; idx < run_json->size(); ++idx) {
        if (!run_json->GetString(idx, &run_pipelines_[idx])) {
          LOG(FATAL) << "run_pipelines parse failed idx " << idx;
        }
      }
    }
    auto slot_keys_json = dragon_config->Get("dragon_result_slot_key");
    if (slot_keys_json != nullptr && slot_keys_json->IsArray() && slot_keys_json->size() > 0) {
      item_dragon_slot_keys_.resize(slot_keys_json->size());
      for (int idx = 0; idx < slot_keys_json->size(); ++idx) {
        if (!slot_keys_json->GetString(idx, &item_dragon_slot_keys_[idx])) {
          LOG(FATAL) << "dragon_result_slot_keys_ parse failed idx " << idx;
        }
      }
    }
    auto sign_keys_json = dragon_config->Get("dragon_result_sign_key");
    if (sign_keys_json != nullptr && sign_keys_json->IsArray() && sign_keys_json->size() > 0) {
      item_dragon_sign_keys_.resize(sign_keys_json->size());
      for (int idx = 0; idx < sign_keys_json->size(); ++idx) {
        if (!sign_keys_json->GetString(idx, &item_dragon_sign_keys_[idx])) {
          LOG(FATAL) << "dragon_result_sign_keys_ parse failed idx " << idx;
        }
      }
    }
    auto user_slot_keys_json = dragon_config->Get("user_dragon_slot_key");
    if (user_slot_keys_json != nullptr && user_slot_keys_json->IsArray() && user_slot_keys_json->size() > 0) {
      user_dragon_slot_keys_.resize(user_slot_keys_json->size());
      for (int idx = 0; idx < user_slot_keys_json->size(); ++idx) {
        if (!user_slot_keys_json->GetString(idx, &user_dragon_slot_keys_[idx])) {
          LOG(FATAL) << "dragon_common_slot_keys_ parse failed idx " << idx;
        }
      }
    }
    auto user_sign_keys_json = dragon_config->Get("user_dragon_sign_key");
    if (user_sign_keys_json != nullptr && user_sign_keys_json->IsArray() && user_sign_keys_json->size() > 0) {
      user_dragon_sign_keys_.resize(user_sign_keys_json->size());
      for (int idx = 0; idx < user_sign_keys_json->size(); ++idx) {
        if (!user_sign_keys_json->GetString(idx, &user_dragon_sign_keys_[idx])) {
          LOG(FATAL) << "dragon_common_sign_keys_ parse failed idx " << idx;
        }
      }
    }
    auto dragon_feature_config = dragon_config->Get("dragon_feature_config");
    if (dragon_feature_config) {
      for (const auto &feature_config : dragon_feature_config->array()) {
        auto class_name = feature_config->GetString("class_name", "");
        if (class_name == "") {
          LOG(ERROR) << "Invalid class_name value " << class_name;
          continue;
        }
        auto field = feature_config->GetInt("field", -1);
        if (field == -1) {
          LOG(ERROR) << "Invalid field value " << field;
          continue;
        }
        auto slot_size = feature_config->GetInt("size", 0);
        auto remap_slot = feature_config->GetInt("remap_slot", -1);
        if (remap_slot < 0) {
          LOG(ERROR) << "Invalid remap_slot value " << remap_slot;
          continue;
        }
        auto category = feature_config->GetString("category", "combine");
        if (category == "user") {
          user_dragon_features_[remap_slot] = class_name;
          user_dragon_fields_[remap_slot] = field;
        } else if (category == "photo" || category == "combine") {
          item_dragon_features_[remap_slot] = class_name;
          item_dragon_fields_[remap_slot] = field;
        } else if (category == "dense_user") {
          dense_user_dragon_fields_[class_name] = field;
          dense_user_dragon_sizes_[field] = slot_size;
        } else if (category == "dense_photo" || category == "dense_combine") {
          dense_item_dragon_fields_[class_name] = field;
          dense_item_dragon_sizes_[field] = slot_size;
        } else {
          LOG(ERROR) << "Unsupported caetgory " << category;
          continue;
        }

        // if (field + 1 > total_feature_count) {
        //   total_feature_count = field + 1;
        // }
        LOG(INFO) << "success init a dragon feature config class_name: " << class_name
                  << ", field: " << field;
      }
    }

#else
  LOG(FATAL) << "dragon pipeline must run with BUILD_WITH_DRAGON build option set";
#endif
  }
}

bool ExtractFeature::is_nohash(const std::vector<ks::ad_nn::SlotConfig>& slots) {
  for (const auto& slot : slots) {
    if (slot.slot_size == 0) {
      return true;
    }
  }
  return false;
}

ExtractFeature::ExtractFeature() {
  init();
  adlog_data_type = parse_adlog_data_type(flags.adlog_data_type);
  if (adlog_data_type == AdlogDataType::Whole ||
      adlog_data_type == AdlogDataType::Info ||
      adlog_data_type == AdlogDataType::AdJointReco ||
      adlog_data_type == AdlogDataType::AdJointRecoAndMerchant ||
      adlog_data_type == AdlogDataType::Guess) {
    is_need_label = true;
  } else {
    is_need_label = false;
  }
  extend_field = parse_extend_field(adlog_data_type, flags.extend_field);
}

std::vector<std::vector<std::string>> ExtractFeature::process_line(std::string* line) {
  return extract_feature_frame(*line, adlog_data_type);
}

std::vector<AdPersiaFeature> ExtractFeature::extract_feature_label(const std::string& line,
                                                                   AdlogDataType adlog_data_type) {
  std::vector<AdPersiaFeature> vec;
  ks::ad_algorithm::AdLog* p_adlog;
  tensorflow::Tensor t;
  std::vector<std::string> extra_fields;

  std::pair<bool, std::unique_ptr<ks::ad_algorithm::AdLog>> parse_res =
    adlog_parser.parse_adlog(line, PreprocessType::All, &t);
  if (!parse_res.first) {
    return vec;
  }

  p_adlog = parse_res.second.get();

  sparse_results.clear();
  dense_results.clear();
  feature_extractor->ExtractAllAdLogFeatures(*p_adlog, &sparse_results);
  feature_extractor->ExtractAllAdLogDenseFeatures(*p_adlog, &dense_results);

  if (is_need_label) {
    for (size_t i = 0; i < p_adlog->item_size(); ++i) {
      AdPersiaFeature persia_feature = extract_persia_feature(*p_adlog, i);
      const auto & label_vec = get_label_value(t, i);
      if (label_vec.size() > 0 && label_vec[0] != -1) {
        if (flags.ns < 1.0 && !label_vec[0]) {
          int rand_val = dist(gen);
          if (rand_val > (int)(flags.ns * 100000)) {
            continue;
          }
        }
        for (auto value : label_vec) {
          persia_feature.add_final_label(value);
        }

        vec.push_back(std::move(persia_feature));
      }
    }
  } else {
    AdPersiaFeature persia_feature = extract_persia_feature(*p_adlog, 0);
    vec.push_back(std::move(persia_feature));
  }

  return vec;
}

std::vector<AdPersiaFeature> ExtractFeature::extract_feature_label_bundle(const std::string& line,
                                                                          AdlogDataType adlog_data_type) {
  std::vector<AdPersiaFeature> vec;
  ks::ad_algorithm::AdLog* p_adlog;
  tensorflow::Tensor t;
  std::vector<std::string> debug_info_strings;
  std::vector<std::string> extra_fields;

  std::pair<bool, std::unique_ptr<ks::ad_algorithm::AdLog>> parse_res =
    adlog_parser.parse_adlog(line, PreprocessType::All, &t, &debug_info_strings);
  if (!parse_res.first) {
    // LOG_EVERY_N(INFO, 1) << "ExtractFeatureUPModelLtr" << "!parse_res.first-error-ljp";
    return vec;
  }
  p_adlog = parse_res.second.get();

  uint64_t unique_key = ks::ad_algorithm::GenerateUniqueKey();
  p_adlog ->set_unique_key(unique_key);
  if (!LabelCentreMapping::Instance().PreMapping(*p_adlog)) {
      LOG_EVERY_N(WARNING, 1000) << "Tag center btq premapping failed";
  }

  Type2ExtractResultsPtr sparse_results_bundle;
  Type2ExtractResultsPtr dense_results_bundle;

  const Type2RetIndices& sparse_indices = ad_fea_ext_bundle_ptr->sparse_ret_indices();
  const Type2RetIndices& dense_indices = ad_fea_ext_bundle_ptr->dense_ret_indices();
  CacheInfo cache_info;

  ad_fea_ext_bundle_ptr->ExtractAllSparseFeatures(*p_adlog,
                                                  cache_info,
                                                  &sparse_results_bundle,
                                                  FeatureOrgLayout::BY_FIELD);
  ad_fea_ext_bundle_ptr->ExtractAllDenseFeatures(*p_adlog,
                                                  cache_info,
                                                  &dense_results_bundle,
                                                  FeatureOrgLayout::BY_FIELD);

  // 先跑 dragon 提取特征，后续在 Extract 方法中进行填充
  static thread_local std::vector<std::unordered_map<uint32_t, std::vector<int64>>> dragon_signs;
  static thread_local std::unordered_map<uint32_t, std::vector<int64>> user_dragon_signs;
  static thread_local std::unordered_map<std::string, std::vector<double>> dense_user_dragon_signs;
  static thread_local std::vector<std::unordered_map<std::string,
                                  std::vector<double>>> dense_item_dragon_signs;
  dragon_signs.clear();
  dragon_signs.resize(p_adlog->item_size());
  dense_user_dragon_signs.clear();
  dense_item_dragon_signs.clear();
  dense_item_dragon_signs.resize(p_adlog->item_size());
  user_dragon_signs.clear();
  bool dragon_status = false;
#ifdef BUILD_WITH_DRAGON
  if (!flags.dragon_config_file.empty()) {
    dragon_status = ExtractFeatureWithDragon(*p_adlog, &dragon_signs, &user_dragon_signs,
           &dense_user_dragon_signs, &dense_item_dragon_signs);
  }
#endif
  if (is_need_label) {
    for (size_t i = 0; i < p_adlog->item_size(); ++i) {
      AdPersiaFeature persia_feature = extract_persia_feature_bundle(*p_adlog,
                                                                     &sparse_results_bundle,
                                                                     &dense_results_bundle,
                                                                     sparse_indices,
                                                                     dense_indices,
                                                                     i,
                                                                     p_adlog->item_size());
      for (const auto &kv : user_dragon_features_) {
        int feat_index = user_dragon_fields_[kv.first];
        kuaishou::ad::algorithm::SparseFeature* sparse_feature = persia_feature.mutable_sparse_feature(
            feat_index);
        sparse_feature->set_index(feat_index);
        auto value_it = user_dragon_signs.find(kv.first);
        if (value_it != user_dragon_signs.end()) {
          for (int64 v : value_it->second) {
            sparse_feature->add_values(v);
          }
        }
      }
      for (const auto &kv : dense_user_dragon_fields_) {
        std::string class_name = kv.first;
        int feat_index = kv.second;
        auto size_it = dense_user_dragon_sizes_.find(feat_index);
        int slot_size = 0;
        if (size_it != dense_user_dragon_sizes_.end()) {
          slot_size = size_it->second;
        } else {
           LOG(ERROR) << "dragon user dense feature needs slot size!";
           continue;
        }
        kuaishou::ad::algorithm::DenseFeature* dense_feature = persia_feature.mutable_dense_feature(
             feat_index);
        dense_feature->set_index(feat_index);
        dense_feature->mutable_values()->Resize(slot_size, 0.0);
        auto value_it = dense_user_dragon_signs.find(class_name);
        if (value_it != dense_user_dragon_signs.end()) {
          for (int j = 0; j < value_it->second.size() && j < slot_size; j++) {
            float v = (float)(value_it->second)[j];
            dense_feature->set_values(j, v);
          }
        }
      }
      for (const auto &kv : item_dragon_features_) {
        int feat_index = item_dragon_fields_[kv.first];
        // kuaishou::ad::algorithm::SparseFeature* sparse_feature = persia_feature.add_sparse_feature();
        // sparse_feature->set_index(feat_index);
        kuaishou::ad::algorithm::SparseFeature* sparse_feature = persia_feature.mutable_sparse_feature(
            feat_index);
        sparse_feature->set_index(feat_index);
        auto value_it = dragon_signs.at(i).find(kv.first);
        if (value_it != dragon_signs.at(i).end()) {
          for (int64 v : value_it->second) {
            sparse_feature->add_values(v);
          }
        }
      }
     for (const auto &kv : dense_item_dragon_fields_) {
       std::string class_name = kv.first;
       int feat_index = kv.second;
       auto size_it = dense_item_dragon_sizes_.find(feat_index);
       int slot_size = 0;
       if (size_it != dense_item_dragon_sizes_.end()) {
         slot_size = size_it->second;
       } else {
         LOG(ERROR) << "dragon item dense feature needs slot size!";
         continue;
       }
       kuaishou::ad::algorithm::DenseFeature* dense_feature = persia_feature.mutable_dense_feature(
           feat_index);
       dense_feature->set_index(feat_index);
       dense_feature->mutable_values()->Resize(slot_size, 0.0);
       auto value_it = dense_item_dragon_signs.at(i).find(class_name);
       if (value_it != dense_item_dragon_signs.at(i).end()) {
         for (int j = 0; j < value_it->second.size() && j < slot_size; j++) {
           float v = (float)(value_it->second)[j];
           dense_feature->set_values(j, v);
         }
       }
     }

      const auto & label_vec = get_label_value(t, i);
      if (flags.ns < 1.0 && !label_vec[0]) {
        int rand_val = dist(gen);
        if (rand_val > (int)(flags.ns * 100000)) {
          continue;
        }
      }
      for (auto value : label_vec) {
        persia_feature.add_final_label(value);
      }
      if (flags.debug_info != "") {
        persia_feature.set_debug_info(debug_info_strings[i]);
      }
      vec.push_back(std::move(persia_feature));
    }
  } else {
    AdPersiaFeature persia_feature = extract_persia_feature_bundle(*p_adlog,
                                                                   &sparse_results_bundle,
                                                                   &dense_results_bundle,
                                                                   sparse_indices,
                                                                   dense_indices,
                                                                   0,
                                                                   p_adlog->item_size());
    vec.push_back(std::move(persia_feature));
  }

  return vec;
}

bool ExtractFeature::ExtractFeatureWithDragon(const ks::ad_algorithm::AdLog& adlog,
      std::vector<std::unordered_map<uint32_t, std::vector<int64>>> *dragon_signs,
      std::unordered_map<uint32_t, std::vector<int64>> *user_dragon_signs,
      std::unordered_map<std::string, std::vector<double>> *dense_user_dragon_signs,
      std::vector<std::unordered_map<std::string, std::vector<double>>> *dense_item_dragon_signs) {
#ifdef BUILD_WITH_DRAGON
  thread_local std::shared_ptr<ks::platform::CommonRecoLeafPipelineExecutor> executor = nullptr;
  if (executor == nullptr) {
    auto dragon_pipeline = dragon_config->Get("pipeline_manager_config");
    executor = std::make_shared<ks::platform::CommonRecoLeafPipelineExecutor>(dragon_pipeline);
  }
  executor->Reset();
  auto user_id = adlog.Get().user_info().id();
  std::string device_id = "";
  if (adlog.has_user_info() && adlog.user_info().has_ad_user_info() &&
      adlog.user_info().ad_user_info().device_info_size() > 0) {
    device_id = adlog.Get().user_info().ad_user_info().device_info(0).device_id();
  }

  // 关闭 processor CL_LOG 日志
  ks::platform::LoggingUtil::ResetLoggingEnabled(user_id, device_id);

  executor->SetUserId(user_id);
  executor->SetDeviceId(device_id);
  executor->SetPtrCommonAttr(ks::ad_nn::constant::kDragonInputPbItem,
                             dynamic_cast<const ks::ad_algorithm::AdLog*>(&adlog));
  executor->SetRequestTime(adlog.Get().time());  // uint64_t 强制转换成 int64
  for (auto &pipeline_name : run_pipelines_) {
    executor->Run(pipeline_name);
  }
  for (int i = 0; i < user_dragon_slot_keys_.size(); ++i) {
    auto user_slots = executor->GetIntList(user_dragon_slot_keys_[i]);
    auto user_signs = executor->GetIntList(user_dragon_sign_keys_[i]);
    if (user_slots.has_value() && user_signs.has_value()
     && user_slots->size() != 0 && user_signs->size() != 0
      && user_slots->size() == user_signs->size()) {
        for (int i = 0; i < user_slots->size(); i++) {
          (*user_dragon_signs)[user_slots->at(i)].emplace_back(user_signs->at(i));
        }
      }
  }
  // dense user
  for (const auto &kv : dense_user_dragon_fields_) {
    auto& attr_name = kv.first;
    auto result_values = executor->GetDoubleList(attr_name);
    if (result_values.has_value() && result_values->size() != 0) {
      for (int i = 0; i < result_values->size(); ++i) {
        (*dense_user_dragon_signs)[attr_name].emplace_back(result_values->at(i));
      }
    }
  }
  const std::vector<ks::platform::CommonRecoLeafPipelineExecutor::Item> &dragon_items =
    executor->GetItemList();
  if (0 == dragon_items.size()) {
    LOG(ERROR) << "dragon pipeline executed failed, result items is empty";
    return false;
  }
  if (dragon_signs->size() != dragon_items.size()) {
    LOG(ERROR) << "item size different, dragon item size=" << dragon_items.size()
               << " input size=" << dragon_signs->size();
    return false;
  }

  for (int item_index = 0; item_index < dragon_items.size(); ++item_index) {
    auto &dragon_item = dragon_items.at(item_index);
    std::unordered_map<uint32_t, std::vector<int64>> &slot_signs = dragon_signs->at(item_index);
    for (int i = 0; i < item_dragon_slot_keys_.size(); ++i) {
      auto dragon_result_slots = dragon_item.GetIntList(item_dragon_slot_keys_[i]);
      auto dragon_result_signs = dragon_item.GetIntList(item_dragon_sign_keys_[i]);
      if (dragon_result_slots.has_value() && dragon_result_signs.has_value() &&
          dragon_result_slots->size() != 0 && dragon_result_signs->size() != 0 &&
          dragon_result_slots->size() == dragon_result_signs->size()) {
        // spin the result
        for (int i = 0; i < dragon_result_slots->size(); ++i) {
          slot_signs[dragon_result_slots->at(i)].emplace_back(dragon_result_signs->at(i));
        }
      }
    }
    // item dense
    std::unordered_map<std::string, std::vector<double>> &item_dense_signs =
             dense_item_dragon_signs->at(item_index);
    for (const auto &kv : dense_item_dragon_fields_) {
      auto& attr_name = kv.first;
      auto result_values = dragon_item.GetDoubleList(attr_name);
      if (result_values.has_value() && result_values->size() != 0) {
        for (int i = 0; i < result_values->size(); ++i) {
          item_dense_signs[attr_name].emplace_back(result_values->at(i));
        }
      }
    }
  }

#endif
  return true;
}

std::vector<std::vector<std::string>> ExtractFeature::extract_feature_frame(const std::string& line,
                                                                            AdlogDataType adlog_data_type) {
  std::vector<std::vector<std::string>> vec;
  if (line == ";") {
    LOG_EVERY_N(WARNING, 100) << "line is empty";
    return vec;
  }
  std::vector<AdPersiaFeature> features = extract_feature_label(line, adlog_data_type);
  for (auto& persia_feature : features) {
    std::string feature_str;
    persia_feature.SerializeToString(&feature_str);
    std::string str_base64;
    if (base::Base64Encode(feature_str, &str_base64)) {
      if (flags.random_key) {
        std::string key = std::to_string(dist(gen));
        vec.push_back(std::move(make_vec(key, str_base64)));
      } else {
        vec.push_back(std::move(make_vec(str_base64)));
      }
    }
  }

  return vec;
}

VecVecStr ExtractFeature::extract_feature_sequence(const std::string& line,
                                                   AdlogDataType adlog_data_type) {
  VecVecStr vec;
  ks::ad_algorithm::AdLog* p_adlog;
  tensorflow::Tensor t;
  std::vector<std::string> extra_fields;

  std::pair<bool, std::unique_ptr<ks::ad_algorithm::AdLog>> parse_res =
      adlog_parser.parse_adlog(line, PreprocessType::All, &t);
  if (!parse_res.first) {
    return vec;
  }

  p_adlog = parse_res.second.get();
  if (p_adlog->item_size() == 0) {
    LOG(ERROR) << "empty items";
    return vec;
  }

  if (p_adlog->item_size() <= 1) {
    LOG(INFO) << "item_size is 1";
    return vec;
  }

  sparse_results.clear();
  dense_results.clear();
  feature_extractor->ExtractAllAdLogFeatures(*p_adlog, &sparse_results);
  feature_extractor->ExtractAllAdLogDenseFeatures(*p_adlog, &dense_results);
  std::vector<AdPersiaFeature> persia_features;
  persia_features.clear();
  for (size_t i = 0; i< p_adlog->item_size(); ++i) {
    AdPersiaFeature persia_feature = extract_persia_feature(*p_adlog, i);
    persia_features.push_back(persia_feature);
  }
/*
  for (size_t i = p_adLog->item_size()-1; i >0; --i) {
    //AdPersiaFeature persia_feature = extract_persia_feature(*p_adLog, i);

    if (flags.item_context) {
      std::vector<std::string> item_context_fields;
      base::SplitString(flags.item_context_field, ",", &item_context_fields);
      for (auto field_index_str : item_context_fields) {
        int field_index = std::stoi(field_index_str);
        for(size_t j = 0; j< i; ++j) {
          for(auto v : persia_features[j].sparse_feature(field_index).values()) {
            persia_features[i].mutable_sparse_feature(field_index)->add_values(v);
          }
        }
      }
    }
    for (auto value: get_label_value(t, i)) {
      persia_features[i].add_final_label(value);
    }
    std::string feature_str;
    persia_features[i].SerializeToString(&feature_str);
    std::string str_base64;
    if (base::Base64Encode(feature_str, &str_base64)) {
      if (flags.random_key) {
        std::string key = std::to_string(dist(gen));
        vec.push_back(std::move(make_vec(key, str_base64)));
      } else {
        vec.push_back(std::move(make_vec(str_base64)));
      }
    }
  }*/
  int item_size = p_adlog->item_size();
  for (size_t i = item_size -1; i > 0; --i) {
    if (flags.item_context) {
      std::vector<std::string> item_context_fields;
      base::SplitString(flags.item_context_field, ",", &item_context_fields);
      for (auto field_index_str : item_context_fields) {
        int field_index = std::stoi(field_index_str);
       for (size_t k = 0; k < i; ++k) {
          for (auto v : persia_features[k].sparse_feature(field_index).values()) {
            persia_features[i].mutable_sparse_feature(field_index)->add_values(v);
          }
       }
      }
    }

    if (is_need_label) {
      for (auto value : get_label_value(t, i)) {
        persia_features[i].add_final_label(value);
      }
    }
    std::string feature_str;
    persia_features[i].SerializeToString(&feature_str);
    std::string str_base64;
    if (base::Base64Encode(feature_str, &str_base64)) {
      if (str_base64.size() < 50) {
        std::cout << "line: " << line.substr(0, 100)
          << ", str_base64 size: " << str_base64.size() <<std::endl;
      }
      if (flags.random_key) {
        std::string key = std::to_string(dist(gen));
        vec.push_back(std::move(make_vec(key, str_base64)));
      } else {
        vec.push_back(std::move(make_vec(str_base64)));
      }
    }
  }

  return vec;
}

AdPersiaFeature ExtractFeature::extract_persia_feature(const ks::ad_algorithm::AdLog& adlog,
                                                       size_t pos) {
  AdPersiaFeature persia_feature;

  persia_feature.set_version(1);
  persia_feature.set_llsid(adlog.Get().llsid());
  persia_feature.set_time(adlog.Get().time());

  if (adlog.Get().has_user_info()) {
    persia_feature.set_user_id(adlog.Get().user_info().id());
  }

  if (pos >= 0 && adlog.item_size() > 0 && pos < adlog.item_size()) {
    auto& item = adlog.item(pos);
    persia_feature.set_item_id(item.id());
    persia_feature.mutable_label_info()->CopyFrom(item.label_info());
    persia_feature.set_photo_id(item.ad_dsp_info().photo_info().id());
    persia_feature.set_author_id(get_author_id_from_item(item));
    persia_feature.set_unit_id(item.ad_dsp_info().creative().base().unit_id());
    persia_feature.set_account_id(item.ad_dsp_info().creative().base().account_id());
    persia_feature.set_cover_id(item.ad_dsp_info().creative().base().cover_id());

    auto & ad_dsp_info = item.ad_dsp_info();
    if ( ad_dsp_info.call_back_type().action_type_size() > 0 ) {
      for ( size_t j = 0; j < ad_dsp_info.call_back_type().action_type_size(); j++ ) {
        persia_feature.add_call_back_type(ad_dsp_info.call_back_type().action_type(j));
      }
    }

    auto& label_info = item.label_info();
    persia_feature.add_labels(label_info.call_back_label());
  }

  for (size_t j = 0 ; j < feature_file_info->slots_count; j++) {
    kuaishou::ad::algorithm::SparseFeature* sparse_feature = persia_feature.add_sparse_feature();
    sparse_feature->set_index(j);
  }

  for (size_t j = 0 ; j < feature_file_info->dense_slots_count; j++) {
    kuaishou::ad::algorithm::DenseFeature* dense_feature = persia_feature.add_dense_feature();
    dense_feature->set_index(j);
    for (size_t k = 0 ; k < feature_file_info->dense_slots_size[j]; k++) {
        dense_feature->add_values(0.0);
    }
  }

  if (sparse_results.size() > pos && sparse_results[pos].size() > 0) {
    for (auto& ret : sparse_results[pos][0]) {
      persia_feature.mutable_sparse_feature(ret.field_index)->add_values(ret.field_key_sign);
    }
  }

  if (dense_results.size() > pos && dense_results[pos].size() > 0) {
    for (auto& ret : dense_results[pos][0]) {
      if (ret.field_index < feature_file_info->dense_slots_count &&
          ret.field_key_sign < dense_field_value[ret.field_index].size()) {
        persia_feature.mutable_dense_feature(ret.field_index)->set_values(ret.field_key_sign, ret.value);
      }
    }
  }

  return persia_feature;
}

AdPersiaFeature ExtractFeature::extract_persia_feature_bundle(const ks::ad_algorithm::AdLog& adlog,
                                                              Type2ExtractResultsPtr* sparse_results_bundle,
                                                              Type2ExtractResultsPtr* dense_results_bundle,
                                                              const Type2RetIndices& sparse_indices,
                                                              const Type2RetIndices& dense_indices,
                                                              size_t pos,
                                                              int item_size) {
  AdPersiaFeature persia_feature;

  persia_feature.set_version(1);
  persia_feature.set_llsid(adlog.Get().llsid());
  persia_feature.set_time(adlog.Get().time());

  if (adlog.Get().has_user_info()) {
    // LOG_EVERY_N(INFO, 1) << "adlog.Get().user_info().id(): " << adlog.Get().user_info().id();
    persia_feature.set_user_id(adlog.Get().user_info().id());
  }

  if (pos >= 0 && adlog.item_size() > 0 && pos < adlog.item_size()) {
    auto& item = adlog.item(pos);
    persia_feature.set_item_id(item.id());
    persia_feature.mutable_label_info()->CopyFrom(item.label_info());
    persia_feature.set_photo_id(item.ad_dsp_info().photo_info().id());
    persia_feature.set_author_id(get_author_id_from_item(item));
    persia_feature.set_account_id(item.ad_dsp_info().creative().base().account_id());
    persia_feature.set_cover_id(item.ad_dsp_info().creative().base().cover_id());

    auto & ad_dsp_info = item.ad_dsp_info();
    if ( ad_dsp_info.call_back_type().action_type_size() > 0 ) {
      for ( size_t j = 0; j < ad_dsp_info.call_back_type().action_type_size(); j++ ) {
        persia_feature.add_call_back_type(ad_dsp_info.call_back_type().action_type(j));
      }
    }

    auto& label_info = item.label_info();
    persia_feature.add_labels(label_info.call_back_label());
  }
  for (size_t j = 0 ; j < total_feature_count; j++) {
    kuaishou::ad::algorithm::SparseFeature* sparse_feature = persia_feature.add_sparse_feature();
    sparse_feature->set_index(j);
  }
  for (size_t j = 0 ; j < feature_file_info->dense_slots_count; j++) {
    kuaishou::ad::algorithm::DenseFeature* dense_feature = persia_feature.add_dense_feature();
    dense_feature->set_index(j);
    for (size_t k = 0 ; k < feature_file_info->dense_slots_size[j]; k++) {
        dense_feature->add_values(0.0);
    }
  }

  for (auto& sparse_feature_type : sparse_feature_type_sorted) {
    if (sparse_results_bundle->find(sparse_feature_type) == sparse_results_bundle->end()) {
      continue;
    }
    auto& sparse_vec = sparse_results_bundle->find(sparse_feature_type)->second;
    auto& sparse_vec_index = sparse_indices.find(sparse_feature_type)->second;
    if (IsUserType(sparse_feature_type)) {
      for (size_t j = 0; j < sparse_vec->size(); j++) {
        if (sparse_vec->at(j).size() > 0) {
          for (auto& ret : sparse_vec->at(j)) {
            int feature_index = sparse_vec_index[j];
            persia_feature.mutable_sparse_feature(feature_index)->add_values(ret.sign);
          }
        }
      }
    } else {
      for (size_t j = 0; j < sparse_vec->size(); j++) {
        if (j % item_size == pos) {
          if (sparse_vec->at(j).size() > 0) {
            for (auto& ret : sparse_vec->at(j)) {
              int feature_index = sparse_vec_index[j / item_size];
              persia_feature.mutable_sparse_feature(feature_index)->add_values(ret.sign);
            }
          }
        }
      }
    }
  }

  for (auto& dense_feature_type : dense_feature_type_sorted) {
    if (dense_results_bundle->find(dense_feature_type) == dense_results_bundle->end()) {
      continue;
    }
    auto& dense_vec = dense_results_bundle->find(dense_feature_type)->second;
    auto& dense_vec_index = dense_indices.find(dense_feature_type)->second;
    if (IsUserType(dense_feature_type)) {
      for (size_t j = 0; j < dense_vec->size(); j++) {
        if (dense_vec->at(j).size() > 0) {
          for (auto& ret : dense_vec->at(j)) {
            int feature_index = dense_vec_index[j];
            if (feature_index < feature_file_info->dense_slots_count &&
                ret.dense_sign < dense_field_value[feature_index].size()) {
              persia_feature.mutable_dense_feature(feature_index)->set_values(ret.dense_sign, ret.value);
            }
          }
        }
      }
    } else {
      for (size_t j = 0; j < dense_vec->size(); j++) {
        if (j % item_size == pos) {
          if (dense_vec->at(j).size() > 0) {
            for (auto& ret : dense_vec->at(j)) {
              int feature_index = dense_vec_index[j / item_size];
              if (feature_index < feature_file_info->dense_slots_count &&
                  ret.dense_sign < dense_field_value[feature_index].size()) {
                persia_feature.mutable_dense_feature(feature_index)->set_values(ret.dense_sign, ret.value);
              }
            }
          }
        }
      }
    }
  }

  return persia_feature;
}

// ============= ExtractFeatureNew ===========
std::vector<std::vector<std::string>> ExtractFeatureNew::process_line(std::string* line) {
  std::vector<std::vector<std::string>> vec;
  if ((*line) == ";") {
    LOG_EVERY_N(WARNING, 100) << "line is empty";
    return vec;
  }
  std::vector<AdPersiaFeature> features = extract_feature_label_bundle(*line, adlog_data_type);
  for (auto& persia_feature : features) {
    std::string feature_str;
    persia_feature.SerializeToString(&feature_str);
    std::string str_base64;
    if (base::Base64Encode(feature_str, &str_base64)) {
      if (flags.random_key) {
        std::string key = std::to_string(dist(gen));
        vec.push_back(std::move(make_vec(key, str_base64)));
      } else {
        vec.push_back(std::move(make_vec(str_base64)));
      }
    }
  }

  return vec;
}

// ============= ExtractFeatureLtr ===========
std::vector<std::vector<std::string>> ExtractFeatureLtr::process_line(std::string* line) {
  std::vector<std::vector<std::string>> vec;
  if ((*line) == ";") {
    LOG_EVERY_N(WARNING, 100) << "line is empty";
    return vec;
  }
  std::vector<AdPersiaFeature> features = extract_feature_label_bundle(*line, adlog_data_type);
  std::vector<AdPersiaFeature> ltr_features;
  std::vector<int> pos_idx;
  std::vector<int> neg_idx;
  for (int i = 0; i < features.size(); i++) {
    if (features[i].final_label().Get(0) > 0) {
      pos_idx.push_back(i);
    } else {
      neg_idx.push_back(i);
    }
  }
  std::vector<int> item_idx;
  // pair wise
  for (int i = 0; i < pos_idx.size(); i++) {
    for (int j = 0; j < neg_idx.size(); j++) {
      item_idx.clear();
      int64 label = dist(gen) % 2;
      if (label == 0) {
        item_idx.push_back(pos_idx[i]);
        item_idx.push_back(neg_idx[j]);
      } else {
        item_idx.push_back(neg_idx[j]);
        item_idx.push_back(pos_idx[i]);
      }
      for (int k = 0; k < item_idx.size(); k++) {
        AdPersiaFeature persia_feature;
        if (k == 0) {
          for (int m = 0; m < feature_file_info.get()->user_slots.size(); m++) {
            persia_feature.add_sparse_feature()->CopyFrom(features[item_idx[k]].sparse_feature(m));
          }
          for (int m = 0; m < feature_file_info.get()->dense_user_slots.size(); m++) {
            persia_feature.add_dense_feature()->CopyFrom(features[item_idx[k]].sparse_feature(m));
          }
        }
        for (int m = feature_file_info.get()->user_slots.size();
             m < feature_file_info.get()->user_slots.size() +
             feature_file_info.get()->photo_slots.size(); m++) {
          persia_feature.add_sparse_feature()->CopyFrom(features[item_idx[k]].sparse_feature(m));
        }
        for (int m = feature_file_info.get()->dense_user_slots.size();
             m < feature_file_info.get()->dense_user_slots.size() +
             feature_file_info.get()->dense_photo_slots.size(); m++) {
          persia_feature.add_dense_feature()->CopyFrom(features[item_idx[k]].dense_feature(m));
        }
        persia_feature.add_final_label(label);
        ltr_features.push_back(persia_feature);
      }
    }
  }

  for (auto& persia_feature : ltr_features) {
    std::string feature_str;
    persia_feature.SerializeToString(&feature_str);
    std::string str_base64;
    if (base::Base64Encode(feature_str, &str_base64)) {
      if (flags.random_key) {
        std::string key = std::to_string(dist(gen));
        vec.push_back(std::move(make_vec(key, str_base64)));
      } else {
        vec.push_back(std::move(make_vec(str_base64)));
      }
    }
  }

  return vec;
}

// ============= ExtractSequenceFeature ===========
std::vector<std::vector<std::string>> ExtractSequenceFeature::process_line(std::string* line) {
  return extract_feature_sequence(*line, adlog_data_type);
}

// ============= ExtractFeatureJoinId ===========
std::vector<std::vector<std::string>> ExtractFeatureJoinId::process_line(std::string* line) {
  std::vector<std::vector<std::string>> vec;
  if ((*line) == ";") {
    LOG_EVERY_N(WARNING, 100) << "line is empty";
    return vec;
  }

  std::vector<AdPersiaFeature> features = extract_feature_label_bundle(*line, adlog_data_type);

  for (auto& x : features) {
    std::string feature_str;
    std::string str_base64;

    x.SerializeToString(&feature_str);
    if (base::Base64Encode(feature_str, &str_base64)) {
      std::vector<std::string> one_line = make_vec_feature_join_id(x, str_base64, extend_field);
      vec.push_back(std::move(one_line));
    }
  }

  return vec;
}

// ============= ExtractFeatureUPModelPoint =========== 【TODO】

// ============= ExtractFeatureUPModelLtr ===========
bool ExtractFeatureUPModelLtr::LoadPdctEmb() {
  // 载入 product embedding
  std::ifstream ifs;
  ifs.open(flags.pdct_emb_file, std::ios::in);
  std::string str3;
  int product_emb_cnt = 0;
  while (getline(ifs, str3)) {
    str3.erase(std::remove(str3.begin(), str3.end(), '\n'), str3.cend());
    std::vector<std::string> results(0);
    base::SplitString(str3, flags.pdct_emb_first_sep, &results);
    if (results.size() == 2) {
      std::string product_name = results[0];
      std::string values = results[1];
      std::vector<std::string> parts(0);
      base::SplitString(values, flags.pdct_emb_second_sep, &parts);
      if (parts.size() == flags.pdct_emb_size) {
        std::vector<float> emb_vec_float;
        for (int i = 0; i < flags.pdct_emb_size; ++i) {
          std::string emb = parts[i];
          float emb_float = stof(emb);
          emb_vec_float.push_back(emb_float);
        }
        product_emb_map[product_name] = emb_vec_float;
        ++product_emb_cnt;
      }
    }
  }
  LOG(INFO) << "product_emb_cnt:" << product_emb_cnt;
  if (product_emb_cnt > 0) {
    return true;
  } else {
    return false;
  }
}
ExtractFeatureUPModelLtr::ExtractFeatureUPModelLtr() {
  if (flags.is_extract_upinf || flags.is_u2uemb2redis || flags.is_pdctemb2redis || flags.is_get_histemb || flags.is_pdctcluster_emb2redis || flags.is_pdctseq2redis) {
    if (flags.is_pdct_split_train) {
      redis_embedding_p2e_ = std::make_unique<RedisManager>();
      redis_embedding_p2e_->Initialize(flags.p2e_cluster_name,
          RedisEncodeType::RAW_STRING, flags.p2e_prefix);
      // 读入所有 outloop top product
      std::ifstream ifs_pdct;
      ifs_pdct.open(flags.all_outloop_pdct_file, std::ios::in);
      if (!ifs_pdct.is_open())
      {
        return;
      }

      std::string str_pdct;
      all_outloop_product_set.clear();
      while (getline(ifs_pdct, str_pdct)) {
        str_pdct.erase(std::remove(str_pdct.begin(), str_pdct.end(), '\n'), str_pdct.cend());
        all_outloop_product_set.insert(str_pdct);
      }
      ifs_pdct.close();
    } else {
      // 初始化 redis 写入
      std::string cluster_name = flags.up_cluster_name;
      redis_client_ = ks::infra::RedisProxyClient::GetRedisClientByKccFromKconf(cluster_name, 20);
      if (redis_client_ == nullptr) {
        LOG(FATAL) << "Failed to create redis client, cluster name: " << cluster_name;
      }
      redis_async_client_ = ks::infra::RedisProxyClient::GetRedisPipelineClientByKccFromKconf(cluster_name, 20);
      if (redis_async_client_ == nullptr) {
        LOG(FATAL) << "Failed to create redis async client, cluster name: " << cluster_name;
      }

      if (flags.is_get_histemb) {
        redis_embedding_u2p_ = std::make_unique<RedisManager>();
        redis_embedding_p2e_ = std::make_unique<RedisManager>();
        redis_embedding_u2p_->Initialize(flags.u2p_cluster_name,
          RedisEncodeType::RAW_STRING, flags.u2p_prefix);
        redis_embedding_p2e_->Initialize(flags.p2e_cluster_name,
          RedisEncodeType::RAW_STRING, flags.p2e_prefix);
        return;
      }

      if (flags.is_u2uemb2redis || flags.is_pdctemb2redis || flags.is_pdctcluster_emb2redis || flags.is_pdctseq2redis) {
        return;
      }
    }
  }

  // 【TODO】读取 product 的特征表，组织成为易于解析的格式
  std::ifstream ifs;
  ifs.open(flags.pdct_feat_file, std::ios::in);
  if (!ifs.is_open())
  {
    return;
  }

  std::string str;
  while (getline(ifs, str)) {
    // LOG_EVERY_N(INFO, 1) << "getline product_feat: size:" << str.size() << ",str:" << str;
    str.erase(std::remove(str.begin(), str.end(), '\n'), str.cend());
    std::vector<std::string> results(0);
    // base::SplitString(str, ",", &results);
    base::SplitString(str, "\t", &results);

    // 【Notice】特征列数 column_num = 7
    int column_num = flags.pdct_feat_num;
    if (results.size() == column_num && column_num >= 7) {
      std::string product_name = results[0];
      uint64_t first_id_v3 = stoi(results[1]);
      uint64_t second_id_v3 = stoi(results[2]);
      std::vector<std::string> word_cut(0);
      base::SplitString(results[3], ",", &word_cut);
      std::vector<std::string> character_cut(0);
      base::SplitString(results[4], ",", &character_cut);
      std::vector<std::string> similar_pdct(0);
      base::SplitString(results[5], ",", &similar_pdct);
      float freq = stof(results[6]);
      std::vector<std::string> similar_pdct_mmu(0);
      if (column_num == 10) {
        base::SplitString(results[9], ",", &similar_pdct_mmu);
      }
      mp_product_name_freq[product_name] = freq;
      ProductFeat product_feat;
      product_feat.product_name = product_name;
      product_feat.first_id_v3 = first_id_v3;
      product_feat.second_id_v3 = second_id_v3;
      product_feat.word_cut = word_cut;
      product_feat.character_cut = character_cut;
      product_feat.similar_pdct = similar_pdct;
      product_feat.similar_pdct_mmu = similar_pdct_mmu;
      mp_product_feature[product_name] = product_feat;
    }
  }
  ifs.close();
  // 【Notice】行为序列数据生产函数
  LOG(INFO) << "ljp-debug, inf_seq_len: " << flags.inf_seq_len;
  constructor.init(mp_product_name_freq, flags.up_window_size, flags.up_min_length, flags.up_neg_k_freq, flags.up_table_size, flags.up_neg_k_negactoin, flags.inf_seq_len,flags.up_action_weight);

  // for valid
  if (flags.up_valid) {
    constructor_valid.init(mp_product_name_freq, flags.up_valid_seq_len, flags.up_valid_neg_num);
  }

  if (mp_product_name_freq.size()) {
    auto it = mp_product_name_freq.begin();
  }

  if (mp_product_feature.size()) {
    auto it = mp_product_feature.begin();
  }
  if (flags.is_extract_pdctemb) {
    if(LoadPdctEmb()) {
      LOG(INFO) << "load product embedding success.";
    } else {
      LOG(INFO) << "load product embedding fail.";
    };
  }
}

void ExtractFeatureUPModelLtr::AllPdctsFillAdlogProto(std::vector<std::string>& results, std::vector<std::string> &rst) {
  if (results.size() >= 6) {
    kuaishou::ad::algorithm::AdJointLabeledLog adlog;
    // 获取 user_info
    kuaishou::ad::algorithm::UserInfo user_info;
    uint64_t user_id = 0;
    user_info.set_id(user_id);

    // 塞入 user_info
    adlog.mutable_user_info()->CopyFrom(user_info);

    std::string product_name = results[0];
    int64_t first_industry_id_v3 = stoi(results[1]);
    int64_t second_industry_id_v3 = stoi(results[2]);
    std::string t_word_cut = results[3];
    std::string t_character_cut = results[4];
    std::string t_similar_word = results[5];
    // product freq 暂时未加入
    // float product_freq = stof(results[6]);
    
    std::vector<std::string> pdct_words = {};   // 从 product 特征中获取
    std::vector<std::string> pdct_characters = {};    // 从 product 特征中获取
    std::vector<std::string> pdct_similars = {};      // 从 product 特征中获取
    base::SplitString(t_word_cut, ",", &pdct_words);
    base::SplitString(t_character_cut, ",", &pdct_characters);
    base::SplitString(t_similar_word, ",", &pdct_similars);

    // 塞入：product_name、一级行业
    kuaishou::ad::algorithm::AdvertiserBase advertiser_base;
    advertiser_base.set_product_name(product_name);
    advertiser_base.set_industry_first_id(first_industry_id_v3);

    // 塞入：二级行业
    kuaishou::ad::algorithm::Creative creative;
    kuaishou::ad::algorithm::CreativeBase creative_base;
    creative_base.set_industry_id_v3(second_industry_id_v3);
    creative.mutable_base()->CopyFrom(creative_base);

    kuaishou::ad::algorithm::AdDspInfo ad_dsp_info;
    ad_dsp_info.mutable_creative()->CopyFrom(creative);
    ad_dsp_info.mutable_advertiser_base()->CopyFrom(advertiser_base);

    // 塞入其他 product 特征，放入:
    // repeated ItemCommonInfoAttr common_info_attr = 24;
    // word cut
    kuaishou::ad::ItemCommonInfoAttr word_cut;
    word_cut.set_name_value(kuaishou::ad::ItemCommonInfoAttr_Name_UPMODEL_PRODUCT_WORD_CUT);
    
    for (int k = 0; k < pdct_words.size(); ++k) {
      std::string word = pdct_words[k];
      word_cut.add_string_list_value(word);
    }

    // character cut
    kuaishou::ad::ItemCommonInfoAttr character_cut;
    character_cut.set_name_value(kuaishou::ad::ItemCommonInfoAttr_Name_UPMODEL_PRODUCT_CHARACTER_CUT);
    for (int k = 0; k < pdct_characters.size(); ++k) {
      std::string character = pdct_characters[k];
      character_cut.add_string_list_value(character);
    }

    // similar product
    kuaishou::ad::ItemCommonInfoAttr similar_pdct;
    similar_pdct.set_name_value(kuaishou::ad::ItemCommonInfoAttr_Name_UPMODEL_PRODUCT_SIMILILAR_PDCT);
    for (int k = 0; k < pdct_similars.size(); ++k) {
      std::string similar = pdct_similars[k];
      similar_pdct.add_string_list_value(similar);
    }

    // product_embedding
    kuaishou::ad::ItemCommonInfoAttr pdct_emb;
    pdct_emb.set_name_value(kuaishou::ad::ItemCommonInfoAttr_Name_UPMODEL_PRODUCT_EMB);
    if (flags.is_extract_pdctemb) {
      if (product_emb_map.find(product_name) != product_emb_map.end()) {
        std::vector<float> curr_pdct_emb = product_emb_map.find(product_name)->second;
        pdct_emb.mutable_float_list_value()->Reserve(curr_pdct_emb.size());
        pdct_emb.mutable_float_list_value()->Add(curr_pdct_emb.cbegin(), curr_pdct_emb.cend());
      }
    }

    // 塞入当前 product 的 label
    int64_t label_ped = 0;
    int64_t label_click = 0;
    int64_t label_conv = 0;
    int64_t label_imp = 0;
    // LOG_EVERY_N(INFO, 1) << "FillAdlogProto,label_ped: " << label_ped << ", label_click:" << label_click << ", label_conv:" << label_conv;
    kuaishou::ad::algorithm::LabelInfo label_info;
    label_info.set_played_end(label_ped);
    label_info.set_click(label_click);
    label_info.set_delivery(label_conv);
    label_info.set_item_impression(label_imp);  // 曝光的一定是 1

    kuaishou::ad::algorithm::Item item;
    item.set_type(ItemType::AD_DSP);
    item.mutable_label_info()->CopyFrom(label_info);
    item.mutable_ad_dsp_info()->CopyFrom(ad_dsp_info);
    item.add_common_info_attr()->CopyFrom(word_cut);
    item.add_common_info_attr()->CopyFrom(character_cut);
    item.add_common_info_attr()->CopyFrom(similar_pdct);

    adlog.add_item()->CopyFrom(item);

    // 塞入一条 AdJointLabeledLog
    std::string adlog_str;
    std::string adlog_str_base64;
    adlog.SerializeToString(&adlog_str);
    if (base::Base64Encode(adlog_str, &adlog_str_base64)) {
      rst.push_back(adlog_str_base64);
    }
  }
}



std::pair<std::string, int> ExtractFeatureUPModelLtr::CalculateSeqLen(const std::string& line) {
  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);
  
  // 解析 action_infos 
  std::string user_id = results[0];
  std::string user_long_history_json = results[1];
  SeqRawData raw_data(std::stoll(user_id), user_long_history_json);
  int seq_len = constructor.calculate_seq_len(raw_data);

  return std::make_pair(user_id, seq_len);
}

bool ExtractFeatureUPModelLtr::WriteEmb2Redis(const Embeddings& embeddings, std::string key) {
  // 序列化
  std::string feature_str;
  bool flag = embeddings.SerializeToString(&feature_str);

  // 转字节码（byte array）写入 redis
  // auto err_code = redis_client_->Set(key, feature_str.c_str(), 20);
  auto err_code = redis_client_->Set(key, feature_str, 20);
  if (err_code != ks::infra::RedisErrorCode::KS_INF_REDIS_NO_ERROR) {
    LOG_EVERY_N(ERROR, 100) << "Set hist act fail, key: " << key
                           << ", err code: " << ks::infra::err2str(err_code);
    return false;
  }

  return true;
}

std::vector<std::vector<std::string>> ExtractFeatureUPModelLtr::WriteU2UtoRedis(const std::string& line) {
  std::vector<std::vector<std::string>> rst;

  // user_id[\t]u2u_emb
  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);
  if (results.size() == 2) {
    std::string user_id = results[0];
    std::string u2u_emb_str = results[1];
    std::vector<std::string> u2u_emb_vec(0);
    base::SplitString(u2u_emb_str, ",", &u2u_emb_vec);
    Embeddings value_u2u;
    CommonSpec spec_u2u;
    if (u2u_emb_vec.size() == flags.u2u_emb_size) {
      for (int i = 0; i < u2u_emb_vec.size(); ++i) {
        float value = stof(u2u_emb_vec[i]);
        spec_u2u.add_embedding(value);
      }
      value_u2u.add_embeddings()->CopyFrom(spec_u2u);
      std::string key_u2u = "hetu_u2u_" + user_id;
      bool flag_u2u = WriteEmb2Redis(value_u2u, key_u2u);
      std::string value_u2u_str;
      value_u2u.SerializeToString(&value_u2u_str);
      std::string str_base64;
      if (base::Base64Encode(value_u2u_str, &str_base64)) {
        rst.push_back({user_id, str_base64});
      }
    }
  }
  return rst;
}

std::vector<std::vector<std::string>> ExtractFeatureUPModelLtr::PdctEmbNormalize(const std::string& line) {
  std::vector<std::vector<std::string>> rst;
  // product_name[\t]product_emb
  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);
  std::hash<std::string> hash_fn;
  if (results.size() == 2) {
    std::string pdct_name = results[0];
    std::string pdct_emb_str = results[1];
    std::vector<std::string> pdct_emb_vec(0);
    base::SplitString(pdct_emb_str, " ", &pdct_emb_vec);
    std::vector<double> pdct_emb_value(0);

    if (pdct_emb_vec.size() == flags.pdct_emb_size) {
      double mag = 0;
      for (int i = 0; i < pdct_emb_vec.size(); ++i) {
        double value = stof(pdct_emb_vec[i]);
        mag += value * value;
        pdct_emb_value.push_back(value);
      }

      std::string pdct_emb_norm_str = "";
      for (int i = 0; i < pdct_emb_value.size(); ++i) {
        pdct_emb_value[i] = pdct_emb_value[i] / sqrt(mag);
        if (pdct_emb_norm_str.size() > 0) {
          pdct_emb_norm_str += " ";
        }
        pdct_emb_norm_str += std::to_string(pdct_emb_value[i]);
      }
      rst.push_back({pdct_name, pdct_emb_norm_str});
    }
  }
  return rst;
}

std::vector<std::vector<std::string>> ExtractFeatureUPModelLtr::WritePdctEmbtoRedis(const std::string& line) {
  std::vector<std::vector<std::string>> rst;
  // product_name[\t]product_emb
  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);
  // std::hash<std::string> hash_fn;
  if (results.size() == 2) {
    std::string pdct_name = results[0];
    std::string pdct_emb_str = results[1];
    std::vector<std::string> pdct_emb_vec(0);
    base::SplitString(pdct_emb_str, " ", &pdct_emb_vec);
    Embeddings value_pdct;
    CommonSpec spec_pdct;
    if (pdct_emb_vec.size() == flags.pdct_emb_size) {
      for (int i = 0; i < pdct_emb_vec.size(); ++i) {
        float value = stof(pdct_emb_vec[i]);
        spec_pdct.add_embedding(value);
      }
      value_pdct.add_embeddings()->CopyFrom(spec_pdct);
      int64_t city_product_id = base::CityHash64(pdct_name.data(), pdct_name.size());
      std::string key_pdct = flags.pdct_emb_prefix + "_" + std::to_string(city_product_id);
      bool flag_pdct = WriteEmb2Redis(value_pdct, key_pdct);
      std::string value_pdct_str;
      value_pdct.SerializeToString(&value_pdct_str);
      std::string str_base64;
      if (base::Base64Encode(value_pdct_str, &str_base64)) {
        rst.push_back({key_pdct, str_base64});
      }
    }
  }
  return rst;
}

std::vector<std::vector<std::string>> ExtractFeatureUPModelLtr::WritePdctSeqtoRedis(const std::string& line) {
  std::vector<std::vector<std::string>> rst;
  // visitor_id[\t]pdct_seq
  // pdct_seq: using comma for separator
  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);
  if (results.size() == 2) {
    std::string visitor_id = results[0];
    std::string pdct_seq_str = results[1];
    std::string key_pdct = flags.histact_pdct_prefix + "_" + visitor_id;

    std::vector<std::string> pdct_vec(0);
    base::SplitString(pdct_seq_str, ",", &pdct_vec);

    Embeddings value_pdct;
    CommonSpec spec_pdct;

    for (int i = 0; i < pdct_vec.size(); ++i) {
      std::string pdct = pdct_vec[i];
      // 跳过空的产品名
      if (pdct.empty() || pdct.size()==0) {
        continue;
      }
      // 强制将 uint64 转换成 int64
      int64_t pdct_id = base::CityHash64(pdct.data(), pdct.size());
      spec_pdct.add_id_list(pdct_id);
    }

    value_pdct.add_embeddings()->CopyFrom(spec_pdct);

    // 写入 redis
    bool flag_pdct = WriteEmb2Redis(value_pdct, key_pdct);
    rst.push_back({visitor_id, pdct_seq_str});
  }
  return rst;
}

std::vector<std::vector<std::string>> ExtractFeatureUPModelLtr::WritePdctClusterEmbtoRedis(const std::string& line) {
  std::vector<std::vector<std::string>> rst;
  // LOG(INFO) << "ljp,debug, line:" << line;
  // photo_id[\t]product_emb
  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);
  // LOG(INFO) << "ljp,debug, results.size():" << results.size();

  if (results.size() == 2) {
    std::string photo_id = results[0];
    std::string pdct_emb_str = results[1];
    // LOG(INFO) << "ljp,debug, photo_id:" << photo_id << ", pdct_emb_str:" << pdct_emb_str;
    std::vector<std::string> pdct_emb_vec(0);
    base::SplitString(pdct_emb_str, " ", &pdct_emb_vec);
    Embeddings value_pdct;
    CommonSpec spec_pdct;
    // LOG(INFO) << "ljp,debug, pdct_emb_vec.size():" << pdct_emb_vec.size() << ", flags.pdct_emb_size:" << flags.pdct_emb_size;
    if (pdct_emb_vec.size() == flags.pdct_emb_size) {
      for (int i = 0; i < pdct_emb_vec.size(); ++i) {
        float value = stof(pdct_emb_vec[i]);
        spec_pdct.add_embedding(value);
      }
      value_pdct.add_embeddings()->CopyFrom(spec_pdct);
      
      std::string key_photo = flags.pdct_emb_prefix + "_" + photo_id;
      // LOG(INFO) << "ljp,debug, key_photo:" << key_photo;
      bool flag_pdct = WriteEmb2Redis(value_pdct, key_photo);
      std::string value_pdct_str;
      value_pdct.SerializeToString(&value_pdct_str);
      std::string str_base64;
      if (base::Base64Encode(value_pdct_str, &str_base64)) {
        rst.push_back({key_photo, str_base64});
      }
    }
  }
  return rst;
}

bool ExtractFeatureUPModelLtr::GetBatchEmbeddings(std::vector<std::string>& mgi_keys, std::string log_flag) {
  std::vector<std::string> mgi_embeddins;
  // 暂时使用同一个 redis，提高服务性能
  if (KS_UNLIKELY(
          !redis_embedding_p2e_->GetRawDataFromRedisByBatch(mgi_keys, &mgi_embeddins))) {
    LOG(ERROR) << "Failed to " << log_flag << ", keys size " << mgi_keys.size();
    return false;
  }
  if (mgi_keys.size() != mgi_embeddins.size()) {
    LOG(ERROR) << "Dismatch with keys size " << mgi_keys.size()
               << ", and embedding string size " << mgi_embeddins.size();
    return false;
  }
  // 第二步：解析 Embeddings，获取 ID list
  // LOG(INFO) << "ljp, mgi_embeddins.size():" << mgi_embeddins.size();
  for (int i = 0; i < mgi_embeddins.size(); ++i) {
    const std::string& embedding_str = mgi_embeddins[i];
    Embeddings value;
    if (embedding_str.size() > 0) {
      if (!value.ParseFromString(embedding_str)) {
        LOG(ERROR) << "Parse user embedding_str fail," << log_flag << ", string size: "
                   << embedding_str.size() << ", key: " << mgi_keys[i];
        continue;
      }
      interest_photo_embeddings_.emplace_back(value);
    } else {
      LOG_EVERY_N(ERROR, 5000) << "Failed to get value from redis, key " << mgi_keys[i];
      continue;
    }
  }
  return true;
}

bool ExtractFeatureUPModelLtr::PrepareEmbKeys(std::vector<uint64_t> ids, std::vector<std::string>& mgi_keys, int id_type, bool enable_mgi_int64_trans) {
  std::string prefix = "";
  // id_type, 0: product, 1: photo
  prefix = flags.p2e_prefix.empty() ? "" : flags.p2e_prefix;
  // LOG(INFO) << "ljp, prefix:" << prefix;
  for (int i = 0; i < ids.size(); ++i) {
    std::string id_str;
    // 只有 product_id 才需要转换
    if (id_type == 0 && enable_mgi_int64_trans) {
      id_str = std::to_string(static_cast<int64_t>(ids[i]));
    } else {
      id_str = std::to_string(ids[i]);
    }
    const std::string key = prefix.empty() ? id_str : (prefix + "_" + id_str);
    // LOG(INFO) << "ljp, i:" << i << ", key: " << key;
    mgi_keys.emplace_back(key);
  }
  return true;
}

bool ExtractFeatureUPModelLtr::ParseIdEmbVector(std::vector<std::vector<float>> &ad_log_mgi_embeddings_, int id_type, uint64_t user_id) {
  std::string log_flag = (id_type == 0 ? "product" : "photo");
  int mgi_count = interest_photo_embeddings_.size();
  ad_log_mgi_embeddings_.clear();
  ad_log_mgi_embeddings_.reserve(mgi_count);
  for (int i = 0; i < mgi_count; ++i) {
    auto& interest_photo_embedding = interest_photo_embeddings_[i];
    if (interest_photo_embedding.embeddings_size() < 1) {
      LOG_EVERY_N(ERROR, 100) << "Failed to parse embedding from redis " << log_flag 
                              << ", user id: " << user_id;
      continue;
    }
    auto& photo_embeddings_spec = interest_photo_embedding.embeddings(0);
    int embedding_length = photo_embeddings_spec.embedding_size();
    ad_log_mgi_embeddings_.emplace_back();
    auto& fill_embedding = ad_log_mgi_embeddings_.back();
    fill_embedding.assign(photo_embeddings_spec.embedding().data(),
                          photo_embeddings_spec.embedding().data() + embedding_length);
  }
  return true;
}

float ExtractFeatureUPModelLtr::dot_product(const std::vector<float>& v1, const std::vector<float>& v2)
{
    // Check that the vectors have the same size
    if (v1.size() != v2.size())
    {
        throw std::invalid_argument("Vector dimensions do not match");
    }

    // Calculate the dot product using a loop
    float result = 0.0;
    for (int i = 0; i < v1.size(); i++)
    {
        result += v1[i] * v2[i];
    }

    return result;
}

// 返回所有 sys_pdct 的相似度得分
std::vector<float> ExtractFeatureUPModelLtr::dot_product_all(const std::vector<std::vector<float>>& v1, const std::vector<std::vector<float>>& v2, int hist_pdct_num)
{
  std::vector<float> result;
  // Get the dimensions of the matrices
  if (v1.empty() || v2.empty()) return result;
  int m1 = v1.size();
  if (m1 == 0) return result;
  int n1 = v1[0].size();

  int m2 = v2.size();
  m2 = m2 > hist_pdct_num ? hist_pdct_num : m2;
  if (m2 == 0) return result;
  int n2 = v2[0].size();

  // Check that the matrices have compatible dimensions
  if (n1 != n2)
  {
    return result;
  }
  // LOG(INFO) << "ljp, m1:" << m1 <<", m2:" << m2;

  // Create a vector to hold the dot products

  // Calculate the dot product of each row of v1 with all rows of v2
  result.reserve(m1);
  for (int i = 0; i < m1; i++)
  {   
    float curr_simi = 0;
    for (int j = 0; j < m2; j++)
    {
      // LOG(INFO) << "bf,i: " << i < ", j:" << j;
      float t = dot_product(v1[i], v2[j]);
      curr_simi = curr_simi > t ? curr_simi : t;
      // LOG(INFO) << "simi, i:" << i << ",j:" << j << ",value:" << curr_simi;
    }
    result.push_back(curr_simi);
  }
  return result;
}

std::string ExtractFeatureUPModelLtr::vector_to_string(const std::vector<float>& vec) {
  std::ostringstream oss;
  std::copy(vec.begin(), vec.end(), std::ostream_iterator<float>(oss, ","));
  std::string result = oss.str();
  // Remove the last comma character
  result.pop_back();
  return result;
}


std::vector<std::vector<std::string>> ExtractFeatureUPModelLtr::GetHistoryEmbedding(const std::string& line) {
  // 1、get photo id and embedding
  std::vector<std::vector<std::string>> rst;

  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);
  std::string user_id = results[0];

  // 如果传入两列，user_id [\t] 系统行为的 pdct 序列，那么计算系统行为与历史行为的相似度
  bool flag_calc_sim = false;
  if (results.size() == 2) {
    flag_calc_sim = true;
  }
  // LOG(INFO) << "ljp, flag_calc_sim:" << flag_calc_sim;
  std::string key_u2p = flags.u2p_prefix + "_" + user_id;
  std::string embedding_str;
  if (KS_UNLIKELY(!redis_embedding_u2p_->GetRawDataFromRedis(key_u2p, &embedding_str))) {
    LOG_EVERY_N(ERROR, 100) << "Failed to get photo id from redis, user id " << user_id
                            << ", prefix " << flags.u2p_prefix;
    return rst;
  }

  if (embedding_str.empty() || !interest_photo_id_.ParseFromString(embedding_str)) {
    LOG(ERROR) << "Parse user embedding string fail, string size: " << embedding_str.size()
              << ", key_u2p: " << key_u2p;
    return rst;
  }

  if (interest_photo_id_.embeddings_size() < 1) {
    LOG_EVERY_N(ERROR, 100) << "Failed to parse photo id from redis, user id " << user_id;
    return rst;
  }

  // 2 get embedding from redis
  auto& photo_id_spec = interest_photo_id_.embeddings(0);
  int photo_count = photo_id_spec.id_list_size();
  photo_ids_.assign(photo_id_spec.id_list().data(),
                    photo_id_spec.id_list().data() + photo_count);
  interest_photo_embeddings_.clear();

  std::vector<std::string> embedding_strings;
  std::vector<std::string> keys;
  for (int i = 0; i < photo_ids_.size(); ++i) {
    uint64_t id = photo_ids_[i];
    const std::string key =
        flags.p2e_prefix.empty() ? std::to_string(id) : (flags.p2e_prefix + "_" + std::to_string(id));
    keys.emplace_back(key);
  }

  if (KS_UNLIKELY(!redis_embedding_p2e_->GetRawDataFromRedisByBatch(keys, &embedding_strings))) {
    LOG(ERROR) << "Failed to get raw data from redis by batch, keys size " << keys.size();
    return rst;
  }
  if (keys.size() != embedding_strings.size()) {
    LOG(ERROR) << "Dismatch with keys size " << keys.size()
               << ", and embedding string size " << embedding_strings.size();
    return rst;
  }
  for (int i = 0; i < embedding_strings.size(); ++i) {
    const std::string& embedding_str = embedding_strings[i];
    Embeddings value;
    if (embedding_str.size() > 0) {
      if (!value.ParseFromString(embedding_str)) {
        LOG(ERROR) << "Parse user embedding_str fail, string size: "
                   << embedding_str.size() << ", key: " << keys[i];
        continue;
      }
      interest_photo_embeddings_.emplace_back(value);
    } else {
      LOG_EVERY_N(ERROR, 5000) << "Failed to get value from redis, key " << keys[i];
      continue;
    }
  }

  int found_photo_embedding_count = interest_photo_embeddings_.size();
  ad_log_hist_mgi_embeddings_.clear();
  ad_log_hist_mgi_embeddings_.reserve(found_photo_embedding_count);
  for (int i = 0; i < found_photo_embedding_count; ++i) {
    auto& interest_photo_embedding = interest_photo_embeddings_[i];
    if (interest_photo_embedding.embeddings_size() < 1) {
      LOG_EVERY_N(ERROR, 100) << "Failed to parse photo embedding from redis, user id "
                              << user_id;
      continue;
    }
    auto& photo_embeddings_spec = interest_photo_embedding.embeddings(0);
    int embedding_length = photo_embeddings_spec.embedding_size();
    ad_log_hist_mgi_embeddings_.emplace_back();
    auto& fill_embedding = ad_log_hist_mgi_embeddings_.back();
    fill_embedding.assign(photo_embeddings_spec.embedding().data(),
                          photo_embeddings_spec.embedding().data() + embedding_length);
  }

  if (!flag_calc_sim) {
    // 将 photo_emb 写入redis, list of float
    std::ostringstream oss;
    for (size_t i = 0; i < ad_log_hist_mgi_embeddings_.size(); ++i) {
      if (i > 0) {
          oss << ",";
      }
      for (size_t j = 0; j < ad_log_hist_mgi_embeddings_[i].size(); ++j) {
          if (j > 0) {
              oss << ",";
          }
          oss << ad_log_hist_mgi_embeddings_[i][j];
      }
    }
    std::string photo_emb_str = oss.str();
    std::string u2p2e_key = flags.u2p2e_prefix + "_" + user_id;
    auto err_code = redis_client_->Set(u2p2e_key, photo_emb_str, 20);
    if (err_code != ks::infra::RedisErrorCode::KS_INF_REDIS_NO_ERROR) {
      LOG_EVERY_N(ERROR, 100) << "Set hist u2p2e fail, key: " << u2p2e_key
                            << ", err code: " << ks::infra::err2str(err_code);
      return rst;
    }
    rst.push_back({user_id, photo_emb_str});
    return rst;
  } else {
    // 获取系统行为的 pdct 对应的 embedding
    std::vector<std::string> sys_pdct_names(0);
    // LOG(INFO) << "ljp, results[1]:" << results[1];
    base::SplitString(results[1], ",", &sys_pdct_names);
    
    // LOG(INFO) << "ljp, sys_pdct_names.size():" << sys_pdct_names.size();
    // 进行 base::CityHash64 
    std::vector<uint64_t> sys_pdct_ids;
    for (int i = 0; i < sys_pdct_names.size(); ++i) {
      std::string pdct = sys_pdct_names[i];
      // LOG(INFO) << "ljp, i: " << i << ", pdct: " << pdct; 
      uint64_t pdct_id = base::CityHash64(pdct.data(), pdct.size());
      sys_pdct_ids.push_back(pdct_id);
    }
    std::vector<std::string> mgi_keys;
    bool flag_prepare_pdctkeys = PrepareEmbKeys(sys_pdct_ids, mgi_keys, 0, true);
    interest_photo_embeddings_.clear();
    bool flag_getembs = GetBatchEmbeddings(mgi_keys, "get_sys_pdct_embs");
    
    // 解析 embeddings 为向量
    int mgi_count = interest_photo_embeddings_.size();
    // LOG(INFO) << "ljp, mgi_count:" << mgi_count;
    ad_log_sys_pdct_embeddings_.clear();
    bool flag_parse_pdctemb = ParseIdEmbVector(ad_log_sys_pdct_embeddings_, 0, std::stoull(user_id));
    // LOG(INFO) << "ad_log_sys_pdct_embeddings_.size():" << ad_log_sys_pdct_embeddings_.size();
    // LOG(INFO) << "ad_log_hist_mgi_embeddings_.size():" << ad_log_hist_mgi_embeddings_.size();
    // 计算两个向量的余弦相似度
    std::string simi_str = "";
    if (ad_log_sys_pdct_embeddings_.size() > 0 && ad_log_hist_mgi_embeddings_.size()>0) {
      std::vector<float> simi = dot_product_all(ad_log_sys_pdct_embeddings_, ad_log_hist_mgi_embeddings_, 10);
      simi_str = vector_to_string(simi);
    }
    rst.push_back({user_id, simi_str});
    return rst;
  }
}

std::vector<std::vector<std::string>> ExtractFeatureUPModelLtr::ExtractUPForInfDaily(const std::string& line) {
  std::vector<std::vector<std::string>> rst;
  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);
  
  // 解析 action_infos 
  std::string user_id = results[0];
  std::string user_the_same_day_json = results[1];
  SeqRawData raw_data(std::stoll(user_id), user_the_same_day_json);
  std::vector<std::vector<ActionItem>> hist_grain_by_act = constructor.produce_user_histpdct(raw_data, flags.is_rm_click_ped, flags.is_dedup_pdct, flags.is_extract_photo);
  if (hist_grain_by_act.size() != 2) {
    return rst;
  }

  return rst;

}

std::vector<std::vector<std::string>> ExtractFeatureUPModelLtr::ExtractUPForInf(const std::string& line) {
  std::vector<std::vector<std::string>> rst;

  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);
  
  // 解析 action_infos 
  std::string user_id = results[0];
  std::string user_long_history_json = results[1];
  std::string user_prev_split_pdct_seq = "";
  std::string user_prev_pdct_seq = "";
  // LOG(INFO) << "ljp-debug, user_id:" << user_id;
  // LOG(INFO) << "ljp-debug, user_long_history_json:" << user_long_history_json;
  SeqRawData raw_data(std::stoll(user_id), user_long_history_json);
  if (flags.is_stats_user_long_actions) {
    return constructor.analyse_user_action(raw_data, user_id);
  }

  std::vector<std::vector<ActionItem>> hist_pdct_by_act = constructor.produce_user_histpdct(raw_data, flags.is_rm_click_ped, flags.is_dedup_pdct, flags.is_extract_photo);
  std::string hist_pdct_str = "";
  if (flags.is_pdct_split_train) {
    user_prev_split_pdct_seq = results[2];
    if (user_prev_split_pdct_seq.size() > 0) {
      hist_pdct_str = user_prev_split_pdct_seq;
    }
  }
  if (flags.is_pdct_train) {
    user_prev_pdct_seq = results[2];
    if (user_prev_pdct_seq.size() > 0) {
      hist_pdct_str = user_prev_pdct_seq;
    }
  }

  if (hist_pdct_by_act.size() != 2) {
    if (flags.is_pdct_split_train) {
      rst.push_back({user_id, hist_pdct_str});
    }
    if (flags.is_pdct_train) {
      rst.push_back({user_id, hist_pdct_str});
    }
    return rst;
  }

  std::vector<ActionItem> hist_pdct = hist_pdct_by_act[0];
  // LOG(INFO) << "ljp-debug, hist_pdct.size():" << hist_pdct.size();
  std::vector<ActionItem> hist_pdct_shallow = hist_pdct_by_act[1];
  std::string key_pdct = flags.histact_pdct_prefix + "_" + user_id;
  Embeddings value_photo;
  CommonSpec spec_photo;
  bool flag_photo = false;
  std::string hist_photo_str = "";
  std::string hist_timestamp_str = "";
  std::string hist_label_str = "";
  ActionItem item;
  std::string photo;
  std::string product_name;
  std::vector<string> conv_photo_keys;
  std::string delivery_timestamp;
  std::string label;
  std::vector<std::string> product_clusters;
  switch(flags.mgi_task_id) {
    case 0: // 解析在线召回 trigger photo，替代 flags.is_extract_photo 的功能
      for (int i = 0; i < hist_pdct.size(); ++i) {
        item = hist_pdct[i];
        photo = item.photo_id;
        // 跳过空的 photo 或者 0 值 photo
        if (photo.empty() || photo.size()==0 || photo == "0") {
          continue;
        }

        if (hist_pdct_str.size() > 0) {
          hist_pdct_str += ",";
        }
        hist_pdct_str += photo;

        int64_t photo_id = std::stoll(photo);
        spec_photo.add_id_list(photo_id);
      }
      value_photo.add_embeddings()->CopyFrom(spec_photo);
      flag_photo = WriteEmb2Redis(value_photo, key_pdct);
      rst.push_back({user_id, hist_pdct_str});
      return rst;
    case 1:
      for (int i = 0; i < hist_pdct.size(); ++i) {
        item = hist_pdct[i];
        photo = item.photo_id;
        delivery_timestamp = std::to_string(item.delivery_timestamp);
        label = std::to_string(item.label);

        // 跳过空的 photo 或者 0 值 photo
        if (photo.empty() || photo.size()==0 || photo == "0") {
          continue;
        }

        if (hist_photo_str.size() > 0) {
          hist_photo_str += ",";
        }
        hist_photo_str += photo;

        if (hist_timestamp_str.size() > 0) {
          hist_timestamp_str += ",";
        }
        hist_timestamp_str += delivery_timestamp;

        if (hist_label_str.size() > 0) {
          hist_label_str += ",";
        }
        hist_label_str += label;
      }
      rst.push_back({user_id, hist_photo_str, hist_timestamp_str, hist_label_str});
      return rst;
    case 3:
      conv_photo_keys.clear();
      product_clusters.clear();
      for (int i = 0; i < hist_pdct.size(); ++i) {
        item = hist_pdct[i];
        photo = item.photo_id;
        // LOG(INFO) << "ljp-debug, photo_id:" << photo << ", i:" << i << ",item.label:" << item.label;
        // 跳过空的 photo 或者 0 值 photo
        if (photo.empty() || photo.size()==0 || photo == "0") {
          continue;
        }
        if (flags.is_pdct_split_train) {
          if ((item.label & 32) || (item.label & 16) || (item.label & 8) 
              || (item.label & 4) || (item.label & 2) || item.label & 1) {
            conv_photo_keys.push_back(flags.pdct_cluster_key + photo);
          }
        } else {
          conv_photo_keys.push_back(flags.pdct_cluster_key + photo);
        }
      }
      if (KS_UNLIKELY(
              !redis_embedding_p2e_->GetRawDataFromRedisByBatch(conv_photo_keys, &product_clusters))) {
        LOG(ERROR) << "Failed to get photo product cluster" << ", keys size " << conv_photo_keys.size();
      }
      if (conv_photo_keys.size() != product_clusters.size()) {
        LOG(ERROR) << "Dismatch with keys size " << conv_photo_keys.size()
                  << ", and product clusters size " << product_clusters.size();
      } else {
        for (int i = 0; i < product_clusters.size(); ++i) {
          const std::string& pdct_cluster = product_clusters[i];
          if (hist_pdct_str.size() > 0) {
            hist_pdct_str += ",";
          }
          hist_pdct_str += pdct_cluster;
        }
      }
      rst.push_back({user_id, hist_pdct_str});
      return rst;
    case 4:
      for (int i = 0; i < hist_pdct.size(); ++i) {
        item = hist_pdct[i];
        product_name = item.product_name;
        // 跳过空的 product_name
        if (product_name.empty() || product_name.size()==0) {
          continue;
        }
        if (hist_pdct_str.size() > 0) {
          hist_pdct_str += ",";
        }
        hist_pdct_str += product_name;
      }
      rst.push_back({user_id, hist_pdct_str});
      return rst;
    default:
      break;
  }

  // 准备实时行为的启动数据
  if (flags.is_prepare_for_online) {
    Embeddings value_pdct_online;
    CommonSpec spec_pdct_A, spec_pdct_B, spec_pdct_C, spec_pdct_D;
    // 写入 online, 转化
    product_count.clear();
    for (int i = 0; i < hist_pdct.size(); ++i) {
      ActionItem item = hist_pdct[i];
      std::string pdct = item.product_name;
      // 跳过空的产品名
      if (pdct.empty() || pdct.size()==0) {
        continue;
      }

      if (hist_pdct_str.size() > 0) {
        hist_pdct_str += ",";
      }
      hist_pdct_str += pdct;

      int64_t pdct_id = base::CityHash64(pdct.data(), pdct.size());
      spec_pdct_A.add_id_list(pdct_id);
      spec_pdct_B.add_id_list(pdct_id);
    }

    // 写入 item_click 以及 playend
    product_count.clear();
    bool write_deep_flag = hist_pdct_str.size() > 0 ? 1: 0;
    for (int i = 0; i < hist_pdct_shallow.size(); ++i) {
      ActionItem item = hist_pdct_shallow[i];
      std::string pdct = item.product_name;
      // 跳过空的产品名
      if (pdct.empty() || pdct.size()==0) {
        continue;
      }

      if (hist_pdct_str.size() > 0) {
        hist_pdct_str += ",shallow_";
      }
      hist_pdct_str += pdct;

      int64_t pdct_id = base::CityHash64(pdct.data(), pdct.size());
      if (write_deep_flag) {
        spec_pdct_A.add_id_list(pdct_id);
      }
      spec_pdct_C.add_id_list(pdct_id);
    }

    value_pdct_online.add_embeddings()->CopyFrom(spec_pdct_A);
    value_pdct_online.add_embeddings()->CopyFrom(spec_pdct_B);
    value_pdct_online.add_embeddings()->CopyFrom(spec_pdct_C);
    value_pdct_online.add_embeddings()->CopyFrom(spec_pdct_D);
    bool flag_pdct_online = WriteEmb2Redis(value_pdct_online, key_pdct);
    rst.push_back({user_id, hist_pdct_str});
    return rst;
  }

  hist_pdct_str = "";
  std::string hist_pdct_hash_str = "";
  std::string hist_firstid_str = "";
  std::string hist_secondid_str = "";

  Embeddings value_pdct, value_firstid, value_secondid;
  CommonSpec spec_pdct, spec_firstid, spec_secondid, spec_pdct_label;

  // 检查是否可以从 Redis 中读到数据，如果可以，则不必写入
  bool write_flag = true;
  std::string raw_string;
  auto err_code = redis_client_->Get(key_pdct, &raw_string);
  if (flags.is_check_write && (err_code != ks::infra::RedisErrorCode::KS_INF_REDIS_NO_ERROR ||
      raw_string.empty())) {
    write_flag = false;
  }
  if (write_flag) {
    product_count.clear();
    for (int i = 0; i < hist_pdct.size(); ++i) {
      ActionItem item = hist_pdct[i];
      std::string pdct = item.product_name;
      // 跳过空的产品名
      if (pdct.empty() || pdct.size()==0) {
        continue;
      }

      int label = item.label;
      if (hist_pdct_str.size() > 0) {
        hist_pdct_str += ",";
      }
      hist_pdct_str += pdct;

      if (hist_pdct_hash_str.size() > 0) {
        hist_pdct_hash_str += ",";
      }
      // 强制将 uint64 转换成 int64
      int64_t pdct_id = base::CityHash64(pdct.data(), pdct.size());
      hist_pdct_hash_str += std::to_string(pdct_id);
      spec_pdct.add_id_list(pdct_id);
      spec_pdct_label.add_id_list(label);
    }

    value_pdct.add_embeddings()->CopyFrom(spec_pdct);
    value_pdct.add_embeddings()->CopyFrom(spec_pdct_label);
    
    // 写入 redis
    // bool flag_firstid = WriteEmb2Redis(value_firstid, key_firstid);
    bool flag_pdct = WriteEmb2Redis(value_pdct, key_pdct);
    // bool flag_secondid = WriteEmb2Redis(value_secondid, key_secondid);
    rst.push_back({user_id, hist_pdct_str, hist_firstid_str, hist_secondid_str, hist_pdct_hash_str});
  }
  return rst;
}

std::vector<std::string> ExtractFeatureUPModelLtr::CalculateDeliPdctNum(const std::string& line) {
  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);
  
  // 解析 action_infos 
  std::string user_id = results[0];
  std::string user_long_history_json = results[1];
  SeqRawData raw_data(std::stoll(user_id), user_long_history_json);
  std::string deli_pdctnum = constructor.calc_delitime_pdctnum(raw_data);
  std::vector<std::string> rst; rst.push_back(deli_pdctnum);
  return rst;
}



// 将 user_info, user_action 等填入 adlog 中
std::vector<std::string> ExtractFeatureUPModelLtr::FillAdlogProto(const std::string& line) {
  std::vector<std::string> rst;

  // 1. all_product 模式，将 line 解析成为：product_name, first_industry_id_v3, second_industry_id_v3, word_cut, character_cut, similar_word, pdct_freq
  // 2. 其他，将 line 解析成为: user_id, user_info, action_infos
  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);

  // 提取所有 product 的特征
  if (flags.up_extract_mode == "all_product") {
    AllPdctsFillAdlogProto(results, rst);
    return rst;
  }

  // 解析 action_infos 
  std::string user_id = results[0];
  std::string user_long_history_json = results[1];

  std::vector<SeqPointwiseSample> all_pointwise_sample;
  SeqRawData raw_data(std::stoll(user_id), user_long_history_json);
  std::vector<SeqPointwiseSample> seq_sample_list;
  if (flags.up_valid) {
    seq_sample_list = constructor_valid.transform(raw_data);
  } else {
    seq_sample_list = constructor.transform(raw_data);
  }

  // 可能为空，需要考虑这部分用户如何处理
  // 限制每个用户的样本数量，来避免数据倾斜问题
  int curr_adlog_num = seq_sample_list.size();
  curr_adlog_num = std::min(curr_adlog_num, flags.up_max_adlog_num);
  for (int i = 0; i < curr_adlog_num; ++i) {
    SeqPointwiseSample sample = seq_sample_list[i];
    kuaishou::ad::algorithm::AdJointLabeledLog adlog;
    // 获取 user_info
    kuaishou::ad::algorithm::UserInfo user_info;

    // 1. 塞入 user_history_list
    // repeated CommonInfoAttr common_info_attr = 42;
    const std::vector<ActionItem>& user_history_list = sample.user_history_list;
    kuaishou::ad::CommonInfoAttr hist_pdct;
    kuaishou::ad::CommonInfoAttr hist_first_id;
    kuaishou::ad::CommonInfoAttr hist_second_id_v3;
    kuaishou::ad::CommonInfoAttr hist_word_cut;   // 【TODO】待填数据
    kuaishou::ad::CommonInfoAttr hist_character_cut;  // 【TODO】待填数据
    kuaishou::ad::CommonInfoAttr hist_similar_pdct;   // 【TODO】待填数据

    kuaishou::ad::CommonInfoAttr hist_ped_label;
    kuaishou::ad::CommonInfoAttr hist_click_label;
    kuaishou::ad::CommonInfoAttr hist_conv_label;
    kuaishou::ad::CommonInfoAttr hist_form_label;
    kuaishou::ad::CommonInfoAttr hist_invk_label;
    kuaishou::ad::CommonInfoAttr hist_pay_label;
    kuaishou::ad::CommonInfoAttr hist_next_label;
    kuaishou::ad::CommonInfoAttr hist_inapp_label;

    hist_pdct.set_name_value(kuaishou::ad::CommonInfoAttr_NameExtendOne_UPMODEL_USER_HIST_PDCT);
    hist_first_id.set_name_value(kuaishou::ad::CommonInfoAttr_NameExtendOne_UPMODEL_USER_HIST_FIRST_ID_V3);
    hist_second_id_v3.set_name_value(kuaishou::ad::CommonInfoAttr_NameExtendOne_UPMODEL_USER_HIST_SECOND_ID_V3);
    hist_ped_label.set_name_value(kuaishou::ad::CommonInfoAttr_NameExtendOne_UPMODEL_USER_HIST_PED_LABEL);
    hist_click_label.set_name_value(kuaishou::ad::CommonInfoAttr_NameExtendOne_UPMODEL_USER_HIST_CLICK_LABEL);
    hist_conv_label.set_name_value(kuaishou::ad::CommonInfoAttr_NameExtendOne_UPMODEL_USER_HIST_CONV_LABEL);
    hist_form_label.set_name_value(kuaishou::ad::CommonInfoAttr_NameExtendOne_UPMODEL_USER_HIST_FORM_LABEL);
    hist_invk_label.set_name_value(kuaishou::ad::CommonInfoAttr_NameExtendOne_UPMODEL_USER_HIST_INVK_LABEL);
    hist_pay_label.set_name_value(kuaishou::ad::CommonInfoAttr_NameExtendOne_UPMODEL_USER_HIST_PAY_LABEL);
    hist_next_label.set_name_value(kuaishou::ad::CommonInfoAttr_NameExtendOne_UPMODEL_USER_HIST_NEXT_LABEL);
    hist_inapp_label.set_name_value(kuaishou::ad::CommonInfoAttr_NameExtendOne_UPMODEL_USER_HIST_INAPP_LABEL);

    for (int j = 0; j < user_history_list.size(); ++j) {
      std::string pdct = user_history_list[j].product_name;
      // 获取当前 product_name 的其他特征
      ProductFeat pdct_feat;
      uint64 industry_first_id = 0;  // 从 product 特征中获取，如果获取不到填入默认值 0
      uint64 industry_id_v3 = 0;     // 从 product 特征中获取，如果获取不到填入默认值 0
      std::vector<std::string> pdct_words = {};   // 从 product 特征中获取
      std::vector<std::string> pdct_characters = {};    // 从 product 特征中获取
      std::vector<std::string> pdct_similars = {};      // 从 product 特征中获取
      // 【TODO】填入更多丰富的 product 特征
      if (mp_product_feature.find(pdct) != mp_product_feature.end()) {
        pdct_feat = mp_product_feature[pdct];
        industry_first_id = pdct_feat.first_id_v3;
        industry_id_v3 = pdct_feat.second_id_v3;
        pdct_words = pdct_feat.word_cut;
        pdct_characters = pdct_feat.character_cut;
        pdct_similars = pdct_feat.similar_pdct;
      }
      hist_pdct.add_string_list_value(pdct);
      hist_first_id.add_int_list_value(industry_first_id);
      hist_second_id_v3.add_int_list_value(industry_id_v3);
      hist_ped_label.add_int_list_value(user_history_list[j].label_ped);
      hist_click_label.add_int_list_value(user_history_list[j].label_click);
      hist_conv_label.add_int_list_value(user_history_list[j].label_conv);
      hist_form_label.add_int_list_value(user_history_list[j].label_form);
      hist_invk_label.add_int_list_value(user_history_list[j].label_invk);
      hist_pay_label.add_int_list_value(user_history_list[j].label_pay);
      hist_next_label.add_int_list_value(user_history_list[j].label_next);
      hist_inapp_label.add_int_list_value(user_history_list[j].label_inapp);
    }
    user_info.add_common_info_attr()->CopyFrom(hist_pdct);
    user_info.add_common_info_attr()->CopyFrom(hist_first_id);
    user_info.add_common_info_attr()->CopyFrom(hist_second_id_v3);
    user_info.add_common_info_attr()->CopyFrom(hist_ped_label);
    user_info.add_common_info_attr()->CopyFrom(hist_click_label);
    user_info.add_common_info_attr()->CopyFrom(hist_conv_label);
    user_info.add_common_info_attr()->CopyFrom(hist_form_label);
    user_info.add_common_info_attr()->CopyFrom(hist_invk_label);
    user_info.add_common_info_attr()->CopyFrom(hist_pay_label);
    user_info.add_common_info_attr()->CopyFrom(hist_next_label);
    user_info.add_common_info_attr()->CopyFrom(hist_inapp_label);
    user_info.set_id(std::stoll(user_id));

    // 2. 塞入 user_info
    adlog.mutable_user_info()->CopyFrom(user_info);

    // 3. 将 user_target_list 塞入 repeated Item 中
    int curr_item_num = sample.user_target_list.size();

    const std::vector<ActionItem>& multi_item = sample.user_target_list;
    for (int j = 0; j < curr_item_num; ++j) {
      const ActionItem& curr_item = multi_item[j];
      std::string pdct = curr_item.product_name;
      // 获取当前 product_name 的其他特征
      ProductFeat pdct_feat;
      uint64 industry_first_id = 0;  // 从 product 特征中获取，如果获取不到填入默认值 0
      uint64 industry_id_v3 = 0;     // 从 product 特征中获取，如果获取不到填入默认值 0
      std::vector<std::string> pdct_words = {};   // 从 product 特征中获取
      std::vector<std::string> pdct_characters = {};    // 从 product 特征中获取
      std::vector<std::string> pdct_similars = {};      // 从 product 特征中获取
      std::vector<std::string> pdct_similars_mmu = {};      // 从 product 特征中获取
      // 【TODO】填入更多丰富的 product 特征
      if (mp_product_feature.find(pdct) != mp_product_feature.end()) {
        pdct_feat = mp_product_feature[pdct];
        industry_first_id = pdct_feat.first_id_v3;
        industry_id_v3 = pdct_feat.second_id_v3;
        pdct_words = pdct_feat.word_cut;
        pdct_characters = pdct_feat.character_cut;
        pdct_similars = pdct_feat.similar_pdct;
        pdct_similars_mmu = pdct_feat.similar_pdct_mmu;
      }

      // 塞入：product_name、一级行业
      kuaishou::ad::algorithm::AdvertiserBase advertiser_base;
      advertiser_base.set_product_name(curr_item.product_name);
      advertiser_base.set_industry_first_id(industry_first_id);

      // 塞入：二级行业
      kuaishou::ad::algorithm::Creative creative;
      kuaishou::ad::algorithm::CreativeBase creative_base;
      creative_base.set_industry_id_v3(industry_id_v3);
      creative.mutable_base()->CopyFrom(creative_base);

      kuaishou::ad::algorithm::AdDspInfo ad_dsp_info;
      ad_dsp_info.mutable_creative()->CopyFrom(creative);
      ad_dsp_info.mutable_advertiser_base()->CopyFrom(advertiser_base);

      // 塞入其他 product 特征，放入:
      // repeated ItemCommonInfoAttr common_info_attr = 24;
      // word cut
      kuaishou::ad::ItemCommonInfoAttr word_cut;
      word_cut.set_name_value(kuaishou::ad::ItemCommonInfoAttr_Name_UPMODEL_PRODUCT_WORD_CUT);
      
      for (int k = 0; k < pdct_words.size(); ++k) {
        std::string word = pdct_words[k];
        word_cut.add_string_list_value(word);
      }

      // character cut
      kuaishou::ad::ItemCommonInfoAttr character_cut;
      character_cut.set_name_value(kuaishou::ad::ItemCommonInfoAttr_Name_UPMODEL_PRODUCT_CHARACTER_CUT);
      for (int k = 0; k < pdct_characters.size(); ++k) {
        std::string character = pdct_characters[k];
        character_cut.add_string_list_value(character);
      }

      // similar product
      kuaishou::ad::ItemCommonInfoAttr similar_pdct;
      similar_pdct.set_name_value(kuaishou::ad::ItemCommonInfoAttr_Name_UPMODEL_PRODUCT_SIMILILAR_PDCT);
      for (int k = 0; k < pdct_similars.size(); ++k) {
        std::string similar = pdct_similars[k];
        similar_pdct.add_string_list_value(similar);
      }

      // similar product mmu
      kuaishou::ad::ItemCommonInfoAttr similar_pdct_mmu;
      similar_pdct_mmu.set_name_value(kuaishou::ad::ItemCommonInfoAttr_Name_UPMODEL_PRODUCT_SIMILILAR_PDCT_MMU_OFFLINE);
      for (int k = 0; k < pdct_similars_mmu.size(); ++k) {
        std::string similar_mmu = pdct_similars_mmu[k];
        similar_pdct_mmu.add_string_list_value(similar_mmu);
      }

      // product_embedding
      kuaishou::ad::ItemCommonInfoAttr pdct_emb;
      pdct_emb.set_name_value(kuaishou::ad::ItemCommonInfoAttr_Name_UPMODEL_PRODUCT_EMB);
      if (flags.is_extract_pdctemb) {
        if (product_emb_map.find(curr_item.product_name) != product_emb_map.end()) {
          std::vector<float> curr_pdct_emb = product_emb_map.find(curr_item.product_name)->second;
          pdct_emb.mutable_float_list_value()->Reserve(curr_pdct_emb.size());
          pdct_emb.mutable_float_list_value()->Add(curr_pdct_emb.cbegin(), curr_pdct_emb.cend());
        }
      }

      // 塞入当前 product 的 label
      int64_t label_ped = curr_item.label_ped;
      int64_t label_click = curr_item.label_click;
      int64_t label_conv = curr_item.label_conv;
      // LOG_EVERY_N(INFO, 1) << "FillAdlogProto,label_ped: " << label_ped << ", label_click:" << label_click << ", label_conv:" << label_conv;
      kuaishou::ad::algorithm::LabelInfo label_info;
      label_info.set_played_end(label_ped);
      label_info.set_item_click(label_click);
      label_info.set_item_conversion(label_conv);
      label_info.set_landing_page_form_submitted(curr_item.label_form);
      label_info.set_delivery(curr_item.label_invk); // delivery: 是否唤端
      label_info.set_item_purchase(curr_item.label_pay);  // item_purchase: 是否付费
      label_info.set_item_impression(curr_item.label_next);  // item_impression: 是否次留
      label_info.set_comment(curr_item.label_inapp); // comment: 是否关键行为

      kuaishou::ad::algorithm::Item item;
      item.set_type(ItemType::AD_DSP);
      item.mutable_label_info()->CopyFrom(label_info);
      item.mutable_ad_dsp_info()->CopyFrom(ad_dsp_info);
      item.add_common_info_attr()->CopyFrom(word_cut);
      item.add_common_info_attr()->CopyFrom(character_cut);
      item.add_common_info_attr()->CopyFrom(similar_pdct);
      item.add_common_info_attr()->CopyFrom(similar_pdct);
      item.add_common_info_attr()->CopyFrom(similar_pdct_mmu);
      adlog.add_item()->CopyFrom(item);
    }

    // 塞入一条 AdJointLabeledLog
    std::string adlog_str;
    std::string adlog_str_base64;
    adlog.SerializeToString(&adlog_str);
    if (base::Base64Encode(adlog_str, &adlog_str_base64)) {
      rst.push_back(adlog_str_base64);
    }
  }

  return rst;
}

std::string ExtractFeatureUPModelLtr::apply_hash(const std::string &in_feat, bool is_multi) {
  std::string hash_rst = "";
  std::hash<std::string> hash_fn;
  if (is_multi) {
    std::vector<std::string> results(0);
    base::SplitString(in_feat, flags.pdct_feat_sep, &results);
    for (int i = 0; i < results.size(); ++i) {
      int64_t id = hash_fn(results[i]);
      std::ostringstream os;
      os << id;
      std::string id_str = os.str();
      if (hash_rst.size() > 0) {
        hash_rst += ",";
      }
      hash_rst += id_str;
    }
  } else {
    int64_t id = hash_fn(in_feat);
    std::ostringstream os;
    os << id;
    std::string id_str = os.str();
    hash_rst += id_str;
  }
  return hash_rst;
}

std::vector<std::vector<std::string>> ExtractFeatureUPModelLtr::apply_hash_on_pdct_feat(const std::string &line) {
  std::vector<std::vector<std::string>> vec;

  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);
  if (results.size() == flags.pdct_feat_num) {
    std::vector<std::string> curr_vec;
    std::string product_name = results[0];
    std::string first_industry_id_v3 = results[1];
    std::string second_industry_id_v3 = results[2];
    std::string t_word_cut = apply_hash(results[3], true);
    std::string t_character_cut = apply_hash(results[4], true);
    std::string t_similar_word = apply_hash(results[5], true);
    std::string freq = results[6];
    curr_vec.push_back(product_name);
    curr_vec.push_back(first_industry_id_v3);
    curr_vec.push_back(second_industry_id_v3);
    curr_vec.push_back(t_word_cut);
    curr_vec.push_back(t_character_cut);
    curr_vec.push_back(t_similar_word);
    curr_vec.push_back(freq);
    vec.push_back(curr_vec);
  }
  return vec;
}

std::vector<std::vector<std::string>> ExtractFeatureUPModelLtr::process_line(std::string* line) {
  std::vector<std::vector<std::string>> vec;
  try {
    if ((*line) == ";") {
      LOG_EVERY_N(WARNING, 100) << "line is empty";
      return vec;
    }

    if (flags.is_u2uemb2redis) {
      vec = WriteU2UtoRedis(*line);
    }

    if (flags.is_pdctemb2redis) {
      vec = WritePdctEmbtoRedis(*line);
    }

    if (flags.is_pdctcluster_emb2redis) {
      vec = WritePdctClusterEmbtoRedis(*line);
    }

    if (flags.is_pdctseq2redis) {
      vec = WritePdctSeqtoRedis(*line);
    }

    if (flags.is_pdctembnorm) {
      vec = PdctEmbNormalize(*line);
    }

    if (flags.is_extract_upinf) {
      vec = ExtractUPForInf(*line);
      return vec;
    }

    if (flags.is_get_histemb) {
      vec = GetHistoryEmbedding(*line);
      return vec;
    }
    
    if (flags.is_cal_seqlen) {
      std::vector<std::string> curr_len;
      std::pair<std::string, int> user_seq_len = CalculateSeqLen(*line);
      // LOG(INFO) << "user_id: " << user_seq_len.first << ", seq_len:" << std::to_string(user_seq_len.second);
      curr_len.push_back(user_seq_len.first);
      curr_len.push_back(std::to_string(user_seq_len.second));
      vec.push_back(curr_len);
      return vec;
    }

    if (flags.is_cal_delipdctnum) {
      std::vector<std::string> deli_pdctnum = CalculateDeliPdctNum(*line);
      vec.push_back(deli_pdctnum);
      return vec;
    }

    if (flags.up_extract_mode == "pdct_feat_hash") {
      return apply_hash_on_pdct_feat(*line);
    }

    std::vector<std::string> adlog_vec = FillAdlogProto(*line);
    int adlog_num = adlog_vec.size();
    for (int k = 0; k < adlog_num; ++k) {
      std::string curr_ad_log = adlog_vec[k];
      std::vector<AdPersiaFeature> features = extract_feature_label_bundle(curr_ad_log, adlog_data_type);
      if (flags.up_valid || flags.up_extract_mode == "all_product") {
        for (auto& persia_feature : features) {
          std::string feature_str;
          persia_feature.SerializeToString(&feature_str);
          std::string str_base64;
          if (base::Base64Encode(feature_str, &str_base64)) {
            if (flags.random_key) {
              std::string key = std::to_string(dist(gen));
              vec.push_back(std::move(make_vec(key, str_base64)));
            } else {
              LOG_EVERY_N(INFO, 1) << "str_base64:" << str_base64;
              if (flags.up_extract_mode == "all_product") {
                std::vector<std::string> results(0);
                base::SplitString(*line, flags.sep, &results);
                std::string product_name = results[0];
                vec.push_back(std::move(make_vec_feature_with_pdct(product_name, str_base64)));
              } else {
                vec.push_back(std::move(make_vec_feature_from_user_info(persia_feature, str_base64)));
              }
            }
          }
        }
      } else {
        // 开始构造 Ltr 数据
        std::vector<AdPersiaFeature> ltr_features;
        std::vector<int> pos_idx;
        std::vector<int> neg_idx;

        for (int i = 0; i < features.size(); i++) {
          if (features[i].final_label().Get(0) > 0) {
            pos_idx.push_back(i);
          } else {
            neg_idx.push_back(i);
          }
        }
        std::vector<int> item_idx;
        // pair wise
        for (int i = 0; i < pos_idx.size(); i++) {
          for (int j = 0; j < neg_idx.size(); j++) {
            item_idx.clear();
            int64 label = dist(gen) % 2;
            if (label == 0) {
              item_idx.push_back(pos_idx[i]);
              item_idx.push_back(neg_idx[j]);
            } else {
              item_idx.push_back(neg_idx[j]);
              item_idx.push_back(pos_idx[i]);
            }
            AdPersiaFeature persia_feature;
            // LOG_EVERY_N(INFO, 1) << "features[item_idx[0]].user_id():" << features[item_idx[0]].user_id();
            persia_feature.set_user_id(features[item_idx[0]].user_id());
            for (int k = 0; k < item_idx.size(); k++) {
              if (k == 0) {
                for (int m = 0; m < feature_file_info.get()->user_slots.size() + feature_file_info.get()->photo_slots.size(); m++) {
                  persia_feature.add_sparse_feature()->CopyFrom(features[item_idx[k]].sparse_feature(m));
                }
                for (int m = 0; m < feature_file_info.get()->dense_user_slots.size() + feature_file_info.get()->dense_photo_slots.size(); m++) {
                  persia_feature.add_dense_feature()->CopyFrom(features[item_idx[k]].dense_feature(m));
                }
              } else {
                // ljp,注意,此处需要修改 index
                int cnt = 0;
                int sparse_feature_num = feature_file_info.get()->user_slots.size() + feature_file_info.get()->photo_slots.size();
                for (int m = feature_file_info.get()->user_slots.size();
                    m < feature_file_info.get()->user_slots.size() +
                    feature_file_info.get()->photo_slots.size(); m++) {
                  kuaishou::ad::algorithm::SparseFeature* sparse_feature = persia_feature.add_sparse_feature();
                  sparse_feature->CopyFrom(features[item_idx[k]].sparse_feature(m));
                  sparse_feature->set_index(sparse_feature_num+cnt);
                  cnt += 1;
                }
                cnt = 0;
                int dense_feature_num = feature_file_info.get()->dense_user_slots.size() + feature_file_info.get()->dense_photo_slots.size();
                for (int m = feature_file_info.get()->dense_user_slots.size();
                    m < feature_file_info.get()->dense_user_slots.size() +
                    feature_file_info.get()->dense_photo_slots.size(); m++) {
                  kuaishou::ad::algorithm::DenseFeature* dense_feature = persia_feature.add_dense_feature();
                  dense_feature->CopyFrom(features[item_idx[k]].dense_feature(m));
                  dense_feature->set_index(dense_feature_num+cnt);
                  cnt += 1;
                }
              }
            }
            // persia_feature.add_final_label(label);
            // LOG(INFO) << "ljp, label: " << features[item_idx[0]].final_label().Get(0);
            persia_feature.add_final_label(features[item_idx[0]].final_label().Get(0));
            ltr_features.push_back(persia_feature);
          }
        }

        for (auto& persia_feature : ltr_features) {
          std::string feature_str;
          persia_feature.SerializeToString(&feature_str);
          std::string str_base64;
          if (base::Base64Encode(feature_str, &str_base64)) {
            if (flags.random_key) {
              std::string key = std::to_string(dist(gen));
              vec.push_back(std::move(make_vec(key, str_base64)));
            } else {
              LOG_EVERY_N(INFO, 10000) << "str_base64:" << str_base64;
              vec.push_back(std::move(make_vec_feature_from_user_info(persia_feature, str_base64)));
            }
          }
        }
      }
    }
  } catch (const char* msg) {
    LOG_EVERY_N(INFO, 1) << "ExtractFeatureUPModelLtr exception: " << msg;
  }

  return vec;
}


// ============= ExtractFeatureFromUserInfo ===========
std::vector<AdPersiaFeature> ExtractFeatureFromUserInfo::extract_feature_from_userinfo(const std::string& line) {
  // 填充相关特征
  std::vector<AdPersiaFeature> vec;
  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);
  // 输入数据仅仅包括 user_id
  if (results.size() == 2) {

    // 1. 将从 Picasso 获取 UserInfo 升级至 Hive 表读取
    std::string user_id = results[0];

    kuaishou::ad::algorithm::UserInfo user_info;
    std::string ori_user_info = results[1];
    std::string bline;

    if (!base::Base64Decode(ori_user_info, &bline) || !user_info.ParseFromString(bline)) {
      cerr << "parse user ad info failed" << std::endl;
      return vec;
    }

    // 将 user info 相关字段填入特征中
    AdPersiaFeature persia_feature;
    persia_feature.set_version(1);
    persia_feature.set_user_id(user_info.id());
    // 预先设置 sparse 特征数量
    for (size_t j = 0 ; j < 28; j++) {
      kuaishou::ad::algorithm::SparseFeature* sparse_feature = persia_feature.add_sparse_feature();
      sparse_feature->set_index(j);
    }
    std::hash<std::string> hash_fn;
    const auto& uid = GetFeature(FeaturePrefix::USER_ID, user_info.id());
    persia_feature.mutable_sparse_feature(0)->add_values(hash_func(uid) % (bucket_size - 1));
    persia_feature.mutable_sparse_feature(0)->add_values(bucket_size - 1);
    const auto& level = GetFeature(USER_LEVEL_NEW, user_info.level());
    persia_feature.mutable_sparse_feature(1)->add_values(hash_func(level) % (bucket_size - 1));
    persia_feature.mutable_sparse_feature(1)->add_values(bucket_size - 1);
    // persia_feature.mutable_sparse_feature(2)->add_values(hash_func(user_info.attribute().gender()) % (bucket_size - 1));
    // persia_feature.mutable_sparse_feature(3)->add_values(hash_func(user_info.attribute().follow_count()) % (bucket_size - 1));
    // persia_feature.mutable_sparse_feature(4)->add_values(hash_func(user_info.attribute().fans_count()) % (bucket_size - 1));
    // persia_feature.mutable_sparse_feature(5)->add_values(hash_func(user_info.attribute().upload_count()) % (bucket_size - 1));

    // feat 3: user_info.attribute.gender
    if (user_info.has_attribute()) {
      const auto& attribute_gender = GetFeature(FeaturePrefix::UP_USER_GENDER, user_info.attribute().gender());
      persia_feature.mutable_sparse_feature(2)->add_values(hash_func(attribute_gender) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(2)->add_values(bucket_size - 1);

    // feat 4: user_info.attribute.follow_count
    if (user_info.has_attribute()) {
      const auto& attribute_follow_count = GetFeature(FeaturePrefix::UP_USER_FOLLOW_COUNT, user_info.attribute().follow_count());
      persia_feature.mutable_sparse_feature(3)->add_values(hash_func(attribute_follow_count) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(3)->add_values(bucket_size - 1);

    // feat 5: user_info.attribute.fans_count
    if (user_info.has_attribute()) {
      const auto& attribute_fans_count = GetFeature(FeaturePrefix::UP_USER_FANS_COUNT, user_info.attribute().fans_count());
      persia_feature.mutable_sparse_feature(4)->add_values(hash_func(attribute_fans_count) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(4)->add_values(bucket_size - 1);

    // feat 6: user_info.attribute.upload_count
    if (user_info.has_attribute()) {
      const auto& attribute_upload_count = GetFeature(FeaturePrefix::UP_USER_UPLOAD_COUNT, user_info.attribute().upload_count());
      persia_feature.mutable_sparse_feature(5)->add_values(hash_func(attribute_upload_count) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(5)->add_values(bucket_size - 1);

    // feat 7: user_info.long_term_loc.ip
    if (user_info.has_long_term_loc()) {
      const auto& long_term_ip = GetFeature(FeaturePrefix::USER_IP_PROVINCE, hash_fn(user_info.long_term_loc().ip()));
      persia_feature.mutable_sparse_feature(6)->add_values(hash_func(long_term_ip) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(6)->add_values(bucket_size - 1);

    // feat 8: user_info.long_term_loc.province_id
    if (user_info.has_long_term_loc()) {
      const auto& long_term_province_id = GetFeature(FeaturePrefix::USER_LOC_PROVINCE, user_info.long_term_loc().province_id());
      persia_feature.mutable_sparse_feature(7)->add_values(hash_func(long_term_province_id) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(7)->add_values(bucket_size - 1);

    // feat 9: user_info.long_term_loc.city_id
    if (user_info.has_long_term_loc()) {
      const auto& long_term_city_id = GetFeature(FeaturePrefix::USER_LOC_CITY, user_info.long_term_loc().city_id());
      persia_feature.mutable_sparse_feature(8)->add_values(hash_func(long_term_city_id) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(8)->add_values(bucket_size - 1);

    // feat 10: user_info.long_term_loc.poi_type
    if (user_info.has_long_term_loc()) {
      const auto& long_term_poi_type = GetFeature(FeaturePrefix::UP_USER_POI_TYPE, user_info.long_term_loc().poi_type());
      persia_feature.mutable_sparse_feature(9)->add_values(hash_func(long_term_poi_type) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(9)->add_values(bucket_size - 1);

    // feat 11: user_info.current_loc.ip
    if (user_info.has_current_loc()) {
      const auto& current_loc_ip = GetFeature(FeaturePrefix::UP_USER_CURRENT_LOC_IP, hash_fn(user_info.current_loc().ip()));
      persia_feature.mutable_sparse_feature(10)->add_values(hash_func(current_loc_ip) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(10)->add_values(bucket_size - 1);

    // feat 12: user_info.current_loc.province_id
    if (user_info.has_current_loc()) {
      const auto& current_loc_province_id
        = GetFeature(FeaturePrefix::UP_USER_CURRENT_LOC_PROVINCE_ID, user_info.current_loc().province_id());
      persia_feature.mutable_sparse_feature(11)->add_values(hash_func(current_loc_province_id) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(11)->add_values(bucket_size - 1);

    // feat 13: user_info.current_loc.city_id
    if (user_info.has_current_loc()) {
      const auto& current_loc_city_id = GetFeature(FeaturePrefix::UP_USER_CURRENT_LOC_CITY_ID, user_info.current_loc().city_id());
      persia_feature.mutable_sparse_feature(12)->add_values(hash_func(current_loc_city_id) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(12)->add_values(bucket_size - 1);

    // feat 14: user_info.current_loc.poi_type
    if (user_info.has_current_loc()) {
      const auto& current_loc_poi_type = GetFeature(FeaturePrefix::UP_USER_CURRENT_LOC_POI_TYPE, user_info.current_loc().poi_type());
      persia_feature.mutable_sparse_feature(13)->add_values(hash_func(current_loc_poi_type) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(13)->add_values(bucket_size - 1);

    // feat 15: user_info.device_info.id
    if (user_info.has_device_info()) {
      const auto& device_info_id = GetFeature(FeaturePrefix::UP_USER_DEVICE_INFO_ID, hash_fn(user_info.device_info().id()));
      persia_feature.mutable_sparse_feature(14)->add_values(hash_func(device_info_id) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(14)->add_values(bucket_size - 1);

    // feat 16: user_info.device_info.app_ver
    if (user_info.has_device_info()) {
      const auto& device_info_app_ver = GetFeature(FeaturePrefix::UP_USER_DEVICE_INFO_APP_VER, hash_fn(user_info.device_info().app_ver()));
      persia_feature.mutable_sparse_feature(15)->add_values(hash_func(device_info_app_ver) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(15)->add_values(bucket_size - 1);

    // feat 17: user_info.device_info.platform
    if (user_info.has_device_info()) {
      const auto& device_info_platform = GetFeature(FeaturePrefix::UP_USER_DEVICE_INFO_PLATFORM, user_info.device_info().platform());
      persia_feature.mutable_sparse_feature(16)->add_values(hash_func(device_info_platform) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(16)->add_values(bucket_size - 1);

    // feat 18: user_info.device_info.visit_net
    if (user_info.has_device_info()) {
      const auto& device_info_visit_net = GetFeature(FeaturePrefix::UP_USER_DEVICE_INFO_VISIT_NET, hash_fn(user_info.device_info().visit_net()));
      persia_feature.mutable_sparse_feature(17)->add_values(hash_func(device_info_visit_net) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(17)->add_values(bucket_size - 1);

    // feat 19: user_info.device_info.visit_mod
    if (user_info.has_device_info()) {
      const auto& device_info_visit_mod = GetFeature(FeaturePrefix::UP_USER_DEVICE_INFO_VISIT_MOD, hash_fn(user_info.device_info().visit_mod()));
      persia_feature.mutable_sparse_feature(18)->add_values(hash_func(device_info_visit_mod) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(18)->add_values(bucket_size - 1);

    // feat 20: user_info.ad_user_info.network
    if (user_info.has_ad_user_info()) {
      const auto& ad_user_info_network = GetFeature(FeaturePrefix::UP_USER_NETWORK, hash_fn(user_info.ad_user_info().network()));
      persia_feature.mutable_sparse_feature(19)->add_values(hash_func(ad_user_info_network) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(19)->add_values(bucket_size - 1);

    // feat 21: user_info.ad_user_info.platform
    if (user_info.has_ad_user_info()) {
      const auto& ad_user_info_platform = GetFeature(FeaturePrefix::UP_USER_PLATFORM, hash_fn(user_info.ad_user_info().platform()));
      persia_feature.mutable_sparse_feature(20)->add_values(hash_func(ad_user_info_platform) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(20)->add_values(bucket_size - 1);

    // feat 22: user_info.ad_user_info.phone_price
    if (user_info.has_ad_user_info()) {
      const auto& ad_user_info_phone_price = GetFeature(FeaturePrefix::UP_USER_DEVICE_INFO_PRICE, int(user_info.ad_user_info().phone_price() / 1000));
      persia_feature.mutable_sparse_feature(21)->add_values(hash_func(ad_user_info_phone_price) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(21)->add_values(bucket_size - 1);

    // feat 23: user_info.ad_user_info.age
    if (user_info.has_ad_user_info()) {
      const auto& ad_user_info_age = GetFeature(FeaturePrefix::UP_USER_AGE, user_info.ad_user_info().age());
      persia_feature.mutable_sparse_feature(22)->add_values(hash_func(ad_user_info_age) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(22)->add_values(bucket_size - 1);

    // feat 24: user_info.ad_user_info.has_car
    if (user_info.has_ad_user_info()) {
      const auto& ad_user_info_has_car = GetFeature(FeaturePrefix::UP_USER_HAS_CAR, user_info.ad_user_info().has_car());
      persia_feature.mutable_sparse_feature(23)->add_values(hash_func(ad_user_info_has_car) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(23)->add_values(bucket_size - 1);

    // feat 25: user_info.ad_user_info.has_house
    if (user_info.has_ad_user_info()) {
      const auto& ad_user_info_has_house = GetFeature(FeaturePrefix::UP_USER_HAS_HOUSE, user_info.ad_user_info().has_house());
      persia_feature.mutable_sparse_feature(24)->add_values(hash_func(ad_user_info_has_house) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(24)->add_values(bucket_size - 1);

    // feat 26: user_info.ad_user_info.is_work
    if (user_info.has_ad_user_info()) {
      const auto& ad_user_info_is_work = GetFeature(FeaturePrefix::UP_USER_IS_WORK, user_info.ad_user_info().is_work());
      persia_feature.mutable_sparse_feature(25)->add_values(hash_func(ad_user_info_is_work) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(25)->add_values(bucket_size - 1);

    // feat 27: user_info.ad_user_info.life_stage
    if (user_info.has_ad_user_info()) {
      const auto& ad_user_info_life_stage = GetFeature(FeaturePrefix::UP_USER_LIFE_STAGE, user_info.ad_user_info().life_stage());
      persia_feature.mutable_sparse_feature(26)->add_values(hash_func(ad_user_info_life_stage) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(26)->add_values(bucket_size - 1);

    // feat 28: user_info.ad_user_info.region
    if (user_info.has_ad_user_info()) {
      const auto& ad_user_info_region = GetFeature(FeaturePrefix::UP_USER_REGION, user_info.ad_user_info().region());
      persia_feature.mutable_sparse_feature(27)->add_values(hash_func(ad_user_info_region) % (bucket_size - 1));
    }
    persia_feature.mutable_sparse_feature(27)->add_values(bucket_size - 1);

    // 预先设置 dense 特征数量
    // 此处 dense 特征数量均为 1 维
    for (size_t j = 0; j < 1; j++) {
      kuaishou::ad::algorithm::DenseFeature* dense_feature = persia_feature.add_dense_feature();
      dense_feature->set_index(j);
      for (size_t k = 0 ; k < 3; k++) {
        dense_feature->add_values(0.0);
      }
    }
    
    // dense feat 0: uid-0  
    // std::string str_user_id = results[0];
    // std::istringstream iss(str_user_id);
    uint64 int_user_id = user_info.id();
    // LOG_EVERY_N(INFO, 1) << "user_info.id(): " << user_info.id();
    // iss >> int_user_id;
    const int MOD_NUM = 10000000;
    uint64_t uid_part_0 = int_user_id % MOD_NUM;
    uint64_t uid_part_1 = (int_user_id / MOD_NUM) % MOD_NUM;
    uint64_t uid_part_2 = (int_user_id / MOD_NUM / MOD_NUM) % MOD_NUM;
    // LOG_EVERY_N(INFO, 1) << "uid_part_0: " << uid_part_0;
    // LOG_EVERY_N(INFO, 1) << "uid_part_1: " << uid_part_1;
    // LOG_EVERY_N(INFO, 1) << "uid_part_2: " << uid_part_2;
    
    persia_feature.mutable_dense_feature(0)->set_values(0, static_cast<float>(uid_part_0));
    persia_feature.mutable_dense_feature(0)->set_values(1, static_cast<float>(uid_part_1));
    persia_feature.mutable_dense_feature(0)->set_values(2, static_cast<float>(uid_part_2));

    vec.push_back(persia_feature);
    LOG_EVERY_N(INFO, 10000) << "prepare persia_feature OK";
  } else {
    LOG(FATAL) << "ExtractFeatureFromUserInfo: unsuported data type!";
  }
  return vec;
}

std::vector<std::vector<std::string>> ExtractFeatureFromUserInfo::process_line(std::string* line) {
  std::vector<std::vector<std::string>> vec;
  if ((*line) == ";") {
    LOG_EVERY_N(WARNING, 100) << "line is empty";
    return vec;
  }

  std::vector<AdPersiaFeature> features = extract_feature_from_userinfo(*line);

  for (auto& x : features) {
    std::string feature_str;
    std::string str_base64;

    x.SerializeToString(&feature_str);
    if (base::Base64Encode(feature_str, &str_base64)) {
      std::vector<std::string> one_line = make_vec_feature_from_user_info(x, str_base64);
      vec.push_back(std::move(one_line));
    }
  }

  return vec;
}

// 将 user_info, user_action 等填入 adlog 中
std::string ExtractFeatureFromUserInfoFill::FillAdlogProto(const std::string& line) {
  std::string rst;
  kuaishou::ad::algorithm::AdJointLabeledLog adlog;

  // 将 line 解析成为: user_id, user_info
  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);

  // 获取 user_info
  kuaishou::ad::algorithm::UserInfo user_info;
  std::string user_info_str = results[1];
  std::string bline;
  if (!base::Base64Decode(user_info_str, &bline) || !user_info.ParseFromString(bline)) {
    LOG_EVERY_N(INFO, 10000) << "ExtractFeatureFromUserInfoFill:" << "parse user info failed";
    return rst;
  }
  LOG_EVERY_N(INFO, 10000) << "ExtractFeatureUPModelLtr:" << "parse user info success";
  LOG_EVERY_N(INFO, 10000) << "ExtractFeatureUPModelLtr,user_info.id(): " << user_info.id();

  adlog.mutable_user_info()->CopyFrom(user_info);
  kuaishou::ad::algorithm::Item item;
  item.set_type(ItemType::AD_DSP);
  adlog.add_item()->CopyFrom(item);
  
  std::string adlog_str;
  std::string adlog_str_base64;
  adlog.SerializeToString(&adlog_str);
  if (base::Base64Encode(adlog_str, &adlog_str_base64)) {
    rst = adlog_str_base64;
  }
  return rst;
}

// ExtractFeatureFromUserInfoFill
std::vector<std::vector<std::string>> ExtractFeatureFromUserInfoFill::process_line(std::string* line) {
  std::vector<std::vector<std::string>> vec;
  if ((*line) == ";") {
    LOG_EVERY_N(WARNING, 100) << "line is empty";
    return vec;
  }

  // 将 user_info 塞入 ad_joint_labeled_log 中
  std::string curr_ad_log = FillAdlogProto(*line);
  std::vector<AdPersiaFeature> features = extract_feature_label_bundle(curr_ad_log, adlog_data_type);
  LOG_EVERY_N(INFO, 1) << "size of features: " << features.size();
  for (auto& x : features) {
    std::string feature_str;
    std::string str_base64;

    x.SerializeToString(&feature_str);
    if (base::Base64Encode(feature_str, &str_base64)) {
      LOG_EVERY_N(INFO, 1) << "str_base64: " << str_base64;
      std::vector<std::string> one_line = make_vec_feature_from_user_info(x, str_base64);
      vec.push_back(std::move(one_line));
    }
  }

  return vec;
}

// ============= ExtractFeatureFromInterestProduct ===========
ExtractFeatureFromInterestProduct::ExtractFeatureFromInterestProduct() {
  // 构造函数
  // 1. 载入 product_id 与 product_name 的对照表
  std::ifstream ifs;
  ifs.open("all_product_names.txt", std::ios::in);
  if (!ifs.is_open())
  {
    // LOG_EVERY_N(INFO, 1) << "ExtractFeatureUPModelLtr:" << "There is no file of product_name!";
    return;
  }
  std::string str;
  int idx = 0;
  while (getline(ifs, str)) {
    ifs >> str;
    str.erase(std::remove(str.begin(), str.end(), '\n'), str.cend());
    std::vector<std::string> results(0);
    base::SplitString(str, ",", &results);
    if (results.size() == 2) {
      std::string pdct_name = results[0];
      std::string pdct_id = results[1];
      LOG_EVERY_N(INFO, 100000) << "ExtractFeatureUPModelLtr:"  << "pdct_name: " << pdct_name
                          << ", pdct_id: " << pdct_id;
      product_name_id_map[pdct_name] = stof(pdct_id);
    }
  }
  ifs.close();

  // 2. 载入 product_name 的出现频次，用于头部产品打压
  std::ifstream ifs2;
  ifs2.open("conv_product_freq", std::ios::in);
  if (!ifs2.is_open())
  {
    // LOG_EVERY_N(INFO, 1) << "ExtractFeatureUPModelLtr:" << "There is no file of conv product freq!";
    return;
  }
  std::string str2;
  while (getline(ifs2, str2)) {
    ifs >> str2;
    str2.erase(std::remove(str2.begin(), str2.end(), '\n'), str2.cend());
    std::vector<std::string> results(0);
    base::SplitString(str2, ",", &results);
    if (results.size() == 4) {
      float freq = stof(results[3]);
      std::string product_name = results[1];
      product_name_freq[product_name] = freq;
    }
  }
  ifs2.close();

  // 3. 载入 product embedding
  std::ifstream ifs3;
  ifs3.open("product_embedding_for_feat", std::ios::in);
  std::string str3;
  while (getline(ifs3, str3)) {
    ifs3 >> str3;
    str3.erase(std::remove(str3.begin(), str3.end(), '\n'), str3.cend());
    std::vector<std::string> results(0);
    base::SplitString(str3, ",", &results);
    if (results.size() == 33) {
      std::string product_name = results[0];
      std::vector<float> emb_vec_float;
      for (int i = 1; i < 33; ++i) {
        std::string emb = results[i];
        float emb_float = stof(emb);
        emb_vec_float.push_back(emb_float);
      }
      product_emb_map[product_name] = emb_vec_float;
    }
  }

}

std::vector<AdPersiaFeature> ExtractFeatureFromInterestProduct::extract_feature_from_product(const std::string& line) {
  // 填充相关特征
  std::vector<AdPersiaFeature> vec;
  std::vector<std::string> results(0);
  base::SplitString(line, flags.sep, &results);
  // base::SplitString(line, ",", &results);
  // 测试输入特征格式：user_id, 产品名，一级行业ID，二级行业ID，一级类目，二级类目，三级类目，是否转化
  if (results.size() >= 8) {
    LOG_EVERY_N(INFO, 10000) << "user_id: " << results[0];
    std::istringstream iss(results[0]);
    uint64 user_id;
    iss >> user_id;
    std::string product_name = results[1];
    std::string first_industry_id = results[2];
    std::string second_industry_id = results[3];
    std::string first_mmu_cato = results[4];
    std::string second_mmu_cato = results[5];
    std::string third_mmu_cato = results[6];
    LOG_EVERY_N(INFO, 10000) << "is_conv: " << results[7];
    int64 is_conv = std::stoi(results[7]);

    // 将 product 相关字段填入特征中
    AdPersiaFeature persia_feature;
    persia_feature.set_version(1);
    persia_feature.set_user_id(user_id);
    
    // 预先设置 sparse 特征数量
    for (size_t j = 0 ; j < 6; j++) {
      kuaishou::ad::algorithm::SparseFeature* sparse_feature = persia_feature.add_sparse_feature();
      sparse_feature->set_index(j);
    }

    // 预先设置 dense 特征数量
    // 此处 dense 特征数量均为 1 维
    for (size_t j = 0; j < 4; j++) {
      kuaishou::ad::algorithm::DenseFeature* dense_feature = persia_feature.add_dense_feature();
      dense_feature->set_index(j);
      for (size_t k = 0 ; k < 1; k++) {
        dense_feature->add_values(0.0);
      }
    }

    // 设置 sparse 特征的取值
    std::hash<std::string> hash_fn;
    persia_feature.mutable_sparse_feature(0)->add_values(hash_func(hash_fn(product_name)) % (bucket_size - 1));
    persia_feature.mutable_sparse_feature(0)->add_values((bucket_size - 1));
    persia_feature.mutable_sparse_feature(1)->add_values(hash_func(hash_fn(first_industry_id)) % (bucket_size - 1));
    persia_feature.mutable_sparse_feature(1)->add_values((bucket_size - 1));
    persia_feature.mutable_sparse_feature(2)->add_values(hash_func(hash_fn(second_industry_id)) % (bucket_size - 1));
    persia_feature.mutable_sparse_feature(2)->add_values((bucket_size - 1));
    persia_feature.mutable_sparse_feature(3)->add_values(hash_func(hash_fn(first_mmu_cato)) % (bucket_size - 1));
    persia_feature.mutable_sparse_feature(3)->add_values((bucket_size - 1));
    persia_feature.mutable_sparse_feature(4)->add_values(hash_func(hash_fn(second_mmu_cato)) % (bucket_size - 1));
    persia_feature.mutable_sparse_feature(4)->add_values((bucket_size - 1));
    persia_feature.mutable_sparse_feature(5)->add_values(hash_func(hash_fn(third_mmu_cato)) % (bucket_size - 1));
    persia_feature.mutable_sparse_feature(5)->add_values((bucket_size - 1));

    // 设置 dense 特征的取值
    // dense feat 0: product_name
    float product_name_id = default_product_id;
    if (product_name_id_map.find(product_name) != product_name_id_map.end()) {
      product_name_id = product_name_id_map[product_name];
    }
    persia_feature.mutable_dense_feature(0)->set_values(0, static_cast<float>(product_name_id));

    // LOG_EVERY_N(INFO, 1) << "product_name: " << product_name
    //                      << ", product_name_id: " << product_name_id
    //                      << ", float: " << static_cast<float>(product_name_id);

    // dense feat 1: first_industry
    std::istringstream iss_1(first_industry_id);
    uint64 first_id = 0;
    iss_1 >> first_id;
    persia_feature.mutable_dense_feature(1)->set_values(0, static_cast<float>(first_id));

    // dense feat 2: second_industry
    std::istringstream iss_2(second_industry_id);
    uint64 second_id = 0;
    iss_2 >> second_id;
    persia_feature.mutable_dense_feature(2)->set_values(0, static_cast<float>(second_id));

    // dense feat 3: product_freq
    float product_freq = 1.0;
    if (product_name_freq.find(product_name) != product_name_freq.end()) {
      product_freq = product_name_freq[product_name];
    }
    LOG_EVERY_N(INFO, 100000) << "ExtractFeatureUPModelLtr:" << "product_name: " << product_name
                         << "product_freq: " << product_freq;
    persia_feature.mutable_dense_feature(3)->set_values(0, product_freq);

    // dense feat 4: product embedding
    // [TODO]
    kuaishou::ad::algorithm::DenseFeature* dense_feature = persia_feature.add_dense_feature();
    dense_feature->set_index(4);
    for (size_t k = 0 ; k < 32; k++) {
      dense_feature->add_values(0.0);
    }

    if (product_emb_map.find(product_name) != product_emb_map.end()
        && product_emb_map[product_name].size() == 32) {
      std::vector<float> curr_emb = product_emb_map[product_name];
      for (size_t k = 0; k < 32; k++) {
        persia_feature.mutable_dense_feature(4)->set_values(k, curr_emb[k]);
      }
    }

    // 设置 label
    persia_feature.add_final_label(is_conv);
    vec.push_back(persia_feature);
  } else {
    LOG(FATAL) << "ExtractFeatureUPModelLtr:" << "ExtractFeatureFromUserInfo: unsuported data type!";
  }
  return vec;
}

std::vector<std::vector<std::string>> ExtractFeatureFromInterestProduct::process_line(std::string* line) {
  std::vector<std::vector<std::string>> vec;
  if ((*line) == ";") {
    LOG_EVERY_N(WARNING, 100) << "ExtractFeatureUPModelLtr:" << "line is empty";
    return vec;
  }
 
  std::vector<AdPersiaFeature> features = extract_feature_from_product(*line);

  for (auto& x : features) {
    std::string feature_str;
    std::string str_base64;

    x.SerializeToString(&feature_str);
    if (base::Base64Encode(feature_str, &str_base64)) {
      std::vector<std::string> one_line = make_vec_feature_from_user_info(x, str_base64);
      vec.push_back(std::move(one_line));
    }
  }

  return vec;
}

std::vector<std::vector<std::string>> ExtracProductCityHash64Id::process_line(std::string* line) {
  std::vector<std::vector<std::string>> vec;
  if ((*line) == ";") {
    LOG_EVERY_N(WARNING, 100) << "line is empty";
    return vec;
  }
  std::vector<std::string> results(0);
  // base::SplitString(*line, flags.sep, &results);
  base::SplitString(*line, ",", &results);
  if (results.size() >= 1) {
    std::string product_name = results[0];
    int64_t product_name_id = base::CityHash64(product_name.data(), product_name.size());
    // uint64 转 string
    std::ostringstream os;
    os << product_name_id;
    std::string city_product_id = os.str();
    // LOG_EVERY_N(INFO, 1) << "product_name: " << product_name
    //                      << ", city_product_id: " << city_product_id;

    std::vector<std::string> curr_vec;
    curr_vec.push_back(std::move(product_name));
    curr_vec.push_back(std::move(city_product_id));
    vec.push_back(std::move(curr_vec));
  }

  return vec;
}

// ============= FilterLog ===========
std::vector<std::vector<std::string>> FilterLog::process_line(std::string* line) {
  // 对于单 item 没问题，多 item 过滤有问题
  std::vector<std::vector<std::string>> vec;
  if ((*line) == ";") {
    LOG_EVERY_N(WARNING, 100) << "line is empty";
    return vec;
  }
  ks::ad_algorithm::AdLogWrapper adlogWrapper;
  ks::ad_algorithm::AdLogExtend adlogExtend;
  tensorflow::Tensor t;

  std::vector<std::string> results;
  base::SplitString(*line, flags.sep, &results);

  std::vector<std::string> debug_infos;
  std::pair<bool, std::unique_ptr<ks::ad_algorithm::AdLog>> parse_res =
    adlog_parser.parse_adlog(*line, PreprocessType::All, &t, &debug_infos, 2);
  if (parse_res.first) {
    std::string s;
    std::string s_base64;
    if (parse_res.second->Get().SerializeToString(&s)) {
      if (base::Base64Encode(s, &s_base64)) {
        vec.push_back(std::move(make_vec(s_base64)));
      }
    }
  }

  return vec;
}

// ============= FilterLogPvId ===========
std::vector<std::vector<std::string>> FilterLogPvId::process_line(std::string* line) {
  std::vector<std::vector<std::string>> vec;
  if ((*line) == ";") {
    LOG_EVERY_N(WARNING, 100) << "line is empty";
    return vec;
  }
  tensorflow::Tensor t;

  std::vector<std::string> results;
  base::SplitString(*line, flags.sep, &results);

  std::pair<bool, std::unique_ptr<ks::ad_algorithm::AdLog>> parse_res =
    adlog_parser.parse_adlog(*line, PreprocessType::All, &t);
  if (parse_res.first) {
    AdJointLabeledLog adlog;
    std::string bline;

    if (!base::Base64Decode(*line, &bline) || !adlog.ParseFromString(bline)) {
      cerr << "parse user ad info failed" << std::endl;
      return vec;
    }

    if (adlog.item().size() == 1) {
      auto llsid = adlog.llsid();
      auto creative_id = adlog.item(0).ad_dsp_info().creative().base().id();
      auto unit_id = adlog.item(0).ad_dsp_info().unit().base().id();
      auto account_id = adlog.item(0).ad_dsp_info().unit().base().account_id();
      std::vector<std::string> info;
      info.push_back(std::move(*line));
      info.push_back(std::move(std::to_string(llsid)));
      info.push_back(std::move(std::to_string(creative_id)));
      info.push_back(std::move(std::to_string(account_id)));
      info.push_back(std::move(std::to_string(unit_id)));
      vec.push_back(std::move(info));
    } else {
      for (size_t i = 0; i < adlog.item().size(); i++) {
        kuaishou::ad::algorithm::AdJointLabeledLog adlog_i;
        adlog_i.CopyFrom(adlog);
        kuaishou::ad::algorithm::AdJointLabeledLog adlog_copy;
        auto & item = adlog.item(i);
        adlog_copy.add_item()->CopyFrom(item);
        adlog_i.mutable_item()->Swap(adlog_copy.mutable_item());

        auto llsid = adlog_i.llsid();
        auto creative_id = adlog_i.item(0).ad_dsp_info().creative().base().id();
        auto account_id = adlog_i.item(0).ad_dsp_info().unit().base().account_id();
        auto unit_id = adlog_i.item(0).ad_dsp_info().unit().base().id();

        std::string adlog_string_i;
        std::string base64result_i;
        adlog_i.SerializeToString(&adlog_string_i);
        if (!base::Base64Encode(adlog_string_i, &base64result_i)) {
          cerr << "encode base64 adlog failed!";
        }
        std::vector<std::string> info;
        info.push_back(std::move(base64result_i));
        info.push_back(std::move(std::to_string(llsid)));
        info.push_back(std::move(std::to_string(creative_id)));
        info.push_back(std::move(std::to_string(account_id)));
        info.push_back(std::move(std::to_string(unit_id)));
        vec.push_back(std::move(info));
      }
    }
  }

  return vec;
}

// ============= TargetFixFeature ===========
std::vector<std::vector<std::string>> TargetFixFeature::process_line(std::string* line) {
  std::vector<std::vector<std::string>> vec;
  if ((*line) == ";") {
    LOG_EVERY_N(WARNING, 100) << "line is empty";
    return vec;
  }
  std::vector<AdPersiaFeature> features = extract_feature_label(*line, adlog_data_type);
  auto pos_set = PosSetFix::GetInstance();
  bool neg_sample = flags.ns < 1.0;
  int ns_threshold = static_cast<int>(0xfff * flags.ns);
  for (auto& persia_feature : features) {
    persia_feature.clear_final_label();
    const auto & llsid = persia_feature.llsid();
    const auto & item_id = persia_feature.item_id();
    uint64 key = (llsid << 32) + item_id;
    if (pos_set->contains(key)) {
      persia_feature.add_final_label(1);
    } else {
      if (neg_sample && (rand_r(&seed) &0xfff) > ns_threshold) {
          continue;
      }
      persia_feature.add_final_label(0);
    }

    std::string feature_str;
    persia_feature.SerializeToString(&feature_str);
    std::string str_base64;
    if (base::Base64Encode(feature_str, &str_base64)) {
      if (flags.random_key) {
        std::string key = std::to_string(dist(gen));
        vec.push_back(std::move(make_vec(key, str_base64)));
      } else {
        vec.push_back(std::move(make_vec(str_base64)));
      }
    }
  }

  return vec;
}

// ============= TargetFixMTLFeature ===========
std::vector<AdPersiaFeature> TargetFixMTLFeature::extract_feature_label(const std::string& line,
                                                                        AdlogDataType adlog_data_type,
                                                                        std::vector<int>* delivery_vec) {
  std::vector<AdPersiaFeature> vec;
  ks::ad_algorithm::AdLog* p_adlog;
  tensorflow::Tensor t;
  std::vector<std::string> extra_fields;

  std::pair<bool, std::unique_ptr<ks::ad_algorithm::AdLog>> parse_res =
    adlog_parser.parse_adlog(line, PreprocessType::All, &t);
  if (!parse_res.first) {
    return vec;
  }

  p_adlog = parse_res.second.get();
  sparse_results.clear();
  dense_results.clear();
  feature_extractor->ExtractAllAdLogFeatures(*p_adlog, &sparse_results);
  feature_extractor->ExtractAllAdLogDenseFeatures(*p_adlog, &dense_results);

  if (is_need_label) {
    for (size_t i = 0; i < p_adlog->item_size(); ++i) {
      AdPersiaFeature persia_feature = extract_persia_feature(*p_adlog, i);
      const auto & label_vec = get_label_value(t, i);
      if (label_vec.size() > 0 && label_vec[0] != -1) {
        for (auto value : label_vec) {
          persia_feature.add_final_label(value);
        }

        vec.push_back(std::move(persia_feature));
        delivery_vec->push_back(p_adlog->item(i).label_info().delivery());
      }
    }
  } else {
    AdPersiaFeature persia_feature = extract_persia_feature(*p_adlog, 0);
    vec.push_back(std::move(persia_feature));
    delivery_vec->push_back(p_adlog->item(0).label_info().delivery());
  }

  return vec;
}

std::vector<std::vector<std::string>> TargetFixMTLFeature::process_line(std::string* line) {
  std::vector<std::vector<std::string>> vec;
  if ((*line) == ";") {
    LOG_EVERY_N(WARNING, 100) << "line is empty";
    return vec;
  }
  std::vector<int> deliveries;
  std::vector<AdPersiaFeature> features = extract_feature_label(*line, adlog_data_type, &deliveries);
  auto pos_set = PosSetFixMtl::GetInstance();
  bool neg_sample = flags.ns < 1.0;
  int ns_threshold = static_cast<int>(0xfff * flags.ns);
  for (int i =0 ; i < features.size(); i++) {
    auto & persia_feature = features[i];
    int item_delivery = deliveries[i];
    // Find label
    const auto & llsid = persia_feature.llsid();
    const auto & item_id = persia_feature.item_id();
    uint64 key = (llsid << 32) + item_id;
    int item_imp = 0;
    int item_click = 0;
    int label = 0;
    if (item_delivery == 1) {
      label |= 1 << (2 * 0 + 1);
    } else {
      if (neg_sample && (rand_r(&seed) & 0xfff) > ns_threshold) {
          continue;
      }
      label |= 1 << (2 * 0);
    }
    if (pos_set->contains(key)) {
      label |= 1 << (2 * 1 + 1);
    } else {
      label |= 1 << (2 * 1);
    }
    if (pos_set->contains2(key)) {
      label |= 1 << (2 * 2 + 1);
    } else {
      label |= 1 << (2 * 2);
    }

    persia_feature.clear_final_label();
    persia_feature.add_final_label(label);

    std::string feature_str;
    persia_feature.SerializeToString(&feature_str);
    std::string str_base64;
    if (base::Base64Encode(feature_str, &str_base64)) {
      if (flags.random_key) {
        std::string key = std::to_string(dist(gen));
        vec.push_back(std::move(make_vec(key, str_base64)));
      } else {
        vec.push_back(std::move(make_vec(str_base64)));
      }
    }
  }

  return vec;
}

std::vector<std::vector<std::string>> ExtractFeatureFederate::process_line(std::string* line) {
  std::vector<std::vector<std::string>> vec;
  if ((*line) == ";") {
    LOG_EVERY_N(WARNING, 100) << "line is empty";
    return vec;
  }

  std::vector<AdPersiaFeature> features = extract_feature_label_bundle(*line, adlog_data_type);
  for (auto& persia_feature : features) {
    kuaishou::ad::algorithm::AdFederateFeature fed_feature;
    fed_feature.set_llsid(persia_feature.llsid());
    fed_feature.set_item_id(persia_feature.item_id());

    for (int i = 0; i < persia_feature.sparse_feature_size(); ++i) {
      kuaishou::ad::algorithm::SparseFeature* sparse_feature = fed_feature.add_sparse_feature();
      sparse_feature->CopyFrom(persia_feature.sparse_feature(i));
    }
    for (int i = 0; i < persia_feature.dense_feature_size(); ++i) {
      kuaishou::ad::algorithm::DenseFeature* dense_feature = fed_feature.add_dense_feature();
      dense_feature->CopyFrom(persia_feature.dense_feature(i));
    }

    std::string feature_str;
    fed_feature.SerializeToString(&feature_str);
    std::string str_base64;
    if (base::Base64Encode(feature_str, &str_base64)) {
      if (flags.random_key) {
        std::string key = std::to_string(dist(gen));
        vec.push_back(std::move(make_vec(key, str_base64)));
      } else {
        vec.push_back(std::move(make_vec(str_base64)));
      }
    }
  }

  return vec;
}

std::vector<std::vector<std::string>> ExtractSimpleFeatures::process_line(std::string* line) {
  std::vector<std::vector<std::string>> vec;
  if ((*line) == ";") {
    LOG_EVERY_N(WARNING, 100) << "line is empty";
    return vec;
  }

  std::vector<AdPersiaFeature> features = extract_feature_label_bundle(*line, adlog_data_type);
  for (auto& persia_feature : features) {
    kuaishou::ad::algorithm::SimpleFeatures simple_feature;

    simple_feature.set_user_id(persia_feature.user_id());
    simple_feature.set_llsid(persia_feature.llsid());
    simple_feature.set_item_id(persia_feature.item_id());
    simple_feature.set_timestamp(persia_feature.time());

    for (int i = 0; i < persia_feature.sparse_feature_size(); ++i) {
      kuaishou::ad::algorithm::SparseFeature* sparse_feature = simple_feature.add_sparse_feature();
      sparse_feature->CopyFrom(persia_feature.sparse_feature(i));
    }
    for (int i = 0; i < persia_feature.dense_feature_size(); ++i) {
      kuaishou::ad::algorithm::DenseFeature* dense_feature = simple_feature.add_dense_feature();
      dense_feature->CopyFrom(persia_feature.dense_feature(i));
    }

    std::string feature_str;
    simple_feature.SerializeToString(&feature_str);
    std::string str_base64;
    if (base::Base64Encode(feature_str, &str_base64)) {
      if (flags.random_key) {
        std::string key = std::to_string(dist(gen));
        vec.push_back(std::move(make_vec(key, str_base64)));
      } else {
        vec.push_back(std::move(make_vec(str_base64)));
      }
    }
  }

  return vec;
}

using AdLiveGlobalGsuProcessor = HardSearchGsuProcessor<ColossusType::AD_LIVE_ITEM,
      GsuHardSearchKeyType::GLOBAL_CLUSTER, 50, 5>;
using AdLiveEcommGsuProcessor = HardSearchGsuProcessor<ColossusType::AD_LIVE_ITEM,
      GsuHardSearchKeyType::ECOMM_CLUSTER, 50, 5>;
using AdLiveAuthorGsuProcessor = HardSearchGsuProcessor<ColossusType::AD_LIVE_ITEM,
      GsuHardSearchKeyType::AUTHOR_ID, 50, 5>;
using AdLiveAuthorClusterGsuProcessor = HardSearchGsuProcessor<ColossusType::AD_LIVE_ITEM,
      GsuHardSearchKeyType::AUTHOR_CLUSTER, 50, 5>;
using AdLiveRemoteClusterGsuProcessor = HardSearchGsuProcessor<ColossusType::AD_LIVE_ITEM,
      GsuHardSearchKeyType::REMOTE_CLUSTER, 50, 5>;
using AdLiveSpuGsuProcessor = HardSearchGsuProcessor<ColossusType::AD_LIVE_ITEM,
      GsuHardSearchKeyType::ITEM_SPU, 50, 5>;
using AdLiveCid3GsuProcessor = HardSearchGsuProcessor<ColossusType::AD_LIVE_ITEM,
      GsuHardSearchKeyType::ITEM_CID3, 50, 5>;
using RecoLiveRemoteClusterGsuProcessor = HardSearchGsuProcessor<ColossusType::RECO_LIVE_ITEM,
      GsuHardSearchKeyType::REMOTE_CLUSTER, 50, 100000>;
using RecoLiveV1RemoteClusterGsuProcessor = HardSearchGsuProcessor<ColossusType::RECO_LIVE_ITEM_V1,
      GsuHardSearchKeyType::REMOTE_CLUSTER, 50, 100000>;
using RecoLiveGlobalGsuProcessor = HardSearchGsuProcessor<ColossusType::RECO_LIVE_ITEM,
      GsuHardSearchKeyType::GLOBAL_CLUSTER, 50, 100000>;
using RecoLiveAuthorGsuProcessor = HardSearchGsuProcessor<ColossusType::RECO_LIVE_ITEM,
      GsuHardSearchKeyType::AUTHOR_ID, 50, 100000>;
using RecoLiveAuthorClusterGsuProcessor = HardSearchGsuProcessor<ColossusType::RECO_LIVE_ITEM,
      GsuHardSearchKeyType::AUTHOR_CLUSTER, 50, 100000>;
using RedisItemSpuProcessor = RedisItemAttrProcessor<RedisItemAttrType::ITEM_SPU>;
using RedisItemCid3Processor = RedisItemAttrProcessor<RedisItemAttrType::ITEM_CID3>;
using AdLiveColossusPlaytimeFilterProcessor = ColossusFilterProcessor<ColossusType::AD_LIVE_ITEM,
ColossusFilterKeyType::LIVE_PLAY_TIME, 200, 0, 0, 30>;

REGISTER_PROCESSOR(AdLiveGlobalGsuProcessor);
REGISTER_PROCESSOR(AdLiveEcommGsuProcessor);
REGISTER_PROCESSOR(AdLiveAuthorGsuProcessor);
REGISTER_PROCESSOR(AdLiveAuthorClusterGsuProcessor);
REGISTER_PROCESSOR(AdLiveRemoteClusterGsuProcessor);
REGISTER_PROCESSOR(AdLiveSpuGsuProcessor);
REGISTER_PROCESSOR(AdLiveCid3GsuProcessor);
REGISTER_PROCESSOR(RecoLiveRemoteClusterGsuProcessor);
REGISTER_PROCESSOR(RecoLiveV1RemoteClusterGsuProcessor);
REGISTER_PROCESSOR(RecoLiveGlobalGsuProcessor);
REGISTER_PROCESSOR(RecoLiveAuthorGsuProcessor);
REGISTER_PROCESSOR(RecoLiveAuthorClusterGsuProcessor);
REGISTER_PROCESSOR(RedisItemSpuProcessor);
REGISTER_PROCESSOR(RedisItemCid3Processor);
REGISTER_PROCESSOR(AdLiveColossusPlaytimeFilterProcessor);

void FeatureProcessor::Init(const std::vector<std::string>& processor_names) {
  auto processors = ClassFactory::GetInstance().GetAllClass();
  LOG(INFO) << "begin FeatureProcessor::Init, processors size: " << processors.size();
  for (const std::string& name : processor_names) {
    auto processor = ClassFactory::GetInstance().GetClassByName(name);
    if (!processor) {
      LOG(FATAL) << "processor " << name << " not found";
      continue;
    }
    if (processor->DoInit()) {
      std::unique_ptr<FeatureProcessor> processor_ptr(processor);
      valid_processors_.emplace_back(std::move(processor_ptr));
    } else {
      LOG(FATAL) << "processor " << name << " DoInit failed";
    }
    LOG(INFO) << "processor " << name << " DoInit success";
  }
}
void FeatureProcessor::Process(ks::ad_algorithm::AdLogWrapper* adlog) {
  for (auto &iter : valid_processors_) {
    iter->DoProcess(adlog);
  }
}

}  // namespace ad_algorithm
}  // namespace ks
