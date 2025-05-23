#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <sstream>
#include "../include/k_factor_tree/k_factor_tree.h"
#include "../include/tau_lambda_index.h"

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] 
                  << " [input index path] [output xbwt path] [output masked_index path]" << std::endl;
        return 1;
    }

    std::string inputIndexPath = argv[1];
    std::string outputPath_1 = argv[2];
    std::string outputPath_2 = argv[3];

    std::ifstream inputIndex(inputIndexPath);
    if (!inputIndex.is_open()) {
        std::cerr << "Error: Could not open input index file " << inputIndexPath << std::endl;
        return 1;
    }

    // create a tau-lambda-index instance and operate it
    tau_lambda_index* idx = new tau_lambda_index();
    idx->load(inputIndex, inputIndexPath);

    std::ofstream outputFile_1(outputPath_1);
    if (!outputFile_1.is_open()) {
        std::cerr << "Error: Could not open output xbwt file " << outputPath_1 << std::endl;
        return 1;
    }

    std::ofstream outputFile_2(outputPath_2);
    if (!outputFile_2.is_open()) {
        std::cerr << "Error: Could not open output masked_index file " << outputPath_2 << std::endl;
        return 1;
    }

    idx->serialize_partition(outputFile_1, outputFile_2);

    
    inputIndex.close();
    outputFile_1.close();
    outputFile_2.close();

    std::cout << "tau-lambda-index location queries completed successfully!" << std::endl;
    return 0;
}
