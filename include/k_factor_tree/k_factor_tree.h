#pragma once
#include <string>
#include <vector>
#include <queue>
#include <tuple>
#include <memory>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <sys/resource.h>
#include <sys/time.h>
#include "k_factor_tree_node.h"

class k_factor_tree {
using Node = k_factor_tree_node;

private:
    Node *root;
    size_t lambda, tau_l, tau_u;
    std::unordered_set<unsigned char> delimiters {'\0', 127};
    //std::string text;
    std::vector<Node*> node_vector; // debug only
    std::vector<std::pair<size_t, size_t>> min_factors;
    //std::unordered_map<size_t, char> position_to_char;

    size_t dfs_preSum(Node* node);
    void gen_failure_links(const std::string &text);
    void gen_mf(const std::string &text, size_t tau_l, size_t tau_u);
    void insert(const std::string &text, size_t start, size_t end, size_t lambda); // [start, end)
    void delimiter_to_char(const std::string &delimiter) {
        for (size_t i = 0, e = delimiter.size(); i < e; i++) {
            char c = delimiter[i];
            if (delimiter[i] == '\\' && i+1 < e) {
                char c__ = delimiter[i+1];
                if (c__ == '0') { c = 0; }
                else if (c__ == '1') { c = 1; }
                else if (c__ == '2') { c = 2; }
                else if (c__ == '3') { c = 3; }
                else if (c__ == '4') { c = 4; }
                else if (c__ == '5') { c = 5; }
                else if (c__ == '6') { c = 6; }
                else if (c__ == '7') { c = 7; }
                else if (c__ == '8') { c = 8; }
                else if (c__ == 't') { c = 9; }
                else if (c__ == 'n') { c = 10; }
                else if (c__ == 'v') { c = 11; }
                else if (c__ == 'f') { c = 12; }
                else if (c__ == 'r') { c = 13; }
                i = i + 1;
                if (c == '\\') { i--; }
            }
            delimiters.insert(c);
        }
    }
    std::string delimiters_to_string() {
        std::string delimiter;
        for (char c : delimiters) {
            if (c < 14) {
                delimiter.append("\\");
                if (c == 0) { delimiter.push_back('0'); }
                else if (c == 1) { delimiter.push_back('1'); }
                else if (c == 2) { delimiter.push_back('2'); }
                else if (c == 3) { delimiter.push_back('3'); }
                else if (c == 4) { delimiter.push_back('4'); }
                else if (c == 5) { delimiter.push_back('5'); }
                else if (c == 6) { delimiter.push_back('6'); }
                else if (c == 7) { delimiter.push_back('7'); }
                else if (c == 8) { delimiter.push_back('8'); }
                else if (c == 9) { delimiter.push_back('t'); }
                else if (c == 10) { delimiter.push_back('n'); }
                else if (c == 11) { delimiter.push_back('v'); }
                else if (c == 12) { delimiter.push_back('f'); }
                else if (c == 13) { delimiter.push_back('r'); }
            } else {
                delimiter.push_back(c);
            }
        }
        return delimiter;
    }

public:
    k_factor_tree(){}
    k_factor_tree(const std::string &text, size_t lambda, size_t tau_l, size_t tau_u, const std::string &delimiter);
    size_t count(std::string &text, std::string &pattern);
    size_t get_node_cnt() { return node_vector.size(); }
    std::vector<std::pair<size_t, size_t>> get_min_factors() { return min_factors; }
    void Serialize_min_factors (std::ostream &out, std::string &inputTextPath);
    ~k_factor_tree(){
        for (auto node : node_vector) {
            delete node;
        }
    }
    
    /*
    friend std::ostream& operator<< (std::ostream &out, k_factor_tree const &k_factor_tree) {
        out << "[k_factor_tree level order begin]\n";
        std::queue<Node*> que_node;
        for (auto [c, next_node] : k_factor_tree.root->links) {
            que_node.push(next_node);
        }
        size_t level = 0;
        while (!que_node.empty()) {
            out << "level: " << level++ << "\n";
            for (size_t i = que_node.size(); i > 0; i--) {
                Node* node = que_node.front();
                que_node.pop();
                for (auto j = node->start; j <= node->end; j++) {
                    out << k_factor_tree.text[j];
                }
                out << "\n";
                for (auto [c, next_node] : node->links) {
                    que_node.push(next_node);
                }
            }
        }
        out << "[k_factor_tree level order end]\n";
        return out;
    }
    */
};

