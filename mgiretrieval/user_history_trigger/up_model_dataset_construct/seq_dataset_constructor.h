#pragma once

#include <stdexcept>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <cstdlib> // std::rand()
#include <ctime>
#include <json/json.h>
#include "base/strings/string_split.h"

namespace ks {
namespace ad_algorithm {

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
    std::string photo_id;   // 将 uint64 转成 photo
    int64_t product_name_id;
    int64_t delivery_timestamp;
    int64_t label_ped;
    int64_t label_click;
    int64_t label_conv;
    int64_t label_form;
    int64_t label_invk;
    int64_t label_pay;
    int64_t label_next;
    int64_t label_inapp;
    int64_t label = 0;
    
    std::string generate_strategy; // 产生的策略，如"sample_from_neg_action" "sample_from_freq"
    ActionItem()
        : product_name_id(0), delivery_timestamp(0), label_ped(0), label_click(0), label_conv(0), label_form(0), label_invk(0), label_pay(0), label_next(0), label_inapp(0), label(0)
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
    size_t _inf_seq_len;
    int ped_wgt = 1;
    int click_wgt = 1;
    int conv_wgt = 3;
    int form_wgt = 3;
    int invk_wgt = 2;
    int pay_wgt = 4;
    int next_wgt = 4;
    int inapp_wgt = 3;
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
            size_t neg_k_freq = 5, size_t table_size = 1e7, size_t neg_k_negactoin = 5, size_t inf_seq_len = 40, std::string up_action_weight="ped:1,click:1,conversion:3,form_submit:3,app_invoke:2,pay:4,next:4,inapp:3") {
        _window_size = window_size;
        _min_length = min_length;
        _neg_k_freq = neg_k_freq;
        _neg_k_negactoin = neg_k_negactoin;
        _table_size = table_size;
        _inf_seq_len = inf_seq_len;
        std::vector<std::string> results_weight(0);
        base::SplitString(up_action_weight, ",", &results_weight);
        for (int i = 0; i < results_weight.size(); ++i) {
            std::string curr = results_weight[i];
            std::vector<std::string> parts(0);
            base::SplitString(curr, ":", &parts);
            if (parts[0] == "ped") {
                ped_wgt = stoi(parts[1]);
            } else if (parts[0] == "click") {
                click_wgt = stoi(parts[1]);
            } else if (parts[0] == "conversion") {
                conv_wgt = stoi(parts[1]);
            } else if (parts[0] == "form_submit") {
                form_wgt = stoi(parts[1]);
            } else if (parts[0] == "app_invoke") {
                invk_wgt = stoi(parts[1]);
            } else if (parts[0] == "pay") {
                pay_wgt = stoi(parts[1]);
            } else if (parts[0] == "next") {
                next_wgt = stoi(parts[1]);
            } else if (parts[0] == "inapp") {
                inapp_wgt = stoi(parts[1]);
            }
        }

        // 初始设置随机数种子
        std::srand(std::time(nullptr));
        TIME_BEGIN;
        // 初始化频率负采样辅助table
        init_unigram_table(mp_product_name_freq);
        // debug_log("time cost init_unigram_table:" << TIME_END);
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
        // debug_log("sample_from_vector:" << v[i] );
        return v[i];
    }

    int calculate_seq_len(const SeqRawData& raw_data) {
        ::Json::Reader reader;
        ::Json::Value value;
        if (!reader.parse(raw_data.user_long_history_json, value)) {
            return 0;
        }
        if (value.type() != ::Json::arrayValue) {
            LOG(INFO) << "value.type() != ::Json::arrayValue";
            return 0;
        }

        int history_length = value.size();
        std::map<int64_t, ActionItem> mp_action_item;
        std::map<std::string, int> mp_actiontype_cnt;
        // 按timestamp合并
        for (int i = 0; i < history_length; i++) {
            ::Json::Value user_action = value[i];
            std::string product_name = user_action["product_name"].asString();
            std::string action_type = user_action["action_type"].asString();
            int64_t delivery_timestamp = user_action["delivery_timestamp"].asInt64();
            if (mp_actiontype_cnt.count(action_type)) {
                mp_actiontype_cnt[action_type] += 1;
            } else {
                mp_actiontype_cnt[action_type] = 1;
            }
            int label_ped = 0;
            int label_click = 0;
            int label_conv = 0;
            int label_form = 0;
            int label_invk = 0;
            int label_pay = 0;
            int label_next = 0;
            int label_inapp = 0;
            
            // 默认是负样本，遇到下列情况是正样本
            if (action_type == "AD_PHOTO_PLAYED_END") {
                label_ped = ped_wgt;
            } else if (action_type == "AD_ITEM_CLICK") {
                label_click = click_wgt;
            } else if (action_type == "EVENT_CONVERSION") {
                label_conv = conv_wgt;
            } else if (action_type == "EVENT_FORM_SUBMIT") {
                label_form = form_wgt;
            } else if (action_type == "EVENT_APP_INVOKED") {
                label_invk = invk_wgt;
            } else if (action_type == "EVENT_PAY") {
                label_pay = pay_wgt;
            } else if (action_type == "EVENT_NEXTDAY_STAY") {
                label_next = next_wgt;
            } else if (action_type == "EVENT_KEY_INAPP_ACTION") {
                label_inapp = inapp_wgt;
            }
            
            ActionItem action_item;
            action_item.product_name = product_name;
            action_item.product_name_id = hash_func(product_name);
            action_item.delivery_timestamp = delivery_timestamp;
            action_item.label_ped = label_ped;
            action_item.label_click = label_click;
            action_item.label_conv = label_conv;
            action_item.label_form = label_form;
            action_item.label_invk = label_invk;
            action_item.label_pay = label_pay;
            action_item.label_next = label_next;
            action_item.label_inapp = label_inapp;

            auto it = mp_action_item.find(delivery_timestamp);
            if (it != mp_action_item.end()) {
                ActionItem& ref_action_item = it->second;
                ref_action_item.label_ped = ref_action_item.label_ped || label_ped;
                ref_action_item.label_click = ref_action_item.label_click || label_click;
                ref_action_item.label_conv = ref_action_item.label_conv || label_conv;
                ref_action_item.label_form = ref_action_item.label_form || label_form;
                ref_action_item.label_invk = ref_action_item.label_invk || label_invk;
                ref_action_item.label_pay = ref_action_item.label_pay || label_pay;
                ref_action_item.label_next = ref_action_item.label_next || label_next;
                ref_action_item.label_inapp = ref_action_item.label_inapp || label_inapp;
            } else {
                mp_action_item[action_item.delivery_timestamp] = action_item;
            }
        }

        // 选出 label 为1的 action 的action
        std::vector<ActionItem> action_pos_item_list;
        std::vector<ActionItem> action_neg_item_list;
        for (auto& p : mp_action_item) {
            bool label_positive = (p.second.label_ped > 0) || (p.second.label_click > 0)
                               || (p.second.label_conv > 0) || (p.second.label_form > 0)
                               || (p.second.label_invk > 0) || (p.second.label_pay > 0)
                               || (p.second.label_next > 0) || (p.second.label_inapp > 0);
            if (label_positive) {
                action_pos_item_list.emplace_back(p.second);
            } else {
                action_neg_item_list.emplace_back(p.second);
            }
        }
        return action_pos_item_list.size();
    }

