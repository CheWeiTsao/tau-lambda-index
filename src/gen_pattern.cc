#include <string>
#include <iostream>
#include <fstream>
#include "../include/tau_lambda_index.h"

enum class patternTypes {
    without_mf = 0,
    with_mf = 1
};

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] 
                  << " [input mf path] [output pattern path] [pattern counts]" << std::endl;
        return 1;
    }

    std::string inputMfPath = argv[1];
    std::string outputPatternPath = argv[2];
    size_t patterCnt = std::stoull(argv[3]);
    
    tau_lambda_index* idx = new tau_lambda_index(inputMfPath);
    idx->gen_patterns(outputPatternPath, patterCnt);

    std::cout << "query pattern generation completed successfully!" << std::endl;
    return 0;
}