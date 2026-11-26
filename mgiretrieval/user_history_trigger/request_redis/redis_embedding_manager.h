#pragma once
#include <string>
#include <vector>
#include "infra/redis_proxy_client/src/redis_proxy_client/redis_proxy_client.h"
// #include "teams/ad/ad_nn/twin_towers_ps/common.h"
#include "teams/ad/ad_algorithm/mr/request_redis/redis_manager.h"
#include "teams/ad/ad_proto/kuaishou/retrieval/retrieval_service.pb.h"
// #include "tensorflow/core/framework/tensor.h"

namespace ks {
namespace ad_algorithm {

template <typename T>
class RedisEmbeddingManager : public RedisManager {
 public:
  static RedisEmbeddingManager* Instance() {
    static RedisEmbeddingManager em;
    return &em;
  }
  RedisEmbeddingManager() : RedisManager() {
    LOG(INFO) << "Construct RedisEmbeddingManager";
  }
  virtual ~RedisEmbeddingManager() { LOG(INFO) << "Deconstruct RedisEmbeddingManager"; }

  bool GetEmbeddingFromRedis(const uint64_t& id, T* value);
  bool GetEmbeddingFromRedis(const uint64_t& id, const std::string& prefix_key, T* value);

  bool GetEmbeddingFromRedisByBatch(const std::vector<uint64_t>& id,
                                    std::vector<T>* value);
  bool GetEmbeddingFromRedisByBatch(const std::vector<uint64_t>& id,
                                    const std::string& prefix_key, std::vector<T>* value);
};

}  // namespace ad_algorithm
}  // namespace ks