    std::string calc_delitime_pdctnum(const SeqRawData& raw_data) {
        std::string rst = "";
        ::Json::Reader reader;
        ::Json::Value value;
        if (!reader.parse(raw_data.user_long_history_json, value)) {
            return {};
        }
        if (value.type() != ::Json::arrayValue) {
            return {};
        }
        int history_length = value.size();
        std::map<int64_t, std::unordered_set<std::string>> mp_delitime_pdct;
        for (int i = 0; i < history_length; i++) {
            ::Json::Value user_action = value[i];
            std::string product_name = user_action["product_name"].asString();
            int64_t delivery_timestamp = user_action["delivery_timestamp"].asInt64();
            auto it = mp_delitime_pdct.find(delivery_timestamp);
            if (it != mp_delitime_pdct.end()) {
                mp_delitime_pdct[delivery_timestamp].insert(product_name);
            } else {
                std::unordered_set<std::string> curr_set;
                curr_set.insert(product_name);
                mp_delitime_pdct[delivery_timestamp] = curr_set;
            }
        }
       
        for (auto& ele : mp_delitime_pdct) {
            int num = ele.second.size();
            if (rst.size() > 0) {
                rst += ",";
            }
            rst += std::to_string(num);
            // debug 用
            if (num > 4) {
                std::string deli_pdct_names = "";
                for (auto& pdct : ele.second) {
                    deli_pdct_names += ",";
                    deli_pdct_names += pdct;
                }
                LOG(INFO) << "user_id:" << raw_data.user_id << ", delivery_timestamp:" << ele.first << ", deli_pdct_names:" << deli_pdct_names;
            }
        }

        return rst;
    }

    string compressString(std::vector<uint64_t> S) {
        string ans = "";
        if ((int)S.size() == 0) return ans; // 空串处理
        
        int cnt = 1;
        uint64_t ch = S[0];
        for (int i = 1; i < (int)S.size(); ++i){
            if (ch == S[i]) cnt++;
            else{
                ans += (std::to_string(ch) + ":" + std::to_string(cnt) + ","); // 注意 cnt 要转为字符串
                ch = S[i];
                cnt = 1;
            }
        }
        ans += (std::to_string(ch) + ":" + std::to_string(cnt) + ",");
        return ans;
    }

    string compressString(std::vector<std::string> S) {
        string ans = "";
        if ((int)S.size() == 0) return ans; // 空串处理
        
        int cnt = 1;
        std::string ch = S[0];
        std::replace(ch.begin(), ch.end(), ':', '@');
        for (int i = 1; i < (int)S.size(); ++i){
            if (ch == S[i]) cnt++;
            else{
                ans += (ch + ":" + std::to_string(cnt) + ","); // 注意 cnt 要转为字符串
                ch = S[i];
                std::replace(ch.begin(), ch.end(), ':', '@');
                cnt = 1;
            }
        }
        ans += (ch + ":" + std::to_string(cnt) + ",");
        return ans;
    }

