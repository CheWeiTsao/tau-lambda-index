#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <random>
#include <sys/resource.h>
#include <sys/time.h>
#include "../Minimun_factors.h"
#include "../k_index.h"
#include "../Ac_automata.h"
#include "k_factor_tree.h"
#include "../../r-index/internal/r_index.hpp"

std::chrono::steady_clock::time_point t1, t2, t3, t4, t5, t6;

std::vector<std::string> dataSize {"2000MB"};
std::vector<std::string> peopleCount {"200People"};
std::vector<int> tauValue {2,6,10,14,18,22};
std::vector<int> lowertauValue {0};
std::vector<int> lambdaValue {20,60,100,200,400,800};
std::vector<int> pattern_length {10,50,100,200,400};
std::vector<int> repeat_times {10000};

// functions

void gen_query_pattern(std::string &content, std::string &query_pattern, std::vector<std::pair<size_t, size_t>> &mf, int length, int lambda) {
    // gen random query pattern
    std::random_device rand;
    std::mt19937 gen(rand());
    std::uniform_int_distribution<> rand_idx(0, mf.size() - 1);
    size_t idx = rand_idx(gen);
    size_t start = 0, end = content.size() - length;
    if (std::get<1>(mf[idx]) > lambda) { start = std::get<1>(mf[idx]) - lambda; }
    if (std::get<0>(mf[idx]) + lambda - length <= end) { end = std::get<0>(mf[idx]) + lambda - length; }
    std::uniform_int_distribution<> rand_begin(start, end);
    query_pattern = content.substr(rand_begin(gen), length);
}

// experiments

void gen_min_factors_measurement(bool need_double_bwt, bool if_exist_file_then_skip) {
    std::string plainTextPath = "/mnt/f/alg/git/archived/multi-layer-figiss/dataset/";
    //std::string mfTextDir = "";
    std::string outputDir = "/mnt/f/alg/git/multi-layer-figiss/experiments/";
    if (!std::filesystem::exists(outputDir)) {
        if (!std::filesystem::create_directories(outputDir)) {
            throw std::invalid_argument("failed to create experiment output directory");
        }
    }
    std::string mf_outputDir = outputDir + "min_factors/";
    if (!std::filesystem::exists(mf_outputDir)) {
        if (!std::filesystem::create_directories(mf_outputDir)) {
            throw std::invalid_argument("failed to create min_factors output directory");
        }
    }
    std::string masked_notation_outputDir = outputDir + "masked_notations/";
    if (!std::filesystem::exists(masked_notation_outputDir)) {
        if (!std::filesystem::create_directories(masked_notation_outputDir)) {
            throw std::invalid_argument("failed to create masked_notations output directory");
        }
    }
    std::ofstream exp_log(outputDir + "mf_log.txt", std::ios_base::app);

    // k_factor_tree
    exp_log << "[k_factor_tree]\n";
    exp_log << "name\ttau'\ttau\tlambda\ttime(ms)\tcoverage_rate\n";
    for (std::string size : dataSize) {
        for (std::string people : peopleCount) {
            std::string content;
            std::ifstream plainText(plainTextPath + size + "_" + people + ".txt");
            std::getline(plainText, content);
            plainText.close();

            for (int tau : tauValue) {
                for (int lambda : lambdaValue) {
                    for (int lowertau : lowertauValue) {
                        std::cout << "process k_factor_tree with tau = " << tau << ", lambda = " << lambda << "\n";
                        std::string file_name = size + "_" + people + "_" + std::to_string(lowertau) + "_" + std::to_string(tau) + "_" + std::to_string(lambda);
                        if (if_exist_file_then_skip && std::filesystem::exists(mf_outputDir + file_name + ".txt")) {
                            continue;
                        }

                        t1 = std::chrono::steady_clock::now();
                        k_factor_tree ksf (content, lambda, lowertau, tau);
                        t2 = std::chrono::steady_clock::now();

                        std::ofstream mf_output(mf_outputDir + file_name + ".txt");
                        std::vector<std::pair<size_t, size_t>> mf = ksf.get_min_factors();
                        mf_output << mf.size() << "\n";
                        for (auto p : mf) {
                            mf_output << std::get<0>(p) << "\t" << std::get<1>(p) << "\n";
                        }
                        mf_output.close();

                        std::ofstream masked_notation_output(masked_notation_outputDir + file_name + ".txt");
                        std::vector<std::pair<size_t, size_t>> masked_notation = ksf.get_masked_notation();
                        masked_notation_output << masked_notation.size() << "\n";
                        for (auto p : masked_notation) {
                            masked_notation_output << std::get<0>(p) << "\t" << std::get<1>(p) << "\n";
                        }
                        masked_notation_output.close();

                        exp_log << (size + "_" + people) << "\t";
                        exp_log << lowertau << "\t";
                        exp_log << tau << "\t";
                        exp_log << lambda << "\t";
                        exp_log << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << "\t"; // "comsum time (ms): "
                        exp_log << ksf.get_coverage_rate() << "\n";
                        exp_log.flush();
                    }       
                }
            }
        }
    }

    // double_bwt
    if (need_double_bwt) {
        exp_log << "\n";
        exp_log << "[double_bwt]\n";
        exp_log << "tau'\ttau\tlambda\ttime(ms)\n";
        for (std::string size : dataSize) {
            for (std::string people : peopleCount) {
                std::string content;
                std::ifstream plainText(plainTextPath + size + "_" + people + ".txt");
                std::getline(plainText, content);
                plainText.close();

                for (int tau : tauValue) {
                    for (int lambda : lambdaValue) {
                        for (int lowertau : lowertauValue) {
                            std::cout << "process double_bwt with tau = " << tau << ", lambda = " << lambda << "\n";
                            t1 = std::chrono::steady_clock::now();
                            Minimun_factors mf(content, tau, lambda, true);
                            t2 = std::chrono::steady_clock::now();
                            exp_log << lowertau << "\t";
                            exp_log << tau << "\t";
                            exp_log << lambda << "\t";
                            exp_log << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << "\n"; // "comsum time (ms): "
                            exp_log.flush();
                        }
                    }
                }
            }
        }
    }

    exp_log.close();
}

