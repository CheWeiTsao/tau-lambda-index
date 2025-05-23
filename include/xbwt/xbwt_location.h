#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <set>
#include <stack>
#include <sdsl/wavelet_trees.hpp>
#include <sdsl/rrr_vector.hpp>
#include <sdsl/bit_vectors.hpp>
#include <sdsl/bp_support_sada.hpp>
#include <sdsl/int_vector.hpp>
#include <cstddef>
#include <memory>
#include "../symbol_table/symbol_bucket_offsets.h"
#include "../symbol_table/symbol_table.h"
#include <sdsl/qsufsort.hpp>
#include <chrono>

class XBWT_location
{
public:
    typedef std::pair<size_t, size_t> xbwt_range; // [l, r)

private:
    sdsl::wt_rlmn
    <
        sdsl::sd_vector<>,
        typename sdsl::sd_vector<>::rank_1_type,
        typename sdsl::sd_vector<>::select_1_type,
        sdsl::wt_ap<>
    > L;
    sdsl::bit_vector last;
    sdsl::bit_vector::rank_1_type last_rank;
    sdsl::bit_vector::select_1_type last_select;
    SymbolBucketOffsets C;
    uint64_t alphabet_size;
    uint64_t t_offset = static_cast<uint64_t>(1); // terminate symbols

    sdsl::int_vector<> locations, locations_offset, locations_length, next_leaf_rank; // leaves (rank only consider t_symbol)
    sdsl::int_vector<> leftest_child_rank, rightest_child_rank; // internal nodes ('1' rank in Last)
    SymbolTable symbol_table_;

    bool DownwardNavigation(xbwt_range& L_range, size_t c) const; // return found or not
    bool UpwardNavigation(size_t& l, size_t& r) const; // return found or not
    std::pair<size_t, size_t> pi_range_to_L_range(xbwt_range& pi_range) const; // pi range [l, r) to L/last range [l, r)
    std::pair<size_t, size_t> L_range_to_pi_range(xbwt_range& L_range, size_t c) const; // L/last range [l, r) to pi range [l, r) (starting with 'c')
    inline bool IsNotEmptyRange (xbwt_range const &range) const {
        return (std::get<0>(range) < std::get<1>(range));
    }

    void dfs(xbwt_range L_range, size_t &prev_leaf_rank);

public:
    XBWT_location();
    ~XBWT_location(){};

    void insert(sdsl::int_vector<> text);
    uint64_t match(sdsl::int_vector<>::iterator text_begin, sdsl::int_vector<>::iterator text_end);
    uint64_t nodeHeight(size_t l, size_t r);
    void Serialize (std::ostream &out);
    void Load (std::istream &in);

    std::vector<size_t> getC() {
        std::vector<size_t> ls;
        for (size_t i = 0; i < alphabet_size; i++)
            ls.push_back(C[i]);
        return ls;
    }
    std::vector<bool> getLast() {
        std::vector<bool> ls;
        for (auto i : last) ls.push_back(i);
        return ls;
    }
    std::vector<size_t> getL() {
        std::vector<size_t> ls;
        for (auto i : L) ls.push_back(i);
        return ls;
    }
    uint64_t getGrammarNumber() { return L.rank(L.size(), 1); }
    uint16_t getAlphabetCnt(uint64_t c) { return C[c+1] - C[c]; }
    void getPiPath(size_t i, sdsl::int_vector<>& path) const; // node-to-root path for the i-th '$'
    friend std::ostream& operator<< (std::ostream &out, XBWT_location const &xbwt);
    void insert_locations(std::vector<std::vector<size_t>> &locations_tmp, size_t locations_cnt);
    void locate(sdsl::int_vector<8> pattern, std::vector<uint64_t> &results, const size_t mf_offset);
};

