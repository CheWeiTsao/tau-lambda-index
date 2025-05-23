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

class XBWT
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
    sdsl::bit_vector P_bv;  // 1 for '(', 0 for ')'
    sdsl::bp_support_sada<> P;
    sdsl::bit_vector last;
    sdsl::bit_vector::rank_1_type last_rank;
    sdsl::bit_vector::select_1_type last_select;
    SymbolBucketOffsets C;
    uint64_t alphabet_size;
    uint64_t t_offset = static_cast<uint64_t>(1); // terminate symbols
    sdsl::sd_vector<> singleCharRunCnt_bits;
    sdsl::sd_vector<>::select_1_type singleCharRunCnt;
    sdsl::sd_vector<> subFactorCnt_bits;
    sdsl::sd_vector<>::select_1_type subFactorCnt;

    bool DownwardNavigation(xbwt_range& L_range, size_t c) const; // return found or not
    bool UpwardNavigation(size_t& l, size_t& r) const; // return found or not
    std::pair<size_t, size_t> pi_range_to_L_range(xbwt_range& pi_range) const; // pi range [l, r) to L/last range [l, r)
    std::pair<size_t, size_t> L_range_to_pi_range(xbwt_range& L_range, size_t c) const; // L/last range [l, r) to pi range [l, r) (starting with 'c')
    inline bool IsNotEmptyRange (xbwt_range const &range) const {
        return (std::get<0>(range) < std::get<1>(range));
    }

public:
    XBWT();
    ~XBWT(){};

    void insert(sdsl::int_vector<>& text, uint64_t effective_alphabet_width, uint64_t effective_alphabet_size);
    uint64_t match(sdsl::int_vector<>::iterator text_begin, sdsl::int_vector<>::iterator text_end);
    uint64_t nodeHeight(size_t l, size_t r);
    std::pair<size_t, size_t> match_pos_in_pattern(const sdsl::int_vector<>& pattern);
    void match_pos_in_pattern(const sdsl::int_vector<>& pattern, size_t &offset, size_t &length, size_t &rank);
    // void match_pos_in_pattern(const sdsl::int_vector<>& pattern, size_t &offset, size_t &length, size_t &rank);
    bool match_if_exist(const sdsl::int_vector<>& pattern);
    void failureLink(size_t& l, size_t& r);
    void Serialize (std::ostream &out);
    void Load (std::istream &in);

    size_t getP_size() {return P.size();}
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
    template <typename Iterator, typename Range>
    void suffixRangeQuery(Iterator rbegin, Iterator rend, Range &grammar_range, SymbolTable& runLength_symbol_table, uint64_t maxRunLength);
    template <typename Iterator, typename Range>
    void prefixRangeQuery(Iterator begin, Iterator end, Range &grammar_range, SymbolTable& runLength_symbol_table, uint64_t maxRunLength);
    template <typename Iterator>
    uint64_t infixMatch(Iterator begin, Iterator end); // assume the input is in lex-order
    void gen_subFactorCnt(sdsl::int_vector<> &lex_sl_rl_int_text, SymbolTable &symbol_table, uint64_t maxRunLength);
    template <typename Iterator>
    uint64_t subFactorSuffixRangeQuery(Iterator rbegin, Iterator rend, SymbolTable& runLength_symbol_table, uint64_t maxRunLength);
    void singleCharacterRunQuery(uint64_t rl_int_symbol, SymbolTable& runLength_symbol_table, std::pair<uint64_t, uint64_t> &result, uint64_t maxRunLength); // {cnt, len}
    friend std::ostream& operator<< (std::ostream &out, XBWT const &xbwt);
};

XBWT::XBWT() {}

void XBWT::failureLink(size_t& l, size_t& r) {
    /*
    if (i == 0) return 0; // empty string
    size_t k = P.select(i+1);
    size_t j = P.enclose(k);
    size_t l = P.rank(j)-1;
    */
    if (l == 0) return; // empty string
    size_t k = P.select(last_rank(l) + 1);
    size_t j = P.enclose(k);
    size_t tmp = P.rank(j);
    if (tmp - 1 == 0) { l = 0; }
    else { l = last_select(tmp - 1) + 1; }
    r = last_select(last_rank(l) + 1) + 1;
}