void gen_index_from_scratch_measurement() {

    std::string plainTextPath = "/mnt/f/alg/git/archived/multi-layer-figiss/dataset/";
    //std::string mfTextDir = "";
    std::string outputDir = "/mnt/f/alg/git/multi-layer-figiss/experiments/gen_index_from_scratch.txt";
    std::ofstream exp_results(outputDir, std::ios_base::app);

    // k_factor_tree
    exp_results << "file_name\ttau'\ttau\tlambda\tcoverage_rate\tP1_time(ms)\tP2_time(ms)\tP3_time(ms)\n";
    for (std::string size : dataSize) {
        for (std::string people : peopleCount) {
            std::string content;
            std::string fileName = size + "_" + people;
            std::ifstream plainText(plainTextPath + fileName + ".txt");
            std::getline(plainText, content);
            plainText.close();

            for (int tau : tauValue) {
                for (int lambda : lambdaValue) {
                    for (int lowertau : lowertauValue) {
                        std::cout << "process k_factor_tree with tau = " << tau << ", lambda = " << lambda << "\n";
                        t1 = std::chrono::steady_clock::now();
                        k_factor_tree ksf (content, lambda, lowertau, tau);
                        t2 = std::chrono::steady_clock::now();
                        std::string masked_text;
                        ksf.gen_masked_text(content, masked_text, lambda);

                        std::string tmp_path = "/mnt/f/alg/git/multi-layer-figiss/experiments/" + fileName;
                        std::ofstream out(tmp_path);
                        out << masked_text;
                        out.close();
                        t3 = std::chrono::steady_clock::now();

                        std::string input;
                        {
                            std::ifstream fs(tmp_path);
                            std::stringstream buffer;
                            buffer << fs.rdbuf();

                            input = buffer.str();
                        }
                        auto r_idx = ri::r_index<>(input, true);
                        std::string r_path = tmp_path + "_" + std::to_string(lowertau) + "_" + std::to_string(tau) + "_" + std::to_string(lambda) + ".ri";
                        std::ofstream r_out(r_path);
                        r_idx.serialize(r_out);
                        r_out.close();

                        // std::filesystem::remove(tmp_path);
                        std::remove(tmp_path.c_str());

                        t4 = std::chrono::steady_clock::now();
                        exp_results << fileName << "\t";
                        exp_results << lowertau << "\t";
                        exp_results << tau << "\t";
                        exp_results << lambda << "\t";
                        exp_results << ksf.get_coverage_rate() << "\t";
                        exp_results << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() << "\t"; // "comsum time (ms): "
                        exp_results << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count() << "\t"; // "comsum time (ms): "
                        exp_results << std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count() << "\n"; // "comsum time (ms): "
                        exp_results.flush();
                    }       
                }
            }
        }
    }
    exp_results.close();
}