void XBWT_location::locate(sdsl::int_vector<8> pattern, std::vector<uint64_t> &results, const size_t mf_offset) {
    if (pattern.size() == 0) {
        results.resize(locations.size());
        for (size_t i = 0, e = locations.size(); i < e; i++) { results[i] = locations[i] - mf_offset; }
    } else {
        xbwt_range L_range = {0, last_select(1) + 1};
        size_t c;
        for (size_t i = 0, e = pattern.size(); i < e; i++) {
            c = symbol_table_[pattern[i]];
            if (!DownwardNavigation(L_range, c)) { return; }
        }
        size_t node_idx = last_rank(std::get<1>(L_range)) - 1; // shifting to 0-index
        size_t cur_leaf_idx = leftest_child_rank[node_idx], end_leaf_idx = rightest_child_rank[node_idx];
        size_t offset, length;
        while (cur_leaf_idx != end_leaf_idx) {
            offset = locations_offset[cur_leaf_idx], length = locations_length[cur_leaf_idx];
            for (size_t i = 0; i < length; i++) { results.push_back(locations[offset++] - mf_offset); }
            cur_leaf_idx = next_leaf_rank[cur_leaf_idx];
        }
        offset = locations_offset[cur_leaf_idx], length = locations_length[cur_leaf_idx];
        for (size_t i = 0; i < length; i++) { results.push_back(locations[offset++] - mf_offset); }
    }
}

/// @brief This function aims to do two things: (1) record the idx of next (right) leaf for each leaf (2) record the leaf idx range [start, end] for each internal nodes
/// @param L_range range for an internal node
/// @param prev_leaf_rank the idx (t_offset rank) of the last visiting leaf
void XBWT_location::dfs(xbwt_range L_range, size_t &prev_leaf_rank) {
    size_t leaf_rank = L.rank(std::get<1>(L_range), t_offset) - 1; // shifting to 0-index
    size_t node_idx = last_rank(std::get<1>(L_range)); // shifting to 0-index
    for (size_t i = std::get<0>(L_range); i < std::get<1>(L_range); i++) {
        size_t c = L[i];
        if (c == t_offset) {
            next_leaf_rank[prev_leaf_rank] = leaf_rank;
            prev_leaf_rank = leaf_rank;
            leftest_child_rank[node_idx] = leaf_rank;
            rightest_child_rank[node_idx] = leaf_rank;
        } else {
            xbwt_range cur_range = L_range;
            DownwardNavigation(cur_range, c);
            dfs(cur_range, prev_leaf_rank);
            size_t child_idx = last_rank(std::get<1>(cur_range));
            if (std::get<0>(L_range) > 0) { // current node is node the root
                if (i == std::get<0>(L_range)) { leftest_child_rank[node_idx] = leftest_child_rank[child_idx]; }
                rightest_child_rank[node_idx] = rightest_child_rank[child_idx];
            }
        }
    }
}

void XBWT_location::insert_locations(std::vector<std::vector<size_t>> &locations_tmp, size_t locations_cnt) {
    locations_offset.resize(locations_tmp.size());
    locations_length.resize(locations_tmp.size());
    next_leaf_rank.resize(locations_tmp.size());
    locations.resize(locations_cnt);
    for (size_t i = 0, j = 0, k = 0, cnt = 0, end = locations_tmp.size(); i < end; i++, j++) {
        locations_offset[j] = cnt;
        locations_length[j] = locations_tmp[i].size();
        cnt += locations_tmp[i].size();
        for (auto loc : locations_tmp[i]) { locations[k++] = loc; }
    }

    leftest_child_rank.resize(last_rank(last.size()));
    rightest_child_rank.resize(last_rank(last.size()));
    xbwt_range L_range {0, last_select(1) + 1};
    size_t prev_leaf_rank = 0;
    // dfs(L_range, prev_leaf_rank, 0);
    dfs(L_range, prev_leaf_rank);
    next_leaf_rank[prev_leaf_rank] = L.rank(L.size(), t_offset); // TODO: necessary?

    sdsl::util::bit_compress(locations_offset);
    sdsl::util::bit_compress(locations_length);
    sdsl::util::bit_compress(locations);
    sdsl::util::bit_compress(next_leaf_rank);
    sdsl::util::bit_compress(leftest_child_rank);
    sdsl::util::bit_compress(rightest_child_rank);
}

XBWT_location::XBWT_location() {}