void XBWT::insert(sdsl::int_vector<>& text, uint64_t effective_alphabet_width, uint64_t effective_alphabet_size) {
    std::chrono::steady_clock::time_point t1, t2;
    t1 = std::chrono::steady_clock::now();

    alphabet_size = effective_alphabet_size;
    // appending '0' to text is necessary for the s.sort(), however we don't want '0' appearing in xbwt
    // hence we will skip 'the 1st element in sa' and 'the last element in text'
    if (text[text.size()-1] != 0) { sdsl::append_zero_symbol(text); }
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

    // Construct L, Last, and P from MR

    std::vector<uint64_t> L_vector;
    std::vector<bool> last_vector, P_vector;
    sdsl::int_vector<> C_vector;
    std::stack<std::pair<size_t, size_t>> stk;
    MR.push_back(1);
    for (size_t i = 0, prev = 0; i <= len; i++) {
        if (MR[i] == 1) {
            // L and Last
            std::set<uint64_t> st;
            for (size_t j = prev; j < i; j++) {
                size_t idx = sa[j + 1];  // +1 for skipping the '0'
                st.insert(text[idx > 0 ? idx-1 : len-1]); // BWT
            }
            if (!st.empty()) {
                for (auto itr = st.begin(), e = --st.end(); itr != st.end(); itr++) {
                    L_vector.push_back(*itr);
                    last_vector.push_back(itr == e);
                }
            }
            prev = i;

            // P
            if (i < len) {
                size_t l = RCP[i];
                // size > 1 for the first element is empty string (less than 1)
                // which should be the prefix of all substrings (always in stack)
                while (stk.size() > 1 && stk.top().second >= l) { 
                    P_vector.push_back(0);
                    stk.pop();
                }
                if (stk.size() > 1 && text[sa[stk.top().first + 1] + l] != t_offset) {
                    l = stk.top().second;
                    P_vector.push_back(0);
                    stk.pop();
                }
                P_vector.push_back(1);
                stk.push({i, l});
            }
        }
    }
    while (!stk.empty()){
        P_vector.push_back(0);
        stk.pop();
    }
    MR.pop_back();

    // to succint data structure
    last.resize(last_vector.size());
    for (size_t i = 0; i < last_vector.size(); i++) last[i] = last_vector[i];
    last_rank = decltype(last_rank)(&last);
    last_select = decltype(last_select)(&last);

    sdsl::int_vector<> L_buffer;
    L_buffer.width(effective_alphabet_width);
    L_buffer.resize(L_vector.size());
    for (size_t i = 0; i < L_vector.size(); i++) L_buffer[i] = L_vector[i];
    sdsl::construct_im(L, L_buffer);

    sdsl::bit_vector bv_tmp (P_vector.size(), 0);
    for (size_t i = 0; i < P_vector.size(); i++) bv_tmp[i] = P_vector[i];
    P_bv = bv_tmp;
    sdsl::bp_support_sada<> tmp(&P_bv);
    P = tmp;

    C_vector.resize(alphabet_size + 1); // +1 for counting the # of last alphabet
    for (size_t i = 1, nodeCnt = 1; i <= alphabet_size; i++) {
        size_t cnt = L.rank(L.size(), i-1);
        nodeCnt += cnt;
        C_vector[i] = nodeCnt;
    }
    C = decltype(C)(C_vector);
}

