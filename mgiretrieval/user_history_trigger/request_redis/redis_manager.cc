#include "teams/ad/ad_algorithm/mr/request_redis/redis_manager.h"
#include "base/common/logging.h"
// #include "ks/base/container/common.h"
#include "teams/ad/ad_nn/twin_towers_ps/kconf.h"

namespace ks {
namespace ad_algorithm {

bool RedisManager::GetRawDataFromRedis(const std::string& key, std::string* raw_string) {
  if (raw_string == nullptr) {
    LOG(ERROR) << "Invalid pointer.";
    return false;
  }
  auto err_code = redis_client_->Get(key, raw_string);
  if (err_code != ks::infra::RedisErrorCode::KS_INF_REDIS_NO_ERROR ||
      raw_string->empty()) {
    LOG_EVERY_N(ERROR, 100) << "Get trigger embedding str fail, key: " << key
                            << ", string size: " << raw_string->size()
                            << ", err code: " << ks::infra::err2str(err_code);
    return false;
  }
  return true;
}

bool RedisManager::GetRawSetDataFromRedisByBatch(const std::vector<std::string>& keys,
                                                 std::vector<std::string>* raw_string) {
  if (raw_string == nullptr) {
    LOG(ERROR) << "Invalid pointer.";
    return false;
  }
  thread_local std::set<std::string> members;
  members.clear();
  for (const auto& key : keys) {
    ks::infra::RedisResponse<std::set<std::string>> responses =
        redis_async_client_->SGetMembers(key);
    members.clear();
    auto err_code = responses.Get(&members);
    if (err_code != ks::infra::RedisErrorCode::KS_INF_REDIS_NO_ERROR) {
      LOG_EVERY_N(ERROR, 1000) << "Get redis fail, error code:" << err_code
                               << ", err code: " << ks::infra::err2str(err_code)
                               << ", key: " << key;
      continue;
    }
    for (auto& val : members) {
      raw_string->emplace_back(val);
    }
  }

  return true;
}

bool RedisManager::GetRawDataFromRedisByBatch(const std::vector<std::string>& keys,
                                              std::vector<std::string>* raw_string) {
  if (raw_string == nullptr) {
    LOG(ERROR) << "Invalid pointer.";
    return false;
  }
  ks::infra::RedisResponse<std::vector<std::string>> responses =
      redis_async_client_->MGet(keys);

  auto err_code = responses.Get(raw_string);
  if (err_code != ks::infra::RedisErrorCode::KS_INF_REDIS_NO_ERROR ||
      raw_string->empty()) {
    LOG_EVERY_N(ERROR, 100) << "Get trigger embedding str fail, error code: "
                            << ", err code: " << ks::infra::err2str(err_code);
    return false;
  }
  return true;
}

void RedisManager::Initialize(const std::string& cluster_name,
                              const RedisEncodeType& encode_type,
                              const std::string& prefix_key) {
  redis_client_ = ks::infra::RedisProxyClient::GetRedisClientByKccFromKconf(cluster_name,
                                                                            timeout_ms_);
  if (redis_client_ == nullptr) {
    LOG(FATAL) << "Failed to create redis client, cluster name: " << cluster_name;
  }
  redis_async_client_ = ks::infra::RedisProxyClient::GetRedisPipelineClientByKccFromKconf(
      cluster_name, timeout_ms_);
  if (redis_async_client_ == nullptr) {
    LOG(FATAL) << "Failed to create redis async client, cluster name: " << cluster_name;
  }
  LOG_FIRST_N(INFO, 3) << "Connect to redis cluster success, cluster name: "
                       << cluster_name << ", prefix key " << prefix_key
                       << ", encode type " << static_cast<int>(encode_type)
                       << ", timeout ms: " << timeout_ms_;
}

}  // namespace ad_algorithm
}  // namespace ks