void k_factor_tree::Serialize_min_factors (std::ostream &out, std::string &inputTextPath) {
    out << inputTextPath << "\n";
    out << tau_l << "\t" << tau_u << "\t" << lambda << "\t";
    out << delimiters_to_string() << "\t";
    out << min_factors.size() << "\n";
    for (auto [a, b] : min_factors) { out << a << "\t" << b << "\n"; }
}

void k_factor_tree::gen_failure_links(const std::string &text) {
    std::queue<Node*> que;
    for (auto [c, child] : root->links) {
        if (!child->is_leaf) {
            que.push(child);
        }
    }
    // que.push(root);
    while (!que.empty()) {
        Node* node = que.front();
        que.pop();
        for (auto [c, child] : node->links) {
            if (!child->is_leaf) {
                que.push(child);
            }
        }
        if (node->suffix_link) { continue; }
        else if (node->parent == root) {
            if (node->get_length(0) == 1) { node->suffix_link = root; }
            else {
                size_t start = node->start + 1;
                Node *next_node = root->links[text[start]];
                size_t leftLen = node->get_length(0) - 1, curLen = next_node->get_length(0);
                while (leftLen > curLen) {
                    start += curLen;
                    leftLen -= curLen;
                    next_node = next_node->links[text[start]];
                    curLen = next_node->get_length(0);
                }
                node->suffix_link = next_node; 
            }
        } else {
            size_t start = node->start;
            Node *next_node = node->parent->suffix_link->links[text[start]];
            size_t leftLen = node->get_length(0), curLen = next_node->get_length(0);
            while (leftLen > curLen) {
                start += curLen;
                leftLen -= curLen;
                next_node = next_node->links[text[start]];
                curLen = next_node->get_length(0);
            }
            node->suffix_link = next_node; 
        }
    }
}

void k_factor_tree::gen_mf(const std::string &text, size_t tau_l, size_t tau_u) {
    size_t n = text.size(), l = 0, r = 0;
    Node* node = root;
    while (r < n) {
        char c = text[r];
        while (r < n && !node->is_leaf && node->links[c]->cnt > tau_u && delimiters.count((unsigned char) c) == 0) {
            r += node->links[c]->get_length(0);
            node = node->links[c];
            c = text[r];
        }
        if (r >= n) {
            break;
        } else if (delimiters.count((unsigned char) c)) {
            node = root;
            l = ++r;
        } else if (node->is_leaf) {
            r -= node->get_length(0);
            node = node->parent->suffix_link;
            l++;
            if (l > r) { r = l; }
            //std::cout << "a,";
        } else {
            // Node* next_node = node->suffix_link;
            while (node != root && node->suffix_link->links[c]->cnt <= tau_u) {
                l++;
                node = node->suffix_link;
            }
            // if (node->suffix_link->links[c]->des_node->cnt >= tau_l) {
            if (node->links[c]->cnt >= tau_l) {
                min_factors.push_back({l, r});
            }
            l++;
            //std::cout << "b,";
            if (l > r) { r = l; }
            node = node->suffix_link;
        }
        //std::cout << l << ",";
    }
}

size_t k_factor_tree::count(std::string &text, std::string &pattern) {
    Node *node = root, *child;
    char c = pattern[0];
    if (node->links.count(c) == 0) { return 0; }
    size_t i = 0, j = 0, n = pattern.size(), m = node->links[c]->get_length(0);
    while (i < n && j < m) {
        size_t start = node->links[c]->start;
        if (pattern[i] != text[start + j]) { return 0; }
        if (++i == n) { return node->links[c]->cnt; }
        if (++j == m) {
            node = node->links[c];
            c = pattern[i];
            if (node->links.count(c) == 0) { return 0; }
            m = node->links[c]->get_length(0);
            j = 0;
        }
    }
    return 0;
}

size_t k_factor_tree::dfs_preSum(Node* node) {
    if (node->is_leaf) { return node->cnt; }
    node->cnt = 0;
    for (auto [c, child] : node->links) {
        node->cnt += dfs_preSum(child);
    }
    return node->cnt;
}