void query_time_measurement() {
    std::string plainTextPath = "/mnt/f/alg/git/archived/multi-layer-figiss/dataset/";
    //std::string mfTextDir = "";
    std::string outputDir = "/mnt/f/alg/git/multi-layer-figiss/experiments/";
    if (!std::filesystem::exists(outputDir)) {
        throw std::invalid_argument("log output directory doesn't exist");
    }
    //std::string masked_notation_outputDir = outputDir + "masked_notations/";
    //if (!std::filesystem::exists(masked_notation_outputDir)) {
    //    throw std::invalid_argument("masked_notations output directory doesn't exist");
    //}
    std::string mf_outputDir = outputDir + "min_factors/";
    if (!std::filesystem::exists(mf_outputDir)) {
        throw std::invalid_argument("min_factors output directory doesn't exist");
    }
    std::string r_index_outputDir = outputDir + "r_index/";
    if (!std::filesystem::exists(r_index_outputDir)) {
        throw std::invalid_argument("r_index output directory doesn't exist");
    }
    std::ofstream exp_log(outputDir + "query_time_log.txt", std::ios_base::app);

    // k_factor_tree
    exp_log << "name\ttau'\ttau\tlambda\tpattern_length\ttime_per_char(xbwt)(us)(masked)\ttime_per_char(r-index)(us)(masked)\ttime_per_char(us)(raw)\trepeat_times\ttypes\n";
    for (std::string size : dataSize) {
        for (std::string people : peopleCount) {
            std::string content;
            std::ifstream plainText(plainTextPath + size + "_" + people + ".txt");
            std::getline(plainText, content);
            plainText.close();

            for (int tau : tauValue) {
                for (int lambda : lambdaValue) {
                    for (int lowertau : lowertauValue) {
                        std::cout << "process k_factor_tree with tau = " << tau << ", lambda = " << lambda << "\n";

                        // load min_factors
                        std::string file_name = size + "_" + people + "_" + std::to_string(lowertau) + "_" + std::to_string(tau) + "_" + std::to_string(lambda);
                        std::vector<std::pair<size_t, size_t>> mf; // begin, end
                        {
                            if (!std::filesystem::exists(mf_outputDir + file_name + ".txt")) {
                                throw std::invalid_argument("masked notation file doesn't exist.");
                            }
                            std::ifstream input(mf_outputDir + file_name + ".txt");
                            size_t cnt, a, b;
                            input >> cnt;
                            for (size_t i = 0; i < cnt; i++) {
                                input >> a >> b;
                                mf.push_back({a, b});
                            }
                            input.close();
                        }
                        std::sort(mf.begin(), mf.end());
                        if (mf.size() == 0) {
                            throw std::invalid_argument("min_factors is empty.");
                        }

                        auto r_idx_raw = ri::r_index<>();
                        { // load r-index
                            std::string r_raw_path = r_index_outputDir + size + "_" + people + ".ri";
                            { // check if raw r-index exist
                                if (!std::filesystem::exists(r_raw_path)) {
                                    std::string input;
                                    {
                                        std::ifstream fs(plainTextPath + size + "_" + people + ".txt");
                                        std::stringstream buffer;
                                        buffer << fs.rdbuf();

                                        input = buffer.str();
                                    }
                                    auto r_idx = ri::r_index<>(input, true);
                                    std::ofstream idx_out(r_raw_path);
                                    r_idx.serialize(idx_out);
                                    idx_out.close();
                                }
                            }
                            std::ifstream idx_in(r_raw_path);
                            r_idx_raw.load(idx_in);
                            idx_in.close();
                        }

                        k_index* r_idx_masked {new k_index()};;
                        { // load masked r-index
                            std::string r_maksed_path = r_index_outputDir + file_name + ".ri";
                            { // check if masked r-index exist
                                if (!std::filesystem::exists(r_maksed_path)) {
                                    r_idx_masked = new k_index(plainTextPath + size + "_" + people + ".txt", lowertau, tau, lambda);
                                    std::ofstream idx_out(r_maksed_path);
                                    r_idx_masked->serialize(idx_out);
                                    idx_out.close();
                                }
                            }
                            std::ifstream idx_in(r_maksed_path);
                            r_idx_masked->load(idx_in);
                            idx_in.close();
                        }

                        for (int length : pattern_length) {
                            if (length > lambda) {
                                exp_log << "query pattern length > lambda, skip this query\n";
                                continue;
                            }
                            for (int repeats : repeat_times) {
                                // long long time_sum_masked = 0, time_sum_raw = 0;
                                // xxx_1/2: existing query pattern with/without min_factor
                                long long time_masked_xbwt_1 = 0, time_masked_r_1 = 0, time_raw_1 = 0, cnt_1 = 0;
                                long long time_masked_xbwt_2 = 0, time_masked_r_2 = 0, time_raw_2 = 0, cnt_2 = 0;

                                for (size_t i = 0; i < repeats; i++) {
                                    std::string q;
                                    gen_query_pattern(content, q, mf, length, lambda);

                                    t1 = std::chrono::steady_clock::now();
                                    bool b = r_idx_masked->locate_xbwt(q);
                                    t2 = std::chrono::steady_clock::now();
                                    std::vector<ulint> occ_masked;
                                    if (b) {
                                        t5 = std::chrono::steady_clock::now();
                                        occ_masked = r_idx_masked->locate_r_index(q);
                                        t6 = std::chrono::steady_clock::now();
                                    }
                                    // time_sum_masked += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

                                    t3 = std::chrono::steady_clock::now();
                                    auto occ_raw = r_idx_raw.locate_all_tau(q, lowertau, tau);
                                    // auto occ_raw = r_idx_raw.locate_all(q);
                                    t4 = std::chrono::steady_clock::now();
                                    // time_sum_raw += std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count();

                                    // correction_check
                                    std::sort(occ_masked.begin(), occ_masked.end());
                                    std::sort(occ_raw.begin(), occ_raw.end());
                                    if (occ_masked != occ_raw) {
                                        throw std::invalid_argument("location results not comparison with baseline");
                                    }

                                    if (occ_raw.size() > 0) {
                                        time_masked_xbwt_1 += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                                        time_masked_r_1 += std::chrono::duration_cast<std::chrono::microseconds>(t6 - t5).count();
                                        time_raw_1 += std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count();
                                        cnt_1 += 1;
                                    } else {
                                        time_masked_xbwt_2 += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                                        time_masked_r_2 += std::chrono::duration_cast<std::chrono::microseconds>(t6 - t5).count();
                                        time_raw_2 += std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count();
                                        cnt_2 += 1;
                                    }
                                }

                                // type_1 (with min_factors)
                                exp_log << (size + "_" + people) << "\t";
                                exp_log << lowertau << "\t";
                                exp_log << tau << "\t";
                                exp_log << lambda << "\t";
                                exp_log << length << "\t";
                                exp_log << (cnt_1 == 0 ? -1 : 1.0 * time_masked_xbwt_1 / (length * cnt_1)) << "\t";
                                exp_log << (cnt_1 == 0 ? -1 : 1.0 * time_masked_r_1 / (length * cnt_1)) << "\t";
                                exp_log << (cnt_1 == 0 ? -1 : 1.0 * time_raw_1 / (length * cnt_1)) << "\t";
                                exp_log << cnt_1 << "\t"; // "comsum time (us): "
                                exp_log << "with mf" << "\n";

                                // type_2 (without min_factors)
                                exp_log << file_name << "\t";
                                exp_log << lowertau << "\t";
                                exp_log << tau << "\t";
                                exp_log << lambda << "\t";
                                exp_log << length << "\t";
                                exp_log << (cnt_2 == 0 ? -1 : 1.0 * time_masked_xbwt_2 / (length * cnt_2)) << "\t";
                                exp_log << (cnt_2 == 0 ? -1 : 1.0 * time_masked_r_2 / (length * cnt_2)) << "\t";
                                exp_log << (cnt_2 == 0 ? -1 : 1.0 * time_raw_2 / (length * cnt_2)) << "\t";
                                exp_log << cnt_2 << "\t"; // "comsum time (us): "
                                exp_log << "without mf" << "\n";
                            }
                        }
                        exp_log.flush();
                    }       
                }
            }
        }
    }

    exp_log.close();
}

