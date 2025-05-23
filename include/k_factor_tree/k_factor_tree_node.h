#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>

struct k_factor_tree_node {
    // size_t idx = 0; // debug
    std::map<char, k_factor_tree_node*> links;
    k_factor_tree_node *suffix_link = nullptr, *parent = nullptr;;
    size_t cnt, start, end; // [start, end)
    bool is_leaf, fix_end;
    k_factor_tree_node() : is_leaf(false) {};
    k_factor_tree_node(k_factor_tree_node *parent, size_t start, size_t end, bool fix_end, bool is_leaf, size_t cnt) : 
        parent(parent), start(start), end(end), fix_end(fix_end), is_leaf(is_leaf), cnt(cnt) {};

    size_t get_length(size_t i) {
        if (!fix_end) { return i - start + 1; }
        else { return end - start + 1; }
    }
};