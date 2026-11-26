#include "teams/ad/ad_algorithm/mr/request_redis/redis_embedding_manager.h"
#include "base/common/logging.h"
#include "base/encoding/base64.h"
#include "ks/base/container/common.h"
#include "teams/ad/ad_nn/twin_towers_ps/kconf.h"
#include "teams/ad/ad_proto/kuaishou/retrieval/retrieval_service.pb.h"

namespace ks {
namespace ad_algorithm {

template <typename T>
bool RedisEmbeddingManager<T>::GetEmbeddingFromRedis(const uint64_t& id,
                                                     const std::string& key_prefix,
                                                     T* value) {
  if (value == nullptr) {
    LOG(ERROR) << "Invalid pointer.";
    return false;
  }
  thread_local std::string embedding_str;
  thread_local std::string embedding_str_decoded;
  embedding_str.clear();
  embedding_str_decoded.clear();
  const std::string key =
      key_prefix.empty() ? std::to_string(id) : (key_prefix + "_" + std::to_string(id));
  if (KS_UNLIKELY(!GetRawDataFromRedis(key, &embedding_str))) {
    return false;
  }
  switch (encode_type_) {
  case RedisEncodeType::RAW_STRING: {
    if (embedding_str.empty() || !value->ParseFromString(embedding_str)) {
      LOG(ERROR) << "Parse user embedding string fail, string size: "
                 << embedding_str.size() << ", key: " << key;
      return false;
    }
    break;
  }
  case RedisEncodeType::BASE64: {
    if (!base::Base64Decode(embedding_str, &embedding_str_decoded)) {
      LOG(ERROR) << "Failed to decode embedding string by base64";
      return false;
    }
    if (embedding_str_decoded.empty() || !value->ParseFromString(embedding_str_decoded)) {
      LOG(ERROR) << "parse user embedding_str fail, string size: "
                 << embedding_str_decoded.size() << ", key: " << key;
      return false;
    }
    break;
  }
  default:
    LOG(ERROR) << "Unsupported encode type " << static_cast<int>(encode_type_);
    return false;
    break;
  }
  return true;
}

template <typename T>
bool RedisEmbeddingManager<T>::GetEmbeddingFromRedisByBatch(
    const std::vector<uint64_t>& ids, const std::string& key_prefix,
    std::vector<T>* values) {
  if (values == nullptr) {
    LOG(ERROR) << "Invalid pointer.";
    return false;
  }
  thread_local std::vector<std::string> embedding_strings;
  thread_local std::vector<std::string> keys;
  embedding_strings.clear();
  keys.clear();
  for (int i = 0; i < ids.size(); ++i) {
    uint64_t id = ids[i];
    const std::string key =
        key_prefix.empty() ? std::to_string(id) : (key_prefix + "_" + std::to_string(id));
    keys.emplace_back(key);
  }
  if (KS_UNLIKELY(!GetRawDataFromRedisByBatch(keys, &embedding_strings))) {
    LOG(ERROR) << "Failed to get raw data from redis by batch, keys size " << keys.size();
    return false;
  }
  if (keys.size() != embedding_strings.size()) {
    LOG(ERROR) << "Dismatch with keys size " << keys.size()
               << ", and embedding string size " << embedding_strings.size();
    return false;
  }
  for (int i = 0; i < embedding_strings.size(); ++i) {
    const std::string& embedding_str = embedding_strings[i];
    T value;
    if (embedding_str.size() > 0) {
      if (!value.ParseFromString(embedding_str)) {
        LOG(ERROR) << "Parse user embedding_str fail, string size: "
                   << embedding_str.size() << ", key: " << keys[i];
        continue;
      }
      values->emplace_back(value);
    } else {
      LOG_EVERY_N(ERROR, 5000) << "Failed to get value from redis, key " << keys[i];
      continue;
    }
  }
  return true;
}

template <typename T>
bool RedisEmbeddingManager<T>::GetEmbeddingFromRedis(const uint64_t& id, T* value) {
  return GetEmbeddingFromRedis(id, prefix_key_, value);
}

template <typename T>
bool RedisEmbeddingManager<T>::GetEmbeddingFromRedisByBatch(
    const std::vector<uint64_t>& ids, std::vector<T>* values) {
  return GetEmbeddingFromRedisByBatch(ids, prefix_key_, values);
}

using kuaishou::ad::EmbeddingId2IdWithMeta;
using kuaishou::retr::CommonSpec;
using kuaishou::retr::Embeddings;
template class RedisEmbeddingManager<Embeddings>;
template class RedisEmbeddingManager<CommonSpec>;
template class RedisEmbeddingManager<EmbeddingId2IdWithMeta>;
}  // namespace ad_algorithm
}  // namespace ks
