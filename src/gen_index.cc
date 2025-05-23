#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <sstream>
#include <chrono>
#include "../include/k_factor_tree/k_factor_tree.h"
#include "../include/tau_lambda_index.h"

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] 
                  << "./gen_index [input mf path] [output index path] [self-index type] [output log path](optional)" << std::endl;
        return 1;
    }

    std::string inputMfPath = argv[1];
    std::string outputIndexPath = argv[2];
    index_types index_type = static_cast<index_types>(std::stoi(argv[3]));
    std::string outputLogPath;
    if (argc >= 5) { outputLogPath = argv[4]; }

    std::ofstream outputFile(outputIndexPath);
    if (!outputFile.is_open()) {
        std::cerr << "Error: Could not open output file " << outputIndexPath << std::endl;
        return 1;
    }

    std::ofstream outputLogFile(outputLogPath);
    if (argc >= 5 && !outputLogFile.is_open()) {
        std::cerr << "Error: Could not open output file " << outputLogPath << std::endl;
        return 1;
    }

    // create a tau-lambda-index instance and operate it
    tau_lambda_index* idx;
    std::chrono::steady_clock::time_point t1, t2;
    t1 = std::chrono::steady_clock::now();

    if (index_type == index_types::r_index_type || index_type == index_types::LMS_type) {
        idx = new tau_lambda_index(inputMfPath, index_type);
    } else if (index_type == index_types::lz77_type) {
        std::string lz77OutPath= outputIndexPath + "_lz77";
        idx = new tau_lambda_index(inputMfPath, lz77OutPath, index_type);
    } else {
        throw std::runtime_error("Invalid selfIndexType\n");
    }
    idx->serialize(outputFile);
    t2 = std::chrono::steady_clock::now();

    if (argc >= 5) {
        idx->log(outputLogFile);
        outputLogFile << "consumed time (us): " << std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count() << "\n";
        outputLogFile.close();
    }

    outputFile.close();

    std::cout << "tau-lambda-index completed successfully!" << std::endl;
    return 0;
}