void extract_r_index_runs() {
    std::string plainTextPath = "/mnt/f/alg/git/archived/multi-layer-figiss/dataset/";
    //std::string mfTextDir = "";
    std::string outputDir = "/mnt/f/alg/git/multi-layer-figiss/experiments/";
    if (!std::filesystem::exists(outputDir)) {
        throw std::invalid_argument("log output directory doesn't exist");
    }
    std::string mf_outputDir = outputDir + "min_factors/";
    if (!std::filesystem::exists(mf_outputDir)) {
        throw std::invalid_argument("min_factors output directory doesn't exist");
    }
    std::string r_index_outputDir = outputDir + "r_index/";
    if (!std::filesystem::exists(r_index_outputDir)) {
        throw std::invalid_argument("r_index output directory doesn't exist");
    }
    std::ofstream exp_log(outputDir + "bwt_runs_log.txt", std::ios_base::app);

    // k_factor_tree
    exp_log << "name\ttau'\ttau\tlambda\tbwt_runs\n";
    for (std::string size : dataSize) {
        for (std::string people : peopleCount) {
            std::string content;
            std::ifstream plainText(plainTextPath + size + "_" + people + ".txt");
            std::getline(plainText, content);
            plainText.close();

            for (int tau : tauValue) {
                for (int lambda : lambdaValue) {
                    for (int lowertau : lowertauValue) {
                        // std::cout << "process k_factor_tree with tau = " << tau << ", lambda = " << lambda << "\n";

                        k_index* r_idx_masked {new k_index()};;
                        { // load masked r-index
                            std::string file_name = size + "_" + people + "_" + std::to_string(lowertau) + "_" + std::to_string(tau) + "_" + std::to_string(lambda);
                            std::string r_maksed_path = r_index_outputDir + file_name + ".ri";
                            { // check if masked r-index exist
                                if (!std::filesystem::exists(r_maksed_path)) {
                                    throw std::invalid_argument("r_index doesn't exist, please gen r_index first");
                                }
                            }
                            std::ifstream idx_in(r_maksed_path);
                            r_idx_masked->load(idx_in);
                            idx_in.close();
                        }

                        exp_log << size + "_" + people << "\t";
                        exp_log << lowertau << "\t";
                        exp_log << tau << "\t";
                        exp_log << lambda << "\t";
                        exp_log << r_idx_masked->get_bwt_runs() << "\n";
                        exp_log.flush();
                    }       
                }
            }
        }
    }

    exp_log.close();
}

