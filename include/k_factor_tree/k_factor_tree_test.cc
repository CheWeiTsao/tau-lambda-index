#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include "k_factor_tree.h"
#include "../Minimun_factors.h"

void normal_test() {
    // std::string text = "abracadabraabracadabra$";
    // std::string text = "AABBCCABC$AABBC$";
    // std::string text = "AABBCCABC$AABBCCABC$";
    // std::string text = "CACTACCACCACCACCACCACCACCACCGTCACCACCACCA$";
    
    std::string text;
    std::ifstream text_in ("/mnt/f/alg/git/archived/multi-layer-figiss/dataset/10MB_200People.txt");
    std::getline(text_in, text);
    std::cout << "text.size(): " << text.size() << "\n";
    text_in.close();

    k_factor_tree sf (text, 1000, 0, 8);
    
    std::vector<std::pair<size_t, size_t>> results  = sf.get_min_factors();
    std::cout << "results.size(): " << results.size() << "\n";
    std::ofstream mf_output ("/mnt/f/alg/git/multi-layer-figiss/mf_output.txt");
    for (auto [l, r] : results) { mf_output << l << ", " << r << "\n"; }
    mf_output.close();
    
    //std::string pattern = text.substr(9531044, 9);
    //std::cout << "pattern: " << pattern << " -- cnt = " << sf.count(text, pattern) << "\n";

    // std::cout << sf << "\n";

    //std::vector<std::string> patterns{"a", "b", "c", "d", "r", "CABC"};
    //for (auto p : patterns) {
        //std::cout << "pattern: " << p << " - " << sf.count(text, p) << "\n";
    //}
}

void compare_with_baseline() {
    std::vector<std::string> dataSize {"10MB"};
    std::vector<std::string> peopleCount {"200People"};
    std::vector<int> tauValue {6,10,20};
    std::vector<int> lowertauValue {0}; // currently can only test '0' (lower_boundary tau is not available in Minimum_factors.h)
    std::vector<int> lambdaValue {20,400,1000};

    std::string plainTextPath = "/mnt/f/alg/git/archived/multi-layer-figiss/dataset/";
    //std::string mfTextDir = "";
    
    for (std::string size : dataSize) {
        for (std::string people : peopleCount) {
            std::string content;
            std::ifstream plainText(plainTextPath + size + "_" + people + ".txt");
            std::getline(plainText, content);
            plainText.close();

            for (int tau : tauValue) {
                for (int lambda : lambdaValue) {
                    for (int lowertau : lowertauValue) {
                        std::cout << "gen_min_factors test with tau = [" << lowertau << ", " << tau << "], lambda = " << lambda << "\n";
                        
                        k_factor_tree ksf (content, lambda, lowertau, tau); // k_factor_tree
                        std::vector<std::pair<size_t, size_t>> mf1 = ksf.get_min_factors();

                        Minimun_factors mf(content, tau, lambda, true); // double_bwt
                        std::vector<std::pair<size_t, size_t>> mf2 = mf.get_min_factors();

                        if (mf1 != mf2) { throw std::invalid_argument("gen_min_factors test failed"); }
                    }       
                }
            }
        }
    }
    std::cout << "pass gen_min_factors correction test\n";
}

void gen_masked_text_correction_test() {
    std::vector<std::string> dataSize {"10MB"};
    std::vector<std::string> peopleCount {"200People"};
    std::vector<int> tauValue {6,10,20};
    std::vector<int> lowertauValue {0,2};
    std::vector<int> lambdaValue {20,400,800};

    std::string plainTextPath = "/mnt/f/alg/git/archived/multi-layer-figiss/dataset/";
    
    for (std::string size : dataSize) {
        for (std::string people : peopleCount) {
            std::string content;
            std::ifstream plainText(plainTextPath + size + "_" + people + ".txt");
            std::getline(plainText, content);
            plainText.close();

            for (int tau : tauValue) {
                for (int lambda : lambdaValue) {
                    for (int lowertau : lowertauValue) {
                        std::cout << "gen_masked_text test with tau = [" << lowertau << ", " << tau << "], lambda = " << lambda << "\n";

                        k_factor_tree ksf (content, lambda, lowertau, tau); // k_factor_tree
                        std::vector<std::pair<size_t, size_t>> mf = ksf.get_min_factors();
                        std::string text_1;
                        ksf.gen_masked_text(content, text_1, lambda);

                        // line sweeping
                        size_t n = content.size();
                        std::vector<long long> ls(n + 1, 0);
                        for (auto v : mf) {
                            size_t start = 0, end = n - 1;
                            if (std::get<1>(v) + 1 > lambda) { start = std::get<1>(v) + 1 - lambda; }
                            if (std::get<0>(v) + lambda - 1 < n) { end = std::get<0>(v) + lambda - 1; }
                            ls[start]++;
                            ls[end + 1]--;
                        }
                        std::string text_2(content);
                        for (size_t i = 0, cnt = 0; i < n; i++) {
                            cnt += ls[i];
                            if (cnt == 0) { text_2[i] = '#'; } // TODO: hard code
                        }

                        if (text_1 != text_2) { throw std::invalid_argument("gen_masked_text test failed"); }
                    }       
                }
            }
        }
    }
    
    std::cout << "pass gen_masked_text correction test\n";
}

int main() {
    compare_with_baseline();
    gen_masked_text_correction_test();
    return 0;
}