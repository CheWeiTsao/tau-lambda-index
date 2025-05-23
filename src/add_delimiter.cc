#include <iostream>
#include <fstream>
#include <string>

std::string insert_separator(const std::string& text, size_t length, unsigned char sep_char) {
    std::string result;
    for (size_t i = 0; i < text.size(); ++i) {
        result += text[i];
        if ((i + 1) % length == 0 && (i + 1) < text.size()) {
            result += static_cast<char>(sep_char);
        }
    }
    if (result.back() != static_cast<char>(sep_char)) result += static_cast<char>(sep_char);
    return result;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " [input_file] [output_file] [length] [separator_code]" << std::endl;
        return 1;
    }

    std::string input_file = argv[1];
    std::string output_file = argv[2];
    size_t length = std::stoul(argv[3]);
    int sep_code = std::stoi(argv[4]);

    if (sep_code < 0 || sep_code > 255) {
        std::cerr << "Error: separator_code must be between 0 and 255." << std::endl;
        return 1;
    }

    std::ifstream infile(input_file, std::ios::binary);
    if (!infile) {
        std::cerr << "Error: cannot open input file." << std::endl;
        return 1;
    }

    std::string text((std::istreambuf_iterator<char>(infile)),
                     std::istreambuf_iterator<char>());
    infile.close();

    char separator = static_cast<char>(sep_code);
    std::string output = insert_separator(text, length, separator);

    std::ofstream outfile(output_file, std::ios::binary);
    if (!outfile) {
        std::cerr << "Error: cannot open output file." << std::endl;
        return 1;
    }

    outfile << output;
    outfile.close();

    std::cout << "Output written to " << output_file << std::endl;
    return 0;
}