    std::vector<std::vector<std::string>> analyse_user_action(const SeqRawData& raw_data, std::string user_id) {
        /*
            1. 指定 app_type + interactive_form 的行为特点
            2. 指定 sub_page_id 的行为特点
            3. 指定 ad_style 的行为特点
            4. 指定 product_name 的行为特点
            5. 指定 first_industry_name 的行为特点
            6. 指定 second_industry_name 的行为特点
        */
        std::vector<std::vector<std::string>> rst;

        ::Json::Reader reader;
        ::Json::Value value;
        if (!reader.parse(raw_data.user_long_history_json, value)) {
            return rst;
        }
        if (value.type() != ::Json::arrayValue) {
            return rst;
        }

        int history_length = value.size();
        // LOG(INFO) << "ljp-debug, history_length:" << history_length;

        // 【统计】用户各种 action_type 的数量（creative_id 粒度）、各种 action_type 的 creative_id/photo_id 重复度
        std::unordered_map<std::string, uint64_t> action_number_mp;
        std::unordered_map<std::string, std::pair<std::vector<uint64_t>, std::set<uint64_t>>> action_pids;
        std::unordered_map<std::string, std::pair<std::vector<uint64_t>, std::set<uint64_t>>> action_cids;
        std::unordered_map<std::string, std::pair<std::vector<std::string>, std::set<std::string>>> action_pdcts;
        // 【统计】重复曝光情况：去重 photo_id 数量 / 曝光 photo_id 数量，去重 creative_id 数量 / 曝光 creative_id 数量

        // 【统计】发生转化的情况，转化定义：ocpc_action_type = action_type（仅能覆盖浅度、不能覆盖深度）、不同 ocpc_action_type 发生转化的数量
        // 【统计】转化发生之前曝光数量、首次曝光到发生转化经历的时间间隔，记录每个产品给当前用户首次曝光的时间、发生转化的时间，进而能够计算出转化时间间隔
        std::unordered_map<std::string, uint64_t> ocpc_conv_number_mp;
        std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> pdct_imp_conv_timegap_mp;
        std::unordered_map<std::string, std::pair<uint64_t, bool>> pdct_imp_num_bf_cv_mp;
        uint64_t conv_number = 0;
        uint64_t cv_product_num = 0;
        // 索引越小，事件发生时间越近
        for (int i = history_length-1; i >= 0; i--) {
            ::Json::Value user_action = value[i];
            std::string product_name = user_action["product_name"].asString();
            std::string action_type = user_action["action_type"].asString();
            std::string app_type = user_action["app_type"].asString();
            std::string ocpc_action_type = user_action["ocpc_action_type"].asString();
            std::string first_industry_name = user_action["first_industry_name"].asString();
            std::string second_industry_name = user_action["second_industry_name"].asString();
            uint64_t interactive_form = user_action["interactive_form"].asUInt64();
            uint64_t sub_page_id = user_action["sub_page_id"].asUInt64();
            uint64_t photo_id = user_action["photo_id"].asUInt64();
            uint64_t creative_id = user_action["creative_id"].asUInt64();
            // unix 时间戳：从1970年1月1日（UTC/GMT的午夜）开始所经过的[毫]秒数
            uint64_t delivery_timestamp = user_action["delivery_timestamp"].asUInt64();     // 单位：毫秒
            uint64_t event_server_timestamp = user_action["event_server_timestamp"].asUInt64();     // 单位：毫秒
            uint64_t llsid = user_action["llsid"].asUInt64();

            // 过滤不关心的 app_type/interactive_form 等，仅统计感兴趣的 sub_page_id 的情况
            // sub_page_id in ('10002001', '10002005', '100012194','10011001','11001001','100013365')
            // if (!(app_type == choose_app_type && interactive_form == 2)) {
            //     continue;
            // }

            // 非流化的行为则过滤
            if (!(sub_page_id == 10002001 || sub_page_id == 10002005 || sub_page_id == 100012194
               || sub_page_id == 10011001 || sub_page_id == 11001001 || sub_page_id == 100013365)) {
                continue;
            }

            // 过滤 timestamp = 0 的 item，未成功打上时间戳的 item 不考虑
            if (delivery_timestamp == 0 || event_server_timestamp == 0) {
                continue;
            }

            // 统计各种 action_type 的 creative_id 数量
            if (action_number_mp.count(action_type)) {
                action_number_mp[action_type] += 1;
            } else {
                action_number_mp[action_type] = 0;
            }

            // creative_id
            if (action_cids.count(action_type)) {
                action_cids[action_type].first.push_back(creative_id);
                action_cids[action_type].second.insert(creative_id);
            } else {
                std::pair<std::vector<uint64_t>, std::set<uint64_t>> curr_cids_pair;
                curr_cids_pair.first.push_back(creative_id);
                curr_cids_pair.second.insert(creative_id);
                action_cids[action_type] = curr_cids_pair;
            }

            // photo_id
            if (action_pids.count(action_type)) {
                action_pids[action_type].first.push_back(photo_id);
                action_pids[action_type].second.insert(photo_id);
            } else {
                std::pair<std::vector<uint64_t>, std::set<uint64_t>> curr_pids_pair;
                curr_pids_pair.first.push_back(photo_id);
                curr_pids_pair.second.insert(photo_id);
                action_pids[action_type] = curr_pids_pair;
            }

            // product_name
            if (action_pdcts.count(action_type)) {
                action_pdcts[action_type].first.push_back(product_name);
                action_pdcts[action_type].second.insert(product_name);
            } else {
                std::pair<std::vector<std::string>, std::set<std::string>> curr_pdcts_pair;
                curr_pdcts_pair.first.push_back(product_name);
                curr_pdcts_pair.second.insert(product_name);
                action_pdcts[action_type] = curr_pdcts_pair;
            }

            // 【统计】重复曝光情况：去重 photo_id 数量 / 曝光 photo_id 数量，去重 creative_id 数量 / 曝光 creative_id 数量
            if (action_type == "AD_PHOTO_PLAYED_3S") {
                if (product_name.length() > 0 && !pdct_imp_conv_timegap_mp.count(product_name)) {
                    // 没有发生转化时，转化时间默认设置为0
                    pdct_imp_conv_timegap_mp[product_name] = {event_server_timestamp, 0};
                }

                if (product_name.length() > 0) {
                    if (!pdct_imp_num_bf_cv_mp.count(product_name)) {
                        // false: 表示还未发生转化
                        pdct_imp_num_bf_cv_mp[product_name] = {1, false};
                    } else {
                        if (!pdct_imp_num_bf_cv_mp[product_name].second) {
                            pdct_imp_num_bf_cv_mp[product_name].first += 1;
                        }
                    }
                } 
            }

            // 记录发生转化的情况
            if ((ocpc_action_type == "AD_CONVERSION" && action_type == "EVENT_CONVERSION")
            || (ocpc_action_type == "AD_LANDING_PAGE_FORM_SUBMITTED" && action_type == "EVENT_FORM_SUBMIT")
            || (ocpc_action_type == "AD_PURCHASE" && action_type == "EVENT_PAY")
            || (ocpc_action_type == "EVENT_ORDER_SUBMIT" && action_type == "EVENT_ORDER_SUBMIT")
            || (ocpc_action_type == "AD_ITEM_CLICK" && action_type == "AD_ITEM_CLICK")
            || (ocpc_action_type == "EVENT_NEXTDAY_STAY" && action_type == "EVENT_NEXTDAY_STAY")
            || (ocpc_action_type == "EVENT_KEY_INAPP_ACTION" && action_type == "EVENT_KEY_INAPP_ACTION")
            || (ocpc_action_type == "EVENT_APP_INVOKED" && action_type == "EVENT_APP_INVOKED")
            || (ocpc_action_type == "EVENT_ORDER_PAIED" && action_type == "EVENT_ORDER_PAIED")
            || (ocpc_action_type == "AD_CREDIT_GRANT" && action_type == "EVENT_CREDIT_GRANT")
            || (ocpc_action_type == "EVENT_REGISTER" && action_type == "EVENT_REGISTER")) {
                if (pdct_imp_conv_timegap_mp.count(product_name) && pdct_imp_conv_timegap_mp[product_name].second == 0) {
                    pdct_imp_conv_timegap_mp[product_name].second = event_server_timestamp;
                    cv_product_num += 1;
                    pdct_imp_num_bf_cv_mp[product_name].second = true;  // 标识已发生转化，后续转化不再考虑
                }
                conv_number += 1;
                if (ocpc_conv_number_mp.count(ocpc_action_type)) {
                    ocpc_conv_number_mp[ocpc_action_type] += 1;
                } else {
                    ocpc_conv_number_mp[ocpc_action_type] = 0;
                }
            }
        }

        // 开始计算统计数据并进行返回
        
       uint64_t p3s_num = action_number_mp["AD_PHOTO_PLAYED_3S"];
       std::string stats_data = "";
       if (p3s_num > 0) {
            /*
                1. creative_id 粒度p3s转化率：转化数 / p3s数
                2. creative_id 粒度p3s->ped率：ped数 / p3s数
                3. creative_id 粒度p3s->click率：click数 / p3s数
                根据 p3s_num 分桶，绘制得到各种率的曲线
            */
            float p3s_conv_rate = conv_number * 1.0 / p3s_num;
            float p3s_ped_rate = action_number_mp.count("AD_PHOTO_PLAYED_END") ? action_number_mp["AD_PHOTO_PLAYED_END"] * 1.0 / p3s_num : 0;
            float p3s_click_rate = action_number_mp.count("AD_ITEM_CLICK") ? action_number_mp["AD_ITEM_CLICK"] * 1.0 / p3s_num : 0;

            stats_data += ("p3s_conv_rate:" + std::to_string(p3s_conv_rate) + ",");
            stats_data += ("p3s_ped_rate:" + std::to_string(p3s_ped_rate) + ",");
            stats_data += ("p3s_click_rate:" + std::to_string(p3s_click_rate) + ",");

            /*
                1. 各种行为的 photo_id 重复情况分析：根据 action_pids
                2. 各种行为的 creative_id 重复情况分析：action_cids
                3. 各种行为的 product_name 重复情况分析：action_pdcts
            */
            float p3s_pid_dup_degree = action_pids.count("AD_PHOTO_PLAYED_3S") ? action_pids["AD_PHOTO_PLAYED_3S"].first.size() * 1.0 / action_pids["AD_PHOTO_PLAYED_3S"].second.size() : -1;
            float ped_pid_dup_degree = action_pids.count("AD_PHOTO_PLAYED_END") ? action_pids["AD_PHOTO_PLAYED_END"].first.size() * 1.0 / action_pids["AD_PHOTO_PLAYED_END"].second.size() : -1;
            float clk_pid_dup_degree = action_pids.count("AD_ITEM_CLICK") ? action_pids["AD_ITEM_CLICK"].first.size() * 1.0 / action_pids["AD_ITEM_CLICK"].second.size() : -1;
            float conv_pid_dup_degree = action_pids.count("EVENT_CONVERSION") ? action_pids["EVENT_CONVERSION"].first.size() * 1.0 / action_pids["EVENT_CONVERSION"].second.size() : -1;
            float form_pid_dup_degree = action_pids.count("AD_LANDING_PAGE_FORM_SUBMITTED") ? action_pids["AD_LANDING_PAGE_FORM_SUBMITTED"].first.size() * 1.0 / action_pids["AD_LANDING_PAGE_FORM_SUBMITTED"].second.size() : -1;
            float invk_pid_dup_degree = action_pids.count("EVENT_APP_INVOKED") ? action_pids["EVENT_APP_INVOKED"].first.size() * 1.0 / action_pids["EVENT_APP_INVOKED"].second.size() : -1;
            stats_data += ("p3s_pid_dup_degree:" + std::to_string(p3s_pid_dup_degree) + ",");
            stats_data += ("ped_pid_dup_degree:" + std::to_string(ped_pid_dup_degree) + ",");
            stats_data += ("clk_pid_dup_degree:" + std::to_string(clk_pid_dup_degree) + ",");
            stats_data += ("conv_pid_dup_degree:" + std::to_string(conv_pid_dup_degree) + ",");
            stats_data += ("form_pid_dup_degree:" + std::to_string(form_pid_dup_degree) + ",");
            stats_data += ("invk_pid_dup_degree:" + std::to_string(invk_pid_dup_degree) + ",");

            float p3s_cid_dup_degree = action_cids.count("AD_PHOTO_PLAYED_3S") ? action_cids["AD_PHOTO_PLAYED_3S"].first.size() * 1.0 / action_cids["AD_PHOTO_PLAYED_3S"].second.size() : -1;
            float ped_cid_dup_degree = action_cids.count("AD_PHOTO_PLAYED_END") ? action_cids["AD_PHOTO_PLAYED_END"].first.size() * 1.0 / action_cids["AD_PHOTO_PLAYED_END"].second.size() : -1;
            float clk_cid_dup_degree = action_cids.count("AD_ITEM_CLICK") ? action_cids["AD_ITEM_CLICK"].first.size() * 1.0 / action_cids["AD_ITEM_CLICK"].second.size() : -1;
            float conv_cid_dup_degree = action_cids.count("EVENT_CONVERSION") ? action_cids["EVENT_CONVERSION"].first.size() * 1.0 / action_cids["EVENT_CONVERSION"].second.size() : -1;
            float form_cid_dup_degree = action_cids.count("AD_LANDING_PAGE_FORM_SUBMITTED") ? action_cids["AD_LANDING_PAGE_FORM_SUBMITTED"].first.size() * 1.0 / action_cids["AD_LANDING_PAGE_FORM_SUBMITTED"].second.size() : -1;
            float invk_cid_dup_degree = action_cids.count("EVENT_APP_INVOKED") ? action_cids["EVENT_APP_INVOKED"].first.size() * 1.0 / action_cids["EVENT_APP_INVOKED"].second.size() : -1;
            stats_data += ("p3s_cid_dup_degree:" + std::to_string(p3s_cid_dup_degree) + ",");
            stats_data += ("ped_cid_dup_degree:" + std::to_string(ped_cid_dup_degree) + ",");
            stats_data += ("clk_cid_dup_degree:" + std::to_string(clk_cid_dup_degree) + ",");
            stats_data += ("conv_cid_dup_degree:" + std::to_string(conv_cid_dup_degree) + ",");
            stats_data += ("form_cid_dup_degree:" + std::to_string(form_cid_dup_degree) + ",");
            stats_data += ("invk_cid_dup_degree:" + std::to_string(invk_cid_dup_degree) + ",");

            float p3s_pdct_dup_degree = action_pdcts.count("AD_PHOTO_PLAYED_3S") ? action_pdcts["AD_PHOTO_PLAYED_3S"].first.size() * 1.0 / action_pdcts["AD_PHOTO_PLAYED_3S"].second.size() : -1;
            float ped_pdct_dup_degree = action_pdcts.count("AD_PHOTO_PLAYED_END") ? action_pdcts["AD_PHOTO_PLAYED_END"].first.size() * 1.0 / action_pdcts["AD_PHOTO_PLAYED_END"].second.size() : -1;
            float clk_pdct_dup_degree = action_pdcts.count("AD_ITEM_CLICK") ? action_pdcts["AD_ITEM_CLICK"].first.size() * 1.0 / action_pdcts["AD_ITEM_CLICK"].second.size() : -1;
            float conv_pdct_dup_degree = action_pdcts.count("EVENT_CONVERSION") ? action_pdcts["EVENT_CONVERSION"].first.size() * 1.0 / action_pdcts["EVENT_CONVERSION"].second.size() : -1;
            float form_pdct_dup_degree = action_pdcts.count("AD_LANDING_PAGE_FORM_SUBMITTED") ? action_pdcts["AD_LANDING_PAGE_FORM_SUBMITTED"].first.size() * 1.0 / action_pdcts["AD_LANDING_PAGE_FORM_SUBMITTED"].second.size() : -1;
            float invk_pdct_dup_degree = action_pdcts.count("EVENT_APP_INVOKED") ? action_pdcts["EVENT_APP_INVOKED"].first.size() * 1.0 / action_pdcts["EVENT_APP_INVOKED"].second.size() : -1;
            stats_data += ("p3s_pdct_dup_degree:" + std::to_string(p3s_pdct_dup_degree) + ",");
            stats_data += ("ped_pdct_dup_degree:" + std::to_string(ped_pdct_dup_degree) + ",");
            stats_data += ("clk_pdct_dup_degree:" + std::to_string(clk_pdct_dup_degree) + ",");
            stats_data += ("conv_pdct_dup_degree:" + std::to_string(conv_pdct_dup_degree) + ",");
            stats_data += ("form_pdct_dup_degree:" + std::to_string(form_pdct_dup_degree) + ",");
            stats_data += ("invk_pdct_dup_degree:" + std::to_string(invk_pdct_dup_degree) + ",");

            /*
                1. 首次曝光到转化的时间间隔：遍历 pdct_imp_conv_timegap_mp，统计所有 product 的间隔
                2. 统计转化发生之前的曝光数量
            */
            uint64_t avg_pdct_cv_timegap = 0;   // 单位：秒
            for (auto iter = pdct_imp_conv_timegap_mp.begin(); iter != pdct_imp_conv_timegap_mp.end(); ++iter) {
                if (iter->second.second > 0) {
                    avg_pdct_cv_timegap += (iter->second.second - iter->second.first) / (1000.0 * cv_product_num);
                }
            }
            stats_data += ("avg_pdct_cv_timegap:" + std::to_string(avg_pdct_cv_timegap) + ",");
            uint64_t avg_pdct_cv_impnum = 0;    // 直接计算平均值，未进行加权
            for (auto iter = pdct_imp_num_bf_cv_mp.begin(); iter != pdct_imp_num_bf_cv_mp.end(); ++iter) {
                if (iter->second.second) {
                    avg_pdct_cv_impnum += iter->second.first / cv_product_num;
                }
            }
            stats_data += ("avg_pdct_cv_impnum:" + std::to_string(avg_pdct_cv_impnum) + ",");
            /*
                1. p3s, ped, click, conv, form, pay, invk photo 序列，暂不去重
                2. p3s, ped, click, conv, form, pay, invk product 序列，暂不去重
            */

            std::string p3s_pid_seq_str = "p3s_pid_seq_str_" + compressString(action_pids["AD_PHOTO_PLAYED_3S"].first);
            std::string click_pid_seq_str = "click_pid_seq_str_" + compressString(action_pids["AD_ITEM_CLICK"].first);
            std::string conv_pid_seq_str = "conv_pid_seq_str_" + compressString(action_pids["EVENT_CONVERSION"].first);
            std::string form_pid_seq_str = "form_pid_seq_str_" + compressString(action_pids["AD_LANDING_PAGE_FORM_SUBMITTED"].first);
            std::string pay_pid_seq_str = "pay_pid_seq_str_" + compressString(action_pids["EVENT_PAY"].first);
            std::string invk_pid_seq_str = "invk_pid_seq_str_" + compressString(action_pids["EVENT_APP_INVOKED"].first);

            std::string p3s_pdct_seq_str = "p3s_pdct_seq_str_" + compressString(action_pdcts["AD_PHOTO_PLAYED_3S"].first);
            std::string click_pdct_seq_str = "click_pdct_seq_str_" + compressString(action_pdcts["AD_ITEM_CLICK"].first);
            std::string conv_pdct_seq_str = "conv_pdct_seq_str_" + compressString(action_pdcts["EVENT_CONVERSION"].first);
            std::string form_pdct_seq_str = "form_pdct_seq_str_" + compressString(action_pdcts["AD_LANDING_PAGE_FORM_SUBMITTED"].first);
            std::string pay_pdct_seq_str = "pay_pdct_seq_str_" + compressString(action_pdcts["EVENT_PAY"].first);
            std::string invk_pdct_seq_str = "invk_pdct_seq_str_" + compressString(action_pdcts["EVENT_APP_INVOKED"].first);

            std::string conversion_number = "AD_CONVERSION:" + std::to_string(ocpc_conv_number_mp["AD_CONVERSION"]);
            std::string form_number = "AD_LANDING_PAGE_FORM_SUBMITTED:" + std::to_string(ocpc_conv_number_mp["AD_LANDING_PAGE_FORM_SUBMITTED"]);
            std::string pay_number = "AD_PURCHASE:" + std::to_string(ocpc_conv_number_mp["AD_PURCHASE"]);
            std::string invk_number = "EVENT_APP_INVOKED:" + std::to_string(ocpc_conv_number_mp["EVENT_APP_INVOKED"]);
            std::string order_number = "EVENT_ORDER_SUBMIT:" + std::to_string(ocpc_conv_number_mp["EVENT_ORDER_SUBMIT"]);

            rst.push_back({"user_id_"+user_id, p3s_pid_seq_str, click_pid_seq_str, conv_pid_seq_str, form_pid_seq_str, pay_pid_seq_str, invk_pid_seq_str, p3s_pdct_seq_str, click_pdct_seq_str, conv_pdct_seq_str, form_pdct_seq_str, form_pdct_seq_str, pay_pdct_seq_str, invk_pdct_seq_str, stats_data, "conv_number_"+std::to_string(conv_number), "p3s_num_"+std::to_string(p3s_num), conversion_number, form_number, pay_number, invk_number, order_number});
       }
        return rst;
    }