void XBWT_location::insert(sdsl::int_vector<> text) {
    // appending '0' to text is necessary for the s.sort(), however we don't want '0' appearing in xbwt
    // hence we will skip 'the 1st element in sa' and 'the last element in text'
    if (text[text.size()-1] != 0) { sdsl::append_zero_symbol(text); }
    symbol_table_ = decltype(symbol_table_)(text);
    for (size_t i = 0; i < text.size(); i++) { text[i] = symbol_table_[text[i]]; }
    alphabet_size = symbol_table_.GetAlphabetSize();
    sdsl::int_vector<> sa;
    sdsl::qsufsort::construct_sa(sa, text);

    // MR, modified Kasai alg.
    std::vector<size_t> RCP;
    std::vector<bool> MR;
    size_t len = text.size() - 1; // negelect the '0'
    sdsl::int_vector<> inv_sa(len);
    MR.assign(len, false);
    RCP.assign(len, 0);
    MR[0] = 1;
    for (size_t i = 0; i < len; i++) inv_sa[sa[i+1]] = i; // +1 for skipping the '0'
    for (size_t i = 0, lcp = 0, rcp = 0; i < len-1; ++i) {
        int j = sa[inv_sa[i] - 1 + 1]; // -1 for the previous position; +1 for skipping the '0'
        while (i + lcp < len && j + lcp < len && text[i + lcp] == text[j + lcp])
            ++lcp;
        while (i + rcp < len && j + rcp < len && text[i + rcp] != t_offset && text[i + rcp] == text[j + rcp])
            ++rcp;
        MR[inv_sa[i]] = lcp <= rcp;
        RCP[inv_sa[i]] = rcp;
        if (lcp > 0) --lcp;
        if (rcp > 0) --rcp;
    }

    std::vector<uint64_t> L_vector;
    std::vector<bool> last_vector;
    sdsl::int_vector<> C_vector;
    MR.push_back(1);
    for (size_t i = 0, prev = 0; i <= len; i++) {
        if (MR[i] == 1) {
            // L and Last
            std::set<uint64_t> st;
            for (size_t j = prev; j < i; j++) {
                size_t idx = sa[j + 1];  // +1 for skipping the '0'
                st.insert(text[idx > 0 ? idx-1 : len-1]); // BWT
            }
            for (auto itr = st.begin(), e = --st.end(); itr != st.end(); itr++) {
                L_vector.push_back(*itr);
                last_vector.push_back(itr == e);
            }
            prev = i;

        }
    }
    MR.pop_back();

    // to succint data structure
    last.resize(last_vector.size());
    for (size_t i = 0; i < last_vector.size(); i++) last[i] = last_vector[i];
    last_rank = decltype(last_rank)(&last);
    last_select = decltype(last_select)(&last);

    sdsl::int_vector<> L_buffer;
    L_buffer.width(symbol_table_.GetEffectiveAlphabetWidth());
    L_buffer.resize(L_vector.size());
    for (size_t i = 0; i < L_vector.size(); i++) L_buffer[i] = L_vector[i];
    sdsl::construct_im(L, L_buffer);
    
    C_vector.resize(alphabet_size + 1); // +1 for counting the # of last alphabet
    for (size_t i = 1, nodeCnt = 1; i <= alphabet_size; i++) {
        size_t cnt = L.rank(L.size(), i-1);
        nodeCnt += cnt;
        C_vector[i] = nodeCnt;
    }
    C = decltype(C)(C_vector);
}

void XBWT_location::Serialize(std::ostream &out) {
    sdsl::write_member(alphabet_size, out);
    L.serialize(out);
    last.serialize(out);
    last_rank.serialize(out);
    last_select.serialize(out);
    C.Serialize(out);
    locations_offset.serialize(out);
    locations_length.serialize(out);
    locations.serialize(out);
    next_leaf_rank.serialize(out);
    leftest_child_rank.serialize(out);
    rightest_child_rank.serialize(out);
    symbol_table_.Serialize(out);
}

void XBWT_location::Load(std::istream &in) {
    sdsl::read_member(alphabet_size, in);
    L.load(in);
    last.load(in);
    last_rank.load(in, &last);
    last_select.load(in, &last);
    C.Load(in);
    locations_offset.load(in);
    locations_length.load(in);
    locations.load(in);
    next_leaf_rank.load(in);
    leftest_child_rank.load(in);
    rightest_child_rank.load(in);
    symbol_table_.Load(in);
}

// match [begin, ..., end)
uint64_t XBWT_location::match(sdsl::int_vector<>::iterator text_begin, sdsl::int_vector<>::iterator text_end) {
    if (last_rank(last_rank.size()) == 0) return 0; // empty
    xbwt_range L_range = {0, last_select(1) + 1};
    while (text_begin != text_end) {
        size_t c = symbol_table_[*text_begin++]; ;
        if (!DownwardNavigation(L_range, c)) { return 0; }
    }
    size_t grammar_below_l = L.rank(std::get<0>(L_range), t_offset);
    size_t grammar_below_r = L.rank(std::get<1>(L_range), t_offset);
    if (grammar_below_l == grammar_below_r) { return 0; } // no grammar within [l, r)
    else { return grammar_below_r; }
}