k_factor_tree::k_factor_tree(const std::string &text, size_t lambda, size_t tau_l, size_t tau_u, const std::string &delimiter): 
    lambda(lambda), tau_l(tau_l), tau_u(tau_u)
{
    if (tau_u > 0) {
        delimiter_to_char(delimiter);

        if (lambda == 0) { lambda = text.size() + 1; }
        else if (lambda == 1) { throw std::invalid_argument("lambda can't be 1"); } // for the gen_masked_notation(), but not sure if necessary
        if (text.size() == 0) { throw std::invalid_argument("input text is empty"); }
        if (delimiters.count((unsigned char) text[0])) { throw std::invalid_argument("input text start with delimiter symbol"); }
        if (tau_l > tau_u) { throw std::invalid_argument("tau_l > tau_u"); }

        size_t n = text.size();
        root = new Node();
        root->suffix_link = root;
        node_vector.push_back(root);
        size_t prev = 0, i = 0;
        while (prev < n && i < n) {
            while (prev < n && delimiters.count((unsigned char) text[prev])) { prev++; }
            i = prev + 1;
            while (i < n && delimiters.count((unsigned char) text[i]) == 0) { i++; }
            if (prev < n) { insert(text, prev, i + 1, std::min(lambda, i - prev + 2)); } // insert the intervals [prev, i)
            prev = i + 1;
        }
        // std::cout << "node_vector.size(): " << node_vector.size() << "\n";
        //t2 = std::chrono::steady_clock::now();
        dfs_preSum(root);
        gen_failure_links(text);
        //t3 = std::chrono::steady_clock::now();
        gen_mf(text, tau_l, tau_u);
        //t4 = std::chrono::steady_clock::now();
        // gen_masked_notation(text, lambda); to be deleted
        //t5 = std::chrono::steady_clock::now();
        //std::string outputDir = "/mnt/f/alg/git/multi-layer-figiss/experiments/ksf_partition.txt";
        //std::ofstream exp_results(outputDir, std::ios_base::app);
        //exp_results << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << "\t"; // "comsum time (ms): "
        //exp_results << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count() << "\t"; // "comsum time (ms): "
        //exp_results << std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count() << "\t"; // "comsum time (ms): "
        //exp_results << std::chrono::duration_cast<std::chrono::milliseconds>(t5 - t4).count() << "\n"; // "comsum time (ms): "
        //exp_results.close();
    }
}