void XBWT::gen_subFactorCnt(sdsl::int_vector<> &lex_sl_rl_int_text, SymbolTable &symbol_table, uint64_t maxRunLength) {
    sdsl::int_vector<> subFactorCnt_bits_tmp(last_rank(last.size()), 0);
    sdsl::int_vector<> singleCharRunCnt_bits_tmp(last_rank(last.size()), 0);
    auto rbegin {std::prev(lex_sl_rl_int_text.end(), 3)}; // skipping the last '1''0'
    auto rend {std::prev(lex_sl_rl_int_text.begin())};
    auto rfirst {rbegin};
    auto rlast {rfirst};
    while (rlast != rend) {
        while (rlast != rend && *rlast != t_offset) { rlast = std::prev(rlast); }
        auto it {rfirst};
        // match function from right to left
        xbwt_range L_range = {0, last_select(1) + 1};
        while (it != rlast) {
            size_t c = *it--;
            if (!DownwardNavigation(L_range, c)) {
                throw std::runtime_error("matching in gen_subFactorCnt() should never fail");
            }
            subFactorCnt_bits_tmp[last_rank(std::get<1>(L_range)) - 1] += 1; // -1 for 0-index
            singleCharRunCnt_bits_tmp[last_rank(std::get<1>(L_range)) - 1] += 1; // -1 for 0-index
        }
        if (rlast != rend) {
            rlast = std::prev(rlast);
            rfirst = rlast;
        }
    }
    for (size_t i = 2; i < alphabet_size; i++) {
        size_t mul = symbol_table.get_run_length(i, maxRunLength);
        size_t begin = C[i] - C[2] + 1, end = C[i+1] - C[2] + 1;
        for (size_t j = begin; j < end; j++) {
            singleCharRunCnt_bits_tmp[j] = mul * singleCharRunCnt_bits_tmp[j];
        }
    }
    for (size_t i = 1; i < subFactorCnt_bits_tmp.size(); i++) {
        subFactorCnt_bits_tmp[i] += subFactorCnt_bits_tmp[i-1];
        singleCharRunCnt_bits_tmp[i] += singleCharRunCnt_bits_tmp[i-1];
    }
    subFactorCnt_bits = decltype(subFactorCnt_bits)(std::begin(subFactorCnt_bits_tmp), std::end(subFactorCnt_bits_tmp));
    subFactorCnt.set_vector(&subFactorCnt_bits);
    singleCharRunCnt_bits = decltype(singleCharRunCnt_bits)(std::begin(singleCharRunCnt_bits_tmp), std::end(singleCharRunCnt_bits_tmp));
    singleCharRunCnt.set_vector(&singleCharRunCnt_bits);
}

void XBWT::Serialize(std::ostream &out) {
    sdsl::write_member(alphabet_size, out);
    L.serialize(out);
    P_bv.serialize(out);
    P.serialize(out);
    last.serialize(out);
    last_rank.serialize(out);
    last_select.serialize(out);
    C.Serialize(out);
    subFactorCnt_bits.serialize(out);
    subFactorCnt.serialize(out);
    singleCharRunCnt_bits.serialize(out);
    singleCharRunCnt.serialize(out);
}

void XBWT::Load(std::istream &in) {
    sdsl::read_member(alphabet_size, in);
    L.load(in);
    P_bv.load(in);
    P.load(in, &P_bv);
    last.load(in);
    last_rank.load(in, &last);
    last_select.load(in, &last);
    C.Load(in);
    subFactorCnt_bits.load(in);
    subFactorCnt.load(in, &subFactorCnt_bits);
    singleCharRunCnt_bits.load(in);
    singleCharRunCnt.load(in, &singleCharRunCnt_bits);
}

// match [begin, ..., end)
uint64_t XBWT::match(sdsl::int_vector<>::iterator text_begin, sdsl::int_vector<>::iterator text_end) {
    if (last_rank(last_rank.size()) == 0) return 0; // empty
    xbwt_range L_range = {0, last_select(1) + 1};
    while (text_begin != text_end) {
        size_t c = *text_begin++;
        if (!DownwardNavigation(L_range, c)) { return 0; }
    }
    size_t grammar_below_l = L.rank(std::get<0>(L_range), t_offset);
    size_t grammar_below_r = L.rank(std::get<1>(L_range), t_offset);
    if (grammar_below_l == grammar_below_r) { return 0; } // no grammar within [l, r)
    else { return grammar_below_r; }
}

bool XBWT::DownwardNavigation(xbwt_range& L_range, size_t c) const{
    xbwt_range pi_range = L_range_to_pi_range(L_range, c);
    if (!IsNotEmptyRange(pi_range)) { return false; }
    L_range = pi_range_to_L_range(pi_range);
    return true;
}

bool XBWT::UpwardNavigation(size_t& l, size_t& r) const{
    if (l == 0) return false; // root (empty string)
    size_t nodeCnt = last_rank(r) - 1; // -1 for 0-index 
    size_t c = C.getSymbol(nodeCnt+ C[2]);
    size_t c_cnt = nodeCnt - C[c] + C[2];
    if (last_rank(L.select(c_cnt, c)) == 0) { l = 0; }
    else { l = last_select(last_rank(L.select(c_cnt, c))) + 1; }
    r = last_select(last_rank(l) + 1) + 1;
    return true;
}

