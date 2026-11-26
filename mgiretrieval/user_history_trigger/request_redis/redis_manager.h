#pragma once
#include <string>
#include <vector>
#include "infra/redis_proxy_client/src/redis_proxy_client/redis_proxy_client.h"
#include "teams/ad/ad_nn/kconf_manager/trigger_config.h"
#include "teams/ad/ad_proto/kuaishou/ad/embedding_retr_item.pb.h"
#include "teams/ad/ad_proto/kuaishou/retrieval/retrieval_service.pb.h"
// #include "teams/ad/ad_nn/twin_towers_ps/common.h"
// #include "tensorflow/core/framework/tensor.h"

namespace ks {
namespace ad_algorithm {
using ks::ad_twin_towers::RedisEncodeType;
using kuaishou::ad::EmbeddingId2IdWithMeta;
using kuaishou::retr::CommonSpec;
using kuaishou::retr::Embeddings;

class RedisManager {
 public:
  explicit RedisManager() = default;
  virtual ~RedisManager() = default;

  virtual bool GetRawDataFromRedis(const std::string& key, std::string* raw_string);
  virtual bool GetRawDataFromRedisByBatch(const std::vector<std::string>& key,
                                          std::vector<std::string>* raw_string);
  virtual bool GetRawSetDataFromRedisByBatch(const std::vector<std::string>& key,
                                             std::vector<std::string>* raw_string);
  virtual void ParseConfig();
  virtual void Initialize(const std::string& cluster_name,
                          const RedisEncodeType& encode_type,
                          const std::string& prefix_key);

 protected:
  std::shared_ptr<Json> config_;
  std::string prefix_key_;
  std::string cluster_name_;
  int timeout_ms_ = 10;
  ks::infra::RedisClient* redis_client_ = nullptr;
  ks::infra::RedisPipelineClient* redis_async_client_ = nullptr;
  RedisEncodeType encode_type_ = RedisEncodeType::RAW_STRING;
};

}  // namespace ad_algorithm
}  // namespace ks