void k_factor_tree::insert(const std::string &text, size_t start, size_t end, size_t lambda) {
    size_t n = end;
//std::cout << "c1\n";
    // part I
    Node *activeNode = root, *lastNode = root;
    size_t activeLen = 0, remaining = 0;
    char activeEdge = 0;
    size_t i = start, l = start; // global_end
    bool endPhase = false;
    std::queue<Node*> que;
//std::cout << "c2\n";
    auto addString = [&](Node *node, size_t start, size_t end) {
        endPhase = true;
        bool endJump = false;
        while (!endJump && (end - start != 0)) {
            char c = text[start];
            Node *child = node->links[c];
            if (end - start >= child->get_length(i)) {
                start += child->get_length(i);
                node = child;
                c = text[start];
            } else {
                endJump = true;
            }
        }
        activeNode = node;
        activeLen = end - start;
        if (end - start == 0) { // stop at node
            char c = text[start];
            if (node->links.count(c) == 0) {
                node->links[c] = new Node(node, start, 0, false, true, 1);
                que.push(node->links[c]);
                remaining--;
                endPhase = false;
                //activeNode = node;
                //activeLen = end - start;
                if (lastNode != root) { lastNode->suffix_link = activeNode; }
                lastNode = activeNode;
                if (activeNode == root && activeLen > 0) { activeLen--; }
                if (node->suffix_link) { activeNode = node->suffix_link; }
                else {
                    activeNode = root;
                    activeLen = remaining - 1;
                }
                node_vector.push_back(node->links[c]);
            } else if (1 + end - start == node->links[c]->get_length(i) && node->links[c]->is_leaf) {
                node->links[c]->cnt++;
                remaining--;
                endPhase = false;
                //activeNode = node;
                //activeLen = end - start;
                if (activeNode == root && activeLen > 0) { activeLen--; }
                if (node->suffix_link) { activeNode = node->suffix_link; }
                else {
                    activeNode = root;
                    activeLen = remaining - 1;
                }
            } else {
                //activeNode = node;
                activeLen = end - start + 1;
            }
            //node_vector.push_back(lastLeaf);
        } else { // stop at the middle of an edge
            char c = text[end], edge_c = text[node->links[text[start]]->start + end - start];
            if (edge_c != c) {
                Node *original_node = node->links[text[start]];
                Node *split_node = new Node(node, original_node->start, original_node->start + end - start - 1, true, false, 0);
                node->links[text[start]] = split_node;
                split_node->links[c] = new Node(split_node, end, end, false, true, 1);
                split_node->links[edge_c] = original_node;
                original_node->parent = split_node;
                original_node->start = original_node->start + end - start;
                que.push(split_node->links[c]);
                if (lastNode != root) { lastNode->suffix_link = split_node; }
                lastNode = split_node;
                remaining--;
                endPhase = false;
                if (activeNode == root && activeLen > 0) { activeLen--; }
                if (node->suffix_link) { activeNode = node->suffix_link; }
                else {
                    activeNode = root;
                    activeLen = remaining - 1;
                }
                node_vector.push_back(split_node);
                node_vector.push_back(split_node->links[c]);
            } else if (1 + end - start == node->links[text[start]]->get_length(i) && node->links[text[start]]->is_leaf) {
                node->links[text[start]]->cnt++;
                remaining--;
                endPhase = false;
                //activeNode = node;
                //activeLen = end - start;
                if (activeNode == root && activeLen > 0) { activeLen--; }
                if (node->suffix_link) { activeNode = node->suffix_link; }
                else {
                    activeNode = root;
                    activeLen = remaining - 1;
                }
            } else {
                //activeNode = node;
                activeLen = end - start + 1;
            }
            //node_vector.push_back(split_node);
            //node_vector.push_back(lastLeaf);
        }
    };
    // addString(root, start, start);
    for (; i < n; i++) {
        remaining++;
        endPhase = false;
        lastNode = root;
        while (remaining > 0 && !endPhase) {
            addString(activeNode, i - activeLen, i);
        };
        if (i >= start + lambda - 1 && !que.empty()) {
            Node* leaf = que.front();
            que.pop();
            leaf->end = i;
            leaf->fix_end = true;
        }
    }
    /*
//std::cout << "c3\n";
    for (i = start + 1; i < start + lambda - 1; i++) {
        endPhase = false;
        lastNode = root;
        do {
            size_t forward = lastLeaf->parent->get_length(i) - 1;
            Node* parent_node = lastLeaf->parent->src_node;
            while (parent_node != root && !parent_node->suffix_link) {
                forward += parent_node->parent->get_length(i);
                parent_node = parent_node->parent->src_node;
            }
            if (parent_node == root) { addString(root, i - forward + 1, i); }
            else { addString(parent_node->suffix_link, i - forward, i); }
        } while (!endPhase);
    }
//std::cout << "c4\n";
    // part II
    for (i = start + lambda - 1; i < n; i++) {
        endPhase = false;
        lastNode = root;
        if (que.empty()) {
            addString(lastPosition_node, i - lastPosition_dist - 1, i);
            if (que.empty()) { // no leaf was created
                lastLeaf = lastPosition_node->links[text[i - lastPosition_dist]]->des_node; // lastPosition_dist will +1 more after the addString function above
                lastLeaf->cnt++; // maybe?
            }
        }
        do {
            size_t forward = lastLeaf->parent->get_length(i) - 1;
            Node* parent_node = lastLeaf->parent->src_node;
            while (parent_node != root && !parent_node->suffix_link) {
                forward += parent_node->parent->get_length(i);
                parent_node = parent_node->parent->src_node;
            }
            if (parent_node == root) { addString(root, i - forward + 1, i); }
            else { addString(parent_node->suffix_link, i - forward, i); }
        } while (!endPhase);
        if (!que.empty()) {
            Node* leaf = que.front();
            que.pop();
            leaf->parent->end = i;
            leaf->parent->fix_end = true;
        }
    }
    */
    //std::cout << "que.size(): " << que.size() << "\n";
    while (!que.empty()) {
        Node* leaf = que.front();
        que.pop();
        leaf->end = n-1;
        leaf->fix_end = true;
    }
    // debug
    //node_vector[0] = root;
    //for (size_t i = 0; i < node_vector.size(); i++) {
    //    node_vector[i]->idx = i;
    //}
    //lastNode->suffix_link = root;
//std::cout << "c5\n";
}