    bool produce_mp_action_item(const SeqRawData& raw_data, std::map<int64_t, std::unordered_map<std::string, ActionItem>> &mp_action_item, bool is_rm_click_ped, bool is_extract_photo) {
        ::Json::Reader reader;
        ::Json::Value value;
        if (!reader.parse(raw_data.user_long_history_json, value)) {
            return false;
        }
        if (value.type() != ::Json::arrayValue) {
            return false;
        }

        int history_length = value.size();
        // LOG(INFO) << "ljp-debug, history_length:" << history_length;
        // 按timestamp合并
        for (int i = 0; i < history_length; i++) {
            ::Json::Value user_action = value[i];
            std::string product_name = user_action["product_name"].asString();
            std::string action_type = user_action["action_type"].asString();
            uint64_t photo_id = user_action["photo_id"].asInt64();
            int64_t delivery_timestamp = user_action["delivery_timestamp"].asInt64();
            // 过滤 timestamp = 0 的 item
            if (delivery_timestamp == 0) {
                continue;
            }

            // 以上逻辑不太合理，用户可能出现多个行为
            // ped, ic, conv, form, invk, pay, next, inapp
            int label = 0;
            if (!is_rm_click_ped) {
                if (action_type == "AD_ITEM_CLICK") {
                    label |= 128;
                }
                if (action_type == "AD_PHOTO_PLAYED_END") {
                    label |= 64;
                }
            }

            if (action_type == "EVENT_KEY_INAPP_ACTION") {
                label |= 32;
            }
            if (action_type == "EVENT_CONVERSION") {
                label |= 16;
            }
            if (action_type == "EVENT_FORM_SUBMIT") {
                label |= 8;
            }
            if (action_type == "EVENT_APP_INVOKED") {
                label |= 4;
            }
            if (action_type == "EVENT_PAY") {
                label |= 2;
            }
            if (action_type == "EVENT_NEXTDAY_STAY") {
                label |= 1;
            }

            ActionItem action_item;
            action_item.product_name = product_name;
            action_item.photo_id = std::to_string(photo_id);
            action_item.product_name_id = hash_func(product_name);
            action_item.delivery_timestamp = delivery_timestamp;
            action_item.label = label;
            // LOG(INFO) << "ljp-debug, i: " << i << ", product_name:" << product_name
            //           << ", photo_id:" << photo_id << ", delivery_timestamp:" << delivery_timestamp;

            // 修复同 delivery_time 多个 pdct_name 的错误
            auto it = mp_action_item.find(delivery_timestamp);
            std::string key = "";
            if (is_extract_photo) {
                key = action_item.photo_id;
            } else {
                key = action_item.product_name;
            }
            if (it != mp_action_item.end()) {
                std::unordered_map<std::string, ActionItem>& ref_map = it->second;
                if (ref_map.count(key)) {
                    ActionItem& ref_action_item = ref_map[key];
                    ref_action_item.label = ref_action_item.label | label;
                } else {
                    ref_map[key] = action_item;
                }
            } else {
                std::unordered_map<std::string, ActionItem> action_item_mp;
                action_item_mp[key] = action_item;
                mp_action_item[delivery_timestamp] = action_item_mp;
            }
        }
        return true;
    }

