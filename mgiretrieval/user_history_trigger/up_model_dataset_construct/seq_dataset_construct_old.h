#pragma once

#include <stdexcept>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <cstdlib> // std::rand()
#include <ctime>
#include <json/json.h>

namespace ks {
namespace ad_algorithm {

#define __SEQ_DATASET_CONSTRUCTOR_DEBUG

#ifdef __SEQ_DATASET_CONSTRUCTOR_DEBUG
#define debug_log(s) {std::cout << s << std::endl;}
#else
#define debug_log(s) {}
#endif

#ifdef __SEQ_DATASET_TIMECOST_DEBUG
std::clock_t _debug_clock_now = 0;
#define TIME_BEGIN {_debug_clock_now = std::clock();}
#define TIME_END (1.0 * (std::clock() - _debug_clock_now) / CLOCKS_PER_SEC)
#else
#define TIME_BEGIN {}
#define TIME_END -1
#endif

struct ActionItem {
    std::string product_name;
    int64_t product_name_id;
    int64_t delivery_timestamp;
    int64_t label_ped;
    int64_t label_click;
    int64_t label_conv;
    std::string generate_strategy; // 产生的策略，如"sample_from_neg_action" "sample_from_freq"
    ActionItem()
        : product_name_id(0), delivery_timestamp(0), label_ped(0), label_click(0), label_conv(0)
    {}
    friend std::ostream & operator<< (std::ostream &o, ActionItem const &s)
    // std::ostream & operator<<(std::ostream & o)
    {
        o << s.product_name << "#" << s.product_name_id << "#" << s.label_ped << s.label_click << s.label_conv;
        #ifdef __SEQ_DATASET_CONSTRUCTOR_DEBUG
        o << "#" << s.generate_strategy;
        #endif
        return o;
    }

};

//  point样本
struct SeqPointwiseSample {
    int64_t user_id;
    std::vector<ActionItem> user_history_list;
    std::vector<ActionItem> user_target_list;
    std::string toString() {
        std::stringstream ss;
        ss << user_id << ",";
        for (size_t j = 0; j < user_history_list.size(); j ++) {
            if (j >= 1) {
                ss << "_";
            }
            ss << user_history_list[j];
        }
        ss << ",";
        for (size_t j = 0; j < user_target_list.size(); j ++) {
            if (j >= 1) {
                ss << "_";
            }
            ss << user_target_list[j];
        }
        return ss.str();
    }
};

// 一个用户的原始序列
struct SeqRawData {
    int64_t user_id;
    std::string user_long_history_json;
    SeqRawData(int64_t user_id, const std::string& user_long_history_json)
        :user_id(user_id), user_long_history_json(user_long_history_json)
    {
    }
    // 构造函数，输入了一行
    SeqRawData(const std::string& line) {
        try {
            size_t i_comma = line.find(',');
            if (i_comma != std::string::npos) {
                user_id = std::stoll(line.substr(0, i_comma));
                size_t i_nul_0 = line.find('\0', i_comma + 1);
                if (i_nul_0 != std::string::npos) {
                    size_t i_nul_1 = line.find('\0', i_nul_0 + 1);
                    if (i_nul_1 != std::string::npos) {
                        user_long_history_json = line.substr(i_nul_0 + 1, i_nul_1 - i_nul_0 - 1);
                    }
                }
            }
        } catch (const std::exception& e ) {

        }
    }
};

class SeqPointwiseDatasetConstructor {
private:
    std::set<std::string> _event_conv_set;
    size_t _window_size;
    size_t _min_length;
    size_t _neg_k_freq; // 负采样倍数，一个正样本配几个频率采样负样本
    size_t _neg_k_negactoin; // 行为中的负样本
    size_t _table_size; // 1e8:0.75s, 1e7: 0.09s
    std::unordered_map<std::string, float> _mp_product_name_freq;
    std::unordered_map<std::string, int> _mp_product_name_index_of_vocab_table;
    std::vector<std::string> vocab_table;
    std::vector<int> unigram_table;
public:
    SeqPointwiseDatasetConstructor() {
        _event_conv_set.insert("EVENT_CONVERSION");
        _event_conv_set.insert("EVENT_FORM_SUBMIT");
        _event_conv_set.insert("EVENT_APP_INVOKED");
        _event_conv_set.insert("EVENT_PAY");
        _event_conv_set.insert("EVENT_NEXTDAY_STAY");
        _event_conv_set.insert("EVENT_KEY_INAPP_ACTION");
    }
    void init(const std::unordered_map<std::string, float>& mp_product_name_freq, size_t window_size = 3, size_t min_length = 3,
            size_t neg_k_freq = 5, size_t table_size = 1e7, size_t neg_k_negactoin = 5) {
        _window_size = window_size;
        _min_length = min_length;
        _neg_k_freq = neg_k_freq;
        _neg_k_negactoin = neg_k_negactoin;
        _table_size = table_size;

        // 初始设置随机数种子
        std::srand(std::time(nullptr));
        TIME_BEGIN;
        // 初始化频率负采样辅助table
        init_unigram_table(mp_product_name_freq);
        debug_log("time cost init_unigram_table:" << TIME_END);
    }
    // 频率采样辅助table
    void init_unigram_table(const std::unordered_map<std::string, float>& mp_product_name_freq) {
        // 先将频率分布归一化
        float sum = 0.0;
        for (const auto& p : mp_product_name_freq) {
            sum += p.second;
        }
        _mp_product_name_freq.clear();
        for (const auto& p : mp_product_name_freq) {
            _mp_product_name_freq[p.first] = p.second / sum;
        }
        unigram_table.clear();
        unigram_table.resize(0);
        if (_mp_product_name_freq.empty()) {
            return;
        }
        for (auto& p: _mp_product_name_freq) {
            vocab_table.emplace_back(p.first);
        }
        for (size_t i = 0; i < vocab_table.size(); i ++) {
            _mp_product_name_index_of_vocab_table[vocab_table[i]] = i;
        }

        std::vector<float> freq_table(vocab_table.size());
        for (size_t i = 0; i < vocab_table.size(); i ++) {
            freq_table[i] = _mp_product_name_freq[vocab_table[i]];
        }

        unigram_table.resize(_table_size);
        size_t i = 0;
        double d1 = freq_table[i];
        for (size_t a = 0; a < _table_size; a++) {
            unigram_table[a] = i;
            // std::cout << "a=" << a << ",i=" << i << ",d1=" << d1 << std::endl;
            if ((a + 1) / (double)_table_size > d1) {
                i++;
                d1 += freq_table[i];
            }
            if (i >= vocab_table.size()) {
                i = vocab_table.size() - 1;
            }
        }
        // std::cout << "unigram_table:" << std::endl;
        // for (size_t i = 0; i < unigram_table.size(); i ++) {
        //     std::cout << unigram_table[i] << " ";
        // }
        // std::cout << std::endl;
    }