void xbwt_AC_size_compare() {
    uint64_t t_offset = static_cast<uint64_t>(1); // terminate symbols
    std::string plainTextPath = "/mnt/f/alg/git/archived/multi-layer-figiss/dataset/";
    //std::string mfTextDir = "";
    std::string outputDir = "/mnt/f/alg/git/multi-layer-figiss/experiments/";
    if (!std::filesystem::exists(outputDir)) {
        throw std::invalid_argument("log output directory doesn't exist");
    }
    std::string mf_outputDir = outputDir + "min_factors/";
    if (!std::filesystem::exists(mf_outputDir)) {
        throw std::invalid_argument("min_factors output directory doesn't exist");
    }
    std::string r_index_outputDir = outputDir + "r_index/";
    if (!std::filesystem::exists(r_index_outputDir)) {
        throw std::invalid_argument("r_index output directory doesn't exist");
    }

    for (std::string size : dataSize) {
        for (std::string people : peopleCount) {
            std::string content;
            std::ifstream plainText(plainTextPath + size + "_" + people + ".txt");
            std::getline(plainText, content);
            plainText.close();

            for (int tau : tauValue) {
                for (int lambda : lambdaValue) {
                    for (int lowertau : lowertauValue) {
                        std::string fileName = size + "_" + people + "_" + std::to_string(lowertau) + "_" + std::to_string(tau) + "_" + std::to_string(lambda);
                        std::string exp_outputDir = outputDir + "/xbwt_AC_size/" + fileName + "/";
                        if (!std::filesystem::exists(exp_outputDir)) {
                            std::filesystem::create_directories(exp_outputDir);
                        }
                        // load mf
                        std::string mf_path = mf_outputDir + fileName + ".txt";
                        if (!std::filesystem::exists(mf_path)) {
                            throw std::invalid_argument("min_factors doesn't exist");
                        }
                        std::ifstream mf_input(mf_path);
                        size_t mf_cnt = 0;
                        std::vector<std::pair<size_t, size_t>> mf;
                        mf_input >> mf_cnt;
                        for (size_t i = 0; i < mf_cnt; i++) {
                            size_t a, b;
                            mf_input >> a >> b;
                            mf.push_back({a, b});
                        }
                        mf_input.close();

                        // XBWT
                        XBWT xbwt;
                        SymbolTable symbol_table_;
                        std::vector<uint64_t> concated_mf; // each substring is separated by terminal_symbol (t_offset)
                        for (auto [b, e] : mf) {
                            for (size_t i = e; i >= b; i--) { concated_mf.push_back(content[i] + t_offset); } // reverse insert
                            concated_mf.push_back(t_offset);
                        }
                        sdsl::int_vector<> concated_mf_int_vector;
                        // size_t usedBit = 9; // TODO: use variable
                        concated_mf_int_vector.width(64);
                        concated_mf_int_vector.resize(concated_mf.size());
                        for (size_t i = 0; i < concated_mf.size(); i++) { concated_mf_int_vector[i] = concated_mf[i]; }
                        sdsl::append_zero_symbol(concated_mf_int_vector);
                        symbol_table_ = decltype(symbol_table_)(concated_mf_int_vector);
                        for (size_t i = 0; i < concated_mf_int_vector.size(); i++) { concated_mf_int_vector[i] = symbol_table_[concated_mf_int_vector[i]]; }
                        xbwt.insert(concated_mf_int_vector, symbol_table_.GetEffectiveAlphabetWidth(), symbol_table_.GetAlphabetSize());
                        std::ofstream xbwt_out(exp_outputDir + "xbwt");
                        symbol_table_.Serialize(xbwt_out);
                        xbwt.Serialize(xbwt_out);
                        xbwt_out.close();

                        // AC
                        Ac_automata ac_automata;
                        ac_automata = Ac_automata(content, mf);
                        std::string ac_output_path = exp_outputDir + "ac";
                        ac_automata.serialize(ac_output_path);
                    }
                }
            }
        }
    }
}

int main() {
    //gen_min_factors_measurement(false, true);
    // gen_index_from_scratch_measurement();
    // query_time_measurement();
    //extract_r_index_runs();
    xbwt_AC_size_compare();
    return 0;
}