#pragma once

#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>

struct ProductFeat {
    std::string product_name;
    uint64_t first_id_v3 = 0;
    uint64_t second_id_v3 = 0;
    std::vector<std::string> word_cut;
    std::vector<std::string> character_cut;
    std::vector<std::string> similar_pdct;
    std::vector<std::string> similar_pdct_mmu;
    float conv_freq;
    std::string toString() {
        std::stringstream ss;
        ss << product_name << ","
            << first_id_v3 << ","
            << second_id_v3 << ","
            << word_cut.size() << ":" << (word_cut.size() ? word_cut[0] : "") << ","
            << character_cut.size() << ":" << (character_cut.size() ? character_cut[0] : "") << ","
            << similar_pdct.size() << ":" << (similar_pdct.size() ? similar_pdct[0] : "") << ","
            << similar_pdct_mmu.size() << ":" << (similar_pdct_mmu.size() ? similar_pdct_mmu[0] : "") << ","
            << conv_freq;
        return ss.str();
    }
};