uint64_t XBWT::nodeHeight(size_t l, size_t r) {
    uint64_t height = 0;
    while (UpwardNavigation(l, r)) { height++; }
    return height;
}

void XBWT::getPiPath(size_t i, sdsl::int_vector<>& path) const {
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

void XBWT::singleCharacterRunQuery(uint64_t rl_int_symbol, SymbolTable& runLength_symbol_table, std::pair<uint64_t, uint64_t> &result, uint64_t maxRunLength) {
    uint64_t l_rl_int_symbol = rl_int_symbol, r_rl_int_symbol = runLength_symbol_table.max_of_this_alphabet(l_rl_int_symbol, maxRunLength); // [l, r)
    xbwt_range pi_range = {
        C[l_rl_int_symbol] - C[2] + 1,
        C[r_rl_int_symbol] - C[2] + 1
    };
    result = {
        singleCharRunCnt(std::get<1>(pi_range)) - singleCharRunCnt(std::get<0>(pi_range)),
        subFactorCnt(std::get<1>(pi_range)) - subFactorCnt(std::get<0>(pi_range))
    };
}

template <typename Iterator>
uint64_t XBWT::subFactorSuffixRangeQuery(Iterator rbegin, Iterator rend, SymbolTable& runLength_symbol_table, uint64_t maxRunLength) {
    uint64_t l_rl_int_symbol = *rbegin, r_rl_int_symbol = runLength_symbol_table.max_of_this_alphabet(l_rl_int_symbol, maxRunLength); // [l, r)
    xbwt_range L_range = {
        last_select(C[l_rl_int_symbol] - C[2] + 1) + 1,
        last_select(C[r_rl_int_symbol] - C[2] + 1) + 1
    };
    size_t cnt = 0;
    rend = std::next(rend); // range-query until the last character run
    auto it {std::prev(rbegin)};
    while (it != rend) {
        if (!DownwardNavigation(L_range, *it)) { return cnt; }
        it = std::prev(it);
    }

    l_rl_int_symbol = *rend, r_rl_int_symbol = runLength_symbol_table.max_of_this_alphabet(l_rl_int_symbol, maxRunLength); // [l, r)
    for (size_t rl_int_symbol = l_rl_int_symbol; rl_int_symbol < r_rl_int_symbol; rl_int_symbol++) {
        xbwt_range tmp_L_range {L_range};
        xbwt_range pi_range = L_range_to_pi_range(tmp_L_range, rl_int_symbol);
        if (!IsNotEmptyRange(pi_range)) { continue; }
        cnt += subFactorCnt(std::get<1>(pi_range)) - subFactorCnt(std::get<0>(pi_range));
    }
    return cnt;
}

template <typename Iterator, typename Range>
void XBWT::suffixRangeQuery(Iterator rbegin, Iterator rend, Range &grammar_range, SymbolTable& runLength_symbol_table, uint64_t maxRunLength) {
    uint64_t l_rl_int_symbol = *rbegin, r_rl_int_symbol = runLength_symbol_table.max_of_this_alphabet(l_rl_int_symbol, maxRunLength); // [l, r)
    xbwt_range L_range = {
        last_select(C[l_rl_int_symbol] - C[2] + 1) + 1,
        last_select(C[r_rl_int_symbol] - C[2] + 1) + 1
    };
    auto it {std::prev(rbegin)};
    while (it != rend) {
        if (!DownwardNavigation(L_range, *it)) { 
            grammar_range = {1, 0}; 
            return;
        }
        it = std::prev(it);
    }
    grammar_range = {L.rank(std::get<0>(L_range), t_offset) + 1, L.rank(std::get<1>(L_range), t_offset)};
    if (std::get<0>(grammar_range) > std::get<1>(grammar_range)) {
        grammar_range = {1, 0};
    }
    return;
}

template <typename Iterator, typename Range>
void XBWT::prefixRangeQuery(Iterator begin, Iterator end, Range &grammar_range, SymbolTable& runLength_symbol_table, uint64_t maxRunLength) {
    uint64_t l_rl_int_symbol = *begin, r_rl_int_symbol = runLength_symbol_table.max_of_this_alphabet(l_rl_int_symbol, maxRunLength); // [l, r)
    xbwt_range L_range = {
        last_select(C[l_rl_int_symbol] - C[2] + 1) + 1,
        last_select(C[r_rl_int_symbol] - C[2] + 1) + 1
    };
    auto it {std::next(begin)};
    while (it != end) {
        if (!DownwardNavigation(L_range, *it)) { 
            grammar_range = {1, 0}; 
            return;
        }
        it = std::next(it);
    }
    grammar_range = {L.rank(std::get<0>(L_range), t_offset) + 1, L.rank(std::get<1>(L_range), t_offset)};
    if (std::get<0>(grammar_range) > std::get<1>(grammar_range)) {
        grammar_range = {1, 0};
    }
    return;
}

template <typename Iterator>
uint64_t XBWT::infixMatch(Iterator begin, Iterator end) {
    xbwt_range L_range = {0, last_select(1) + 1};
    auto it {begin};
    while (it != end) {
        if (!DownwardNavigation(L_range, *it)) { return 0; }
        it = std::next(it);
    }
    size_t grammar_below_l = L.rank(std::get<0>(L_range), t_offset);
    size_t grammar_below_r = L.rank(std::get<1>(L_range), t_offset);
    if (grammar_below_l == grammar_below_r) { return 0; } // no grammar within [l, r)
    else { return grammar_below_r; }
}

std::ostream& operator<< (std::ostream &out, XBWT const &xbwt) {
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
        out << "\n";
    }
    return out;
}