bool XBWT_location::DownwardNavigation(xbwt_range& L_range, size_t c) const{
    xbwt_range pi_range = L_range_to_pi_range(L_range, c);
    if (!IsNotEmptyRange(pi_range)) { return false; }
    L_range = pi_range_to_L_range(pi_range);
    return true;
}

bool XBWT_location::UpwardNavigation(size_t& l, size_t& r) const{
    if (l == 0) return false; // root (empty string)
    size_t nodeCnt = last_rank(r) - 1; // -1 for 0-index 
    // size_t c = C.getSymbol(nodeCnt + 1 + C[2] - 1); // +1 for rank (rightmost excluded)
    size_t c = C.getSymbol(nodeCnt+ C[2]);
    //size_t c_cnt = nodeCnt - (C[c] - C[2] + 1) + 1; // 0-index
    size_t c_cnt = nodeCnt - C[c] + C[2];
    if (last_rank(L.select(c_cnt, c)) == 0) { l = 0; }
    else { l = last_select(last_rank(L.select(c_cnt, c))) + 1; }
    // r = last_select(last_rank(L.select(cnt, c)) + 1) + 1;
    r = last_select(last_rank(l) + 1) + 1;
    return true;
}

uint64_t XBWT_location::nodeHeight(size_t l, size_t r) {
    uint64_t height = 0;
    while (UpwardNavigation(l, r)) { height++; }
    return height;
}

void XBWT_location::getPiPath(size_t i, sdsl::int_vector<>& path) const {
    if (i == 0 || i > L.rank(L.size(), t_offset)) { return; }
    std::vector<uint64_t> tmp;
    size_t l = last_select(last_rank(L.select(i, t_offset))) + 1;
    size_t r = last_select(last_rank(l) + 1) + 1;
    while (l > 0) {
        tmp.push_back(C.getSymbol(last_rank(r) - 1 + 1 + C[2] - 1));
        UpwardNavigation(l, r);
    }
    path.resize(tmp.size());
    for (size_t i = 0; i < tmp.size(); i++) {
        path[i] = tmp[i];
    }
}

std::ostream& operator<< (std::ostream &out, XBWT_location const &xbwt) {
    size_t n = xbwt.last.size();
    out << "idx\tlast\tL\tpi\n";
    for (size_t i = 0; i < n; i++) {
        out << i << "\t";
        out << xbwt.last[i] << "\t";
        out << xbwt.L[i] << "\t";
        if (i > 0 && xbwt.last[i-1] == 1) {
            size_t l = xbwt.last_select(xbwt.last_rank(i)) + 1;
            size_t r = xbwt.last_select(xbwt.last_rank(l) + 1) + 1;
            while (l > 0) {
                out << xbwt.C.getSymbol(xbwt.last_rank(r) - 1 + 1 + xbwt.C[2] - 1) << " ";
                xbwt.UpwardNavigation(l, r);
            }
        }
        out << std::endl;
    }
    return out;
}

std::pair<size_t, size_t> XBWT_location::pi_range_to_L_range(xbwt_range& pi_range) const {
    // size_t L_left = last_select(std::get<0>(pi_range)) + 1, L_right = last_select(std::get<1>(pi_range)) + 1;
    return {
        last_select(std::get<0>(pi_range)) + 1,
        last_select(std::get<1>(pi_range)) + 1
    };
}

std::pair<size_t, size_t> XBWT_location::L_range_to_pi_range(xbwt_range& L_range, size_t c) const {
    // -(C[2]-1) shifting for skipping the C[1]('$'); (L.rank(l,c)+1) to count the i-th c in pi; -1 for the 0-index in pi
    // size_t pi_left = C[c] - (C[2] - 1) + (L.rank(l, c) + 1) - 1;
    // size_t pi_left = C[c] - C[2] + L.rank(std::get<0>(L_range), c) + 1, pi_right = C[c] - C[2] + L.rank(std::get<1>(L_range), c) + 1;
    return {
        C[c] - C[2] + L.rank(std::get<0>(L_range), c) + 1,
        C[c] - C[2] + L.rank(std::get<1>(L_range), c) + 1
    };
}