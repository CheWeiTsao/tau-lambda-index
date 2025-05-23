#pragma once
#include <random>

namespace util {

    template<typename Sequence>
        using Iter = typename Sequence::iterator;

    std::vector<int> to_int_vector(std::string str) {
        std::vector<int> res;
        for (auto c : str) {
            if (c == '$')
                res.push_back(0);
            else 
                res.push_back(static_cast<int>(c));
        }
        return res;
    }

    template<typename C>
    void print_container(C c, std::ostream& out = std::cout) {
        for (const auto& x : c) {
            out << x << " ";
        }
        out << '\n';
    }

    std::vector<int> randomVectorGenerator(int range, int size) {
        if (range < 1 || size <= 0) {
            throw std::invalid_argument("Invalid range or size");
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(1, range);

        std::vector<int> result;
        result.reserve(size);

        for (int i = 0; i < size - 1; ++i) {
            result.push_back(dist(gen));
        }

        result.push_back(0);

        return result;
    }

    void loop(size_t times, std::function<void()> f) {
        for (size_t i = 0; i < times; ++i) f();
    }

    
    // TODO more general definition of load_from_file
    std::string load_from_file(const std::string& file_name) {
        std::ifstream file(file_name);
        std::string str;
        std::string res;

        int line = 0;
        while (std::getline(file, str)) {
            res += str;
            ++line;
            if (line >= 2) {
                std::cout << "Warning: file " << file_name << " has more than 1 line" << std::endl;
            }
        }
        return res;
    }
}