    // 采样出一个item
    std::string sample_item() {
        int n = unigram_table.size();
        if (n <= 0) {
            return "";
        }
        int r = std::rand();
        int i = (r % n + n ) % n;
        int a = unigram_table[i];
        return vocab_table[a];
    }

    // 采样出一个元素
    template<class T>
    T sample_from_vector(const std::vector<T>& v) {
        size_t n = v.size();
        if (n <= 0) {
            T t;
            return t;
        }
        int r = std::rand();
        int i = (r % n + n ) % n;
        debug_log("sample_from_vector:" << v[i] );
        return v[i];
    }

    std::vector<SeqPointwiseSample> transform(const SeqRawData& raw_data) {
        ::Json::Reader reader;
        ::Json::Value value;
        if (!reader.parse(raw_data.user_long_history_json, value)) {
            return {};
        }
        if (value.type() != ::Json::arrayValue) {
            return {};
        }
        int history_length = value.size();
        std::map<int64_t, ActionItem> mp_action_item;
        // std::map<int64_t, ActionItem> mp_action_item_p3s; // 额外用于构造p3s负样本
        // 按timestamp合并
        for (int i = 0; i < history_length; i ++) {
            ::Json::Value user_action = value[i];
            std::string product_name = user_action["product_name"].asString();
            std::string action_type = user_action["action_type"].asString();
            int64_t delivery_timestamp = user_action["delivery_timestamp"].asInt64();
            int label_ped = 0;
            int label_click = 0;
            int label_conv = 0;
            // 默认是负样本，遇到下列情况是正样本
            if (action_type == "AD_PHOTO_PLAYED_END") {
                label_ped = 1;
            } else if (action_type == "AD_ITEM_CLICK") {
                label_click = 1;
            } else if (_event_conv_set.count(action_type)) {
                label_conv = 1;
            } 
            
            ActionItem action_item;
            action_item.product_name = product_name;
            action_item.product_name_id = hash_func(product_name);
            action_item.delivery_timestamp = delivery_timestamp;
            action_item.label_ped = label_ped;
            action_item.label_click = label_click;
            action_item.label_conv = label_conv;

            // if (action_type == "AD_PHOTO_PLAYED_3S") {
            //     mp_action_item_p3s[delivery_timestamp] = action_item;
            // }

            auto it = mp_action_item.find(delivery_timestamp);
            if (it != mp_action_item.end()) {
                ActionItem& ref_action_item = it->second;
                ref_action_item.label_ped = label_ped;
                ref_action_item.label_click = label_click;
                ref_action_item.label_conv = label_conv;
            } else {
                mp_action_item[action_item.delivery_timestamp] = action_item;
            }
        }
        debug_log("mp_action_item.size():" << mp_action_item.size());
        // debug_log("mp_action_item_p3s.size():" << mp_action_item_p3s.size());

        // 选出label去为1的action的action
        std::vector<ActionItem> action_pos_item_list;
        std::vector<ActionItem> action_neg_item_list;
        for (auto& p : mp_action_item) {
            bool label_positive = (p.second.label_ped > 0) || (p.second.label_click > 0) || (p.second.label_conv > 0);
            if (label_positive) {
                action_pos_item_list.emplace_back(p.second);
            } else {
                action_neg_item_list.emplace_back(p.second);
            }
        }

        // 按timestamp排序
        std::sort(action_pos_item_list.begin(), action_pos_item_list.end(), [](const ActionItem& a1, const ActionItem& a2){
            return a1.delivery_timestamp < a2.delivery_timestamp;
        });
        debug_log("action_pos_item_list.size():" << action_pos_item_list.size());
        std::sort(action_neg_item_list.begin(), action_neg_item_list.end(), [](const ActionItem& a1, const ActionItem& a2){
            return a1.delivery_timestamp < a2.delivery_timestamp;
        });
        debug_log("action_neg_item_list.size():" << action_neg_item_list.size());

        // 滑动窗口构造正、负样本
        std::vector<SeqPointwiseSample> result_sample_list;
        for (size_t i = 0; i < action_pos_item_list.size(); i ++) {
            // 处理开头的短序列
            if (i < _min_length) { // 之后可以调整参数-最小用户特征历史序列长度
                continue;
            }

            // 准备一个pointwise sample对象
            SeqPointwiseSample seq_sample;
            seq_sample.user_id = raw_data.user_id;

            // 填入user_history_list
            for (size_t j = i - _window_size; j < i; j ++) {
                if (j >= 0) {
                    seq_sample.user_history_list.emplace_back(action_pos_item_list[j]);
                }
            }
            // debug_log("user_history_list.size():" << user_history_list.size());

            // 正样本填入 user_target_list
            ActionItem next_pos_item = action_pos_item_list[i];
            next_pos_item.generate_strategy = "nextpos";
            seq_sample.user_target_list.emplace_back(next_pos_item);

            // 按频率负采样填入 user_target_list
            for (size_t k = 0; k < _neg_k_freq; k ++) {
                std::string sampled_product_name = sample_item(); // 采样出一个product
                if (sampled_product_name.empty()) {
                    continue;
                }
                ActionItem sampled_action_item; // label默认都是0
                sampled_action_item.generate_strategy = "freq";
                sampled_action_item.product_name = sampled_product_name;
                sampled_action_item.product_name_id = hash_func(sampled_product_name);
                seq_sample.user_target_list.emplace_back(sampled_action_item);
            }

            // 按action的负采样填入 user_target_list
            for (size_t k = 0; k < _neg_k_negactoin; k ++) {
                ActionItem sampled_action_item = sample_from_vector(action_neg_item_list); // 采样出一个product
                if (sampled_action_item.product_name.empty()) {
                    continue;
                }
                sampled_action_item.generate_strategy = "negaction";
                seq_sample.user_target_list.emplace_back(sampled_action_item);
            }
            
            result_sample_list.emplace_back(seq_sample);
        }
        debug_log("result_sample_list.size():" << result_sample_list.size());
    
        return result_sample_list;
    }
    
    int64_t hash_func(const std::string& s){
        return std::hash<std::string>{}(s) % 1000000007;
    }
};


#ifdef __SEQ_DATASET_CONSTRUCTOR_DEBUG
#undef __SEQ_DATASET_CONSTRUCTOR_DEBUG
#endif

}
}