#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include "../include/k_factor_tree/k_factor_tree.h"

int main(int argc, char* argv[]) {
    if (argc < 6) {
        std::cerr << "Usage: " << argv[0] 
                  << " [input text path] [output mf path] [tau_l] [tau_u] [lambda] [delimiter/terminal symbols](optional)" << std::endl;
        return 1;
    }

    std::string inputTextPath = argv[1];
    std::string outputMfPath = argv[2];
    uint64_t tau_l = std::stod(argv[3]);
    uint64_t tau_u = std::stod(argv[4]);
    uint64_t lambda = std::stod(argv[5]);
    std::string delimiter;
    if (argc >= 7) { delimiter = argv[6]; }

    std::ifstream inputFile(inputTextPath);
    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open input file " << inputTextPath << std::endl;
        return 1;
    }

    std::ofstream outputFile(outputMfPath);
    if (!outputFile.is_open()) {
        std::cerr << "Error: Could not open output file " << outputMfPath << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    std::string intputContent = buffer.str();
    
    // bool arr[256];
    std::map<char, size_t> mp;
    bool exist_0 = false;
    // for (size_t i = 0; i < 256; i++) arr[i] = false;
    for (char c : intputContent) {
        if (c == '\0') { exist_0 = true; }
        mp[c]++;
    }
    std::cout << "existing symbols:\n";
    for (auto kv : mp)
        std::cout << kv.first << "\t" << kv.second << "\n";
    // for (size_t i = 0; i < 256; i++)
    //     if (arr[i])
    //         std::cout << i << "\n";
    std::cout << "end\n";
    std::cout << "constains \\0?: " << exist_0 << "\n";
    std::cout << "delimiter: " << delimiter << "\n";

    k_factor_tree ksf(intputContent, lambda, tau_l, tau_u, delimiter);
    ksf.Serialize_min_factors(outputFile, inputTextPath);

    inputFile.close();
    outputFile.close();

    std::cout << "Minimal factors identification completed successfully!" << std::endl;
    return 0;
}