std::pair<size_t, size_t> XBWT::pi_range_to_L_range(xbwt_range& pi_range) const {
    return {
        last_select(std::get<0>(pi_range)) + 1,
        last_select(std::get<1>(pi_range)) + 1
    };
}

std::pair<size_t, size_t> XBWT::L_range_to_pi_range(xbwt_range& L_range, size_t c) const {
    return {
        C[c] - C[2] + L.rank(std::get<0>(L_range), c) + 1,
        C[c] - C[2] + L.rank(std::get<1>(L_range), c) + 1
    };
}


std::pair<size_t, size_t> XBWT::match_pos_in_pattern(const sdsl::int_vector<>& pattern) {
    xbwt_range L_range = {0, last_select(1) + 1};
    for (size_t i = 0; i < pattern.size(); i++) {
        size_t c = pattern[i];
        while (!DownwardNavigation(L_range, c) && std::get<0>(L_range) > 0) { failureLink(std::get<0>(L_range), std::get<1>(L_range)); }
        if (L[std::get<0>(L_range)] == 1) { 
            size_t len = nodeHeight(std::get<0>(L_range), std::get<1>(L_range));
            return std::make_pair(i - len + 1, len);
        }
    }
    return std::make_pair(-1, -1);
}

void XBWT::match_pos_in_pattern(const sdsl::int_vector<>& pattern, size_t &offset, size_t &length, size_t &rank) {
    rank = 0;
    xbwt_range L_range = {0, last_select(1) + 1};
    for (size_t i = 0; i < pattern.size(); i++) {
        size_t c = pattern[i];
        while (!DownwardNavigation(L_range, c) && std::get<0>(L_range) > 0) { failureLink(std::get<0>(L_range), std::get<1>(L_range)); }
        if (L[std::get<0>(L_range)] == 1) { 
            length = nodeHeight(std::get<0>(L_range), std::get<1>(L_range));
            offset = i - length + 1;
            rank = L.rank(std::get<1>(L_range), 1);
            return;
        }
    }
}

bool XBWT::match_if_exist(const sdsl::int_vector<>& pattern) {
    xbwt_range L_range = {0, last_select(1) + 1};
    bool result = false;
    for (size_t i = 0; i < pattern.size(); i++) {
        size_t c = pattern[i];
        while (!DownwardNavigation(L_range, c) && std::get<0>(L_range) > 0) { failureLink(std::get<0>(L_range), std::get<1>(L_range)); }
        if (L[std::get<0>(L_range)] == 1) { 
            result = true;
            break;
        }
    }
    return result;
}