    std::vector<std::vector<ActionItem>> produce_user_histpdct(const SeqRawData& raw_data, bool is_rm_click_ped, bool is_dedup, bool is_extract_photo = false) {
        // LOG(INFO) << "ljp-debug, is_rm_click_ped:" << is_rm_click_ped << ", is_dedup:" << is_dedup << ", is_extract_photo:" << is_extract_photo;
        std::map<int64_t, std::unordered_map<std::string, ActionItem>> mp_action_item;
        bool rt = produce_mp_action_item(raw_data, mp_action_item, is_rm_click_ped, is_extract_photo);

        std::vector<std::vector<ActionItem>> hist_pdct_by_act;
        if (rt) {
            // 选出 label 取值为 1 的 action
            // 修复同 delivery_time 多个 pdct_name 的错误
            std::vector<ActionItem> action_pos_item_list;
            std::vector<ActionItem> action_neg_item_list;
            for (auto& p : mp_action_item) {
                for (auto& act : p.second) {
                    if (act.second.label > 0) {
                        action_pos_item_list.emplace_back(act.second);
                    } else {
                        action_neg_item_list.emplace_back(act.second);
                    }
                }
            }
            // LOG(INFO) << "ljp-debug, action_pos_item_list.size():" << action_pos_item_list.size()
            //           << ", action_neg_item_list.size():" << action_neg_item_list.size();

            // 按 timestamp 排序
            if (is_extract_photo) {
                // 从小到大排序
                std::sort(action_pos_item_list.begin(), action_pos_item_list.end(), [](const ActionItem& a1, const ActionItem& a2){
                    return a1.delivery_timestamp < a2.delivery_timestamp;
                });
                std::sort(action_neg_item_list.begin(), action_neg_item_list.end(), [](const ActionItem& a1, const ActionItem& a2){
                    return a1.delivery_timestamp < a2.delivery_timestamp;
                });
            } else {
                // 从大到小排序
                std::sort(action_pos_item_list.begin(), action_pos_item_list.end(), [](const ActionItem& a1, const ActionItem& a2){
                    return a1.delivery_timestamp > a2.delivery_timestamp;
                });

                std::sort(action_neg_item_list.begin(), action_neg_item_list.end(), [](const ActionItem& a1, const ActionItem& a2){
                    return a1.delivery_timestamp > a2.delivery_timestamp;
                });
            }

            // 塞入 hist deep act pdct
            std::vector<ActionItem> hist_deep_grain;
            std::vector<ActionItem> hist_shallow_grain;
            if (is_dedup) {
                std::unordered_map<std::string, int> grain_count; // for dedup use
                int cnt = 0;
                for (int i = 0; i < action_pos_item_list.size() && cnt < _inf_seq_len; ++i) {
                    std::string key = "";
                    if (is_extract_photo) {
                        key = action_pos_item_list[i].photo_id;
                    } else {
                        key = action_pos_item_list[i].product_name;
                    }
                    if (grain_count.find(key) == grain_count.end()) {
                        hist_deep_grain.push_back(action_pos_item_list[i]);
                        grain_count.emplace(key, 1);
                        cnt++;
                    }
                }
                hist_pdct_by_act.push_back(hist_deep_grain);

                grain_count.clear();
                cnt = 0;
                for (int i = 0; i < action_neg_item_list.size() && cnt < _inf_seq_len; ++i) {
                    std::string key = "";
                    if (is_extract_photo) {
                        key = action_neg_item_list[i].photo_id;
                    } else {
                        key = action_neg_item_list[i].product_name;
                    }
                    if (grain_count.find(key) == grain_count.end()) {
                        hist_shallow_grain.push_back(action_neg_item_list[i]);
                        grain_count.emplace(key, 1);
                        cnt++;
                    }
                }
                hist_pdct_by_act.push_back(hist_shallow_grain);
            } else {
                int photo_count = action_pos_item_list.size() > _inf_seq_len? _inf_seq_len : action_pos_item_list.size();
                // LOG(INFO) << "ljp-debug, photo_count:" << photo_count
                //           << ", action_pos_item_list.size():" << action_pos_item_list.size()
                //           << ", _inf_seq_len:" << _inf_seq_len;
                hist_deep_grain.assign(action_pos_item_list.begin()+(action_pos_item_list.size()-photo_count),
                                       action_pos_item_list.end());
                // LOG(INFO) << "ljp-debug, 1, hist_deep_grain.size():" << hist_deep_grain.size();
                hist_pdct_by_act.push_back(hist_deep_grain);

                photo_count = action_neg_item_list.size() > _inf_seq_len? _inf_seq_len : action_neg_item_list.size();
                // LOG(INFO) << "ljp-debug, photo_count:" << photo_count;
                hist_shallow_grain.assign(action_neg_item_list.begin()+(action_neg_item_list.size()-photo_count),
                                       action_neg_item_list.end());
                // LOG(INFO) << "ljp-debug, 1, hist_shallow_grain.size():" << hist_shallow_grain.size();
                hist_pdct_by_act.push_back(hist_shallow_grain);
            }
            // LOG(INFO) << "ljp-debug, hist_deep_grain.size():" << hist_deep_grain.size()
                    //   << ", hist_shallow_grain.size():" << hist_shallow_grain.size();
        }
        
        return hist_pdct_by_act;
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
        std::map<int64_t, std::unordered_map<std::string, ActionItem>> mp_action_item;
        std::map<std::string, int> mp_actiontype_cnt;
        // std::map<int64_t, ActionItem> mp_action_item_p3s; // 额外用于构造p3s负样本
        // 按timestamp合并
        for (int i = 0; i < history_length; i++) {
            ::Json::Value user_action = value[i];
            std::string product_name = user_action["product_name"].asString();
            std::string action_type = user_action["action_type"].asString();
            int64_t delivery_timestamp = user_action["delivery_timestamp"].asInt64();
            // 过滤 timestamp = 0 的 item
            if (delivery_timestamp == 0) {
                continue;
            }
            if (mp_actiontype_cnt.count(action_type)) {
                mp_actiontype_cnt[action_type] += 1;
            } else {
                mp_actiontype_cnt[action_type] = 1;
            }
            int label_ped = 0;
            int label_click = 0;
            int label_conv = 0;
            int label_form = 0;
            int label_invk = 0;
            int label_pay = 0;
            int label_next = 0;
            int label_inapp = 0;
            
            // 默认是负样本，遇到下列情况是正样本
            if (action_type == "AD_PHOTO_PLAYED_END") {
                label_ped = ped_wgt;
            } else if (action_type == "AD_ITEM_CLICK") {
                label_click = click_wgt;
            } else if (action_type == "EVENT_CONVERSION") {
                label_conv = conv_wgt;
            } else if (action_type == "EVENT_FORM_SUBMIT") {
                label_form = form_wgt;
            } else if (action_type == "EVENT_APP_INVOKED") {
                label_invk = invk_wgt;
            } else if (action_type == "EVENT_PAY") {
                label_pay = pay_wgt;
            } else if (action_type == "EVENT_NEXTDAY_STAY") {
                label_next = next_wgt;
            } else if (action_type == "EVENT_KEY_INAPP_ACTION") {
                label_inapp = inapp_wgt;
            }
            
            ActionItem action_item;
            action_item.product_name = product_name;
            action_item.product_name_id = hash_func(product_name);
            action_item.delivery_timestamp = delivery_timestamp;
            action_item.label_ped = label_ped;
            action_item.label_click = label_click;
            action_item.label_conv = label_conv;
            action_item.label_form = label_form;
            action_item.label_invk = label_invk;
            action_item.label_pay = label_pay;
            action_item.label_next = label_next;
            action_item.label_inapp = label_inapp;

            // if (action_type == "AD_PHOTO_PLAYED_3S") {
            //     mp_action_item_p3s[delivery_timestamp] = action_item;
            // }

            // 修复同 delivery_time 多个 pdct_name 的错误
            auto it = mp_action_item.find(delivery_timestamp);
            if (it != mp_action_item.end()) {
                std::unordered_map<std::string, ActionItem>& ref_map = it->second;
                if (ref_map.count(action_item.product_name)) {
                    ActionItem& ref_action_item = ref_map[action_item.product_name];
                    ref_action_item.label_ped = ref_action_item.label_ped || label_ped;
                    ref_action_item.label_click = ref_action_item.label_click || label_click;
                    ref_action_item.label_conv = ref_action_item.label_conv || label_conv;
                    ref_action_item.label_form = ref_action_item.label_form || label_form;
                    ref_action_item.label_invk = ref_action_item.label_invk || label_invk;
                    ref_action_item.label_pay = ref_action_item.label_pay || label_pay;
                    ref_action_item.label_next = ref_action_item.label_next || label_next;
                    ref_action_item.label_inapp = ref_action_item.label_inapp || label_inapp;
                } else {
                    ref_map[action_item.product_name] = action_item;
                }
            } else {
                std::unordered_map<std::string, ActionItem> action_item_mp;
                action_item_mp[action_item.product_name] = action_item;
                mp_action_item[action_item.delivery_timestamp] = action_item_mp;
            }
        }
        // debug_log("mp_action_item.size():" << mp_action_item.size());
        // debug_log("mp_actiontype_cnt:" << "user_id:" << raw_data.user_id);
        #ifdef __SEQ_DATASET_CONSTRUCTOR_DEBUG
        {
            std::stringstream ss;
            for (auto& p : mp_actiontype_cnt) {
                ss << p.first << ":" << p.second << ",";
            }
            // debug_log("mp_actiontype_cnt:" << ss.str());
        }
        #endif
        
        // debug_log("mp_action_item_p3s.size():" << mp_action_item_p3s.size());

        // 选出label去为1的action的action
        // 修复同 delivery_time 多个 pdct_name 的错误
        std::vector<ActionItem> action_pos_item_list;
        std::vector<ActionItem> action_neg_item_list;
        for (auto& p : mp_action_item) {
            for (auto& act : p.second) {
                bool label_positive = (act.second.label_ped > 0) || (act.second.label_click > 0)
                               || (act.second.label_conv > 0) || (act.second.label_form > 0)
                               || (act.second.label_invk > 0) || (act.second.label_pay > 0)
                               || (act.second.label_next > 0) || (act.second.label_inapp > 0);
                if (label_positive) {
                    action_pos_item_list.emplace_back(act.second);
                } else {
                    action_neg_item_list.emplace_back(act.second);
                }
            }
        }

        // 按 timestamp 排序
        std::sort(action_pos_item_list.begin(), action_pos_item_list.end(), [](const ActionItem& a1, const ActionItem& a2){
            return a1.delivery_timestamp < a2.delivery_timestamp;
        });
        // debug_log("action_pos_item_list.size():" << action_pos_item_list.size());
        #ifdef __SEQ_DATASET_CONSTRUCTOR_DEBUG
        {
            std::stringstream ss;
            for (auto& p : action_pos_item_list) {
                ss << p << ",";
            }
            // debug_log("action_pos_item_list:" << ss.str());
        }
        #endif
        std::sort(action_neg_item_list.begin(), action_neg_item_list.end(), [](const ActionItem& a1, const ActionItem& a2){
            return a1.delivery_timestamp < a2.delivery_timestamp;
        });
        // debug_log("action_neg_item_list.size():" << action_neg_item_list.size());
        #ifdef __SEQ_DATASET_CONSTRUCTOR_DEBUG
        {
            std::stringstream ss;
            for (auto& p : action_neg_item_list) {
                ss << p << ",";
            }
            // debug_log("action_neg_item_list:" << ss.str());
        }
        #endif

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
        // debug_log("result_sample_list.size():" << result_sample_list.size());
    
        return result_sample_list;
    }
    
    int64_t hash_func(const std::string& s){
        return std::hash<std::string>{}(s) % 1000000007;
    }
};


class SeqPointwiseDatasetValidConstructor {
private:
    std::set<std::string> _event_conv_set;
    size_t _valid_seq_len;
    size_t _valid_neg_num;
    std::unordered_map<std::string, float> _mp_product_name_freq;
    std::unordered_map<std::string, int> _mp_product_name_index_of_vocab_table;
    std::vector<std::string> vocab_table;
    std::vector<int> unigram_table;
public:
    SeqPointwiseDatasetValidConstructor() {
        _event_conv_set.insert("EVENT_CONVERSION");
        _event_conv_set.insert("EVENT_FORM_SUBMIT");
        _event_conv_set.insert("EVENT_APP_INVOKED");
        _event_conv_set.insert("EVENT_PAY");
        _event_conv_set.insert("EVENT_NEXTDAY_STAY");
        _event_conv_set.insert("EVENT_KEY_INAPP_ACTION");
    }
    void init(const std::unordered_map<std::string, float>& mp_product_name_freq, size_t valid_seq_len=5, size_t valid_neg_num=100) {
        _mp_product_name_freq = mp_product_name_freq;
        _valid_seq_len = valid_seq_len;
        _valid_neg_num = valid_neg_num;
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
        std::map<std::string, int> mp_actiontype_cnt;
        std::set<std::string> set_product;

        // 按 timestamp 合并
        for (int i = 0; i < history_length; i++) {
            ::Json::Value user_action = value[i];
            std::string product_name = user_action["product_name"].asString();
            std::string action_type = user_action["action_type"].asString();
            int64_t delivery_timestamp = user_action["delivery_timestamp"].asInt64();
            if (mp_actiontype_cnt.count(action_type)) {
                mp_actiontype_cnt[action_type] += 1;
            } else {
                mp_actiontype_cnt[action_type] = 1;
            }
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

            auto it = mp_action_item.find(delivery_timestamp);
            if (it != mp_action_item.end()) {
                ActionItem& ref_action_item = it->second;
                ref_action_item.label_ped = ref_action_item.label_ped || label_ped;
                ref_action_item.label_click = ref_action_item.label_click || label_click;
                ref_action_item.label_conv = ref_action_item.label_conv || label_conv;
            } else {
                mp_action_item[action_item.delivery_timestamp] = action_item;
            }
        }

        #ifdef __SEQ_DATASET_CONSTRUCTOR_DEBUG
        {
            std::stringstream ss;
            for (auto& p : mp_actiontype_cnt) {
                ss << p.first << ":" << p.second << ",";
            }
        }
        #endif

        // 选出 label 为 1 的 action 的 action
        std::vector<ActionItem> action_pos_item_list;
        std::vector<ActionItem> action_neg_item_list;
        for (auto& p : mp_action_item) {
            bool label_positive = (p.second.label_ped > 0) || (p.second.label_click > 0) || (p.second.label_conv > 0);
            if (label_positive) {
                action_pos_item_list.emplace_back(p.second);
                set_product.insert(p.second.product_name);
            } else {
                action_neg_item_list.emplace_back(p.second);
            }
        }

        // 按timestamp排序
        std::sort(action_pos_item_list.begin(), action_pos_item_list.end(), [](const ActionItem& a1, const ActionItem& a2){
            return a1.delivery_timestamp < a2.delivery_timestamp;
        });

        #ifdef __SEQ_DATASET_CONSTRUCTOR_DEBUG
        {
            std::stringstream ss;
            for (auto& p : action_pos_item_list) {
                ss << p << ",";
            }
        }
        #endif

        std::sort(action_neg_item_list.begin(), action_neg_item_list.end(), [](const ActionItem& a1, const ActionItem& a2){
            return a1.delivery_timestamp < a2.delivery_timestamp;
        });

        #ifdef __SEQ_DATASET_CONSTRUCTOR_DEBUG
        {
            std::stringstream ss;
            for (auto& p : action_neg_item_list) {
                ss << p << ",";
            }
        }
        #endif

        std::vector<SeqPointwiseSample> result_sample_list;
        // valid 
        // 1. 仅仅使用最近行为的若干 pdct
        // 2. 所有转化的 pdct 均作为正样本
        SeqPointwiseSample seq_sample;
        seq_sample.user_id = raw_data.user_id;
        if (action_pos_item_list.size() >= _valid_seq_len) {
            for (size_t i = action_pos_item_list.size()-_valid_seq_len; i < action_pos_item_list.size(); i++) {
                seq_sample.user_history_list.emplace_back(action_pos_item_list[i]);
            }
        }

        // 挑选一个 item 组成完整样本
        for (auto kv : _mp_product_name_freq) {
            std::string product_name = kv.first;
            if (set_product.count(product_name) == 0) {
                ActionItem sampled_action_item; // label默认都是0
                sampled_action_item.generate_strategy = "freq";
                sampled_action_item.product_name = product_name;
                sampled_action_item.product_name_id = hash_func(product_name);
                seq_sample.user_target_list.emplace_back(sampled_action_item);
                break;
            }
        }
            
        result_sample_list.emplace_back(seq_sample);
        // debug_log("result_sample_list.size():" << result_sample_list.size());
    
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