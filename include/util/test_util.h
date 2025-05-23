// c assert not work, implement by myself

#pragma once
#include <iostream>
#include <string>
#include <stdexcept>

namespace test_util 
{
    void assert_true(bool condition, std::string message = "") {
        if (!condition) {
            throw std::runtime_error("Assertion failed: " + message);
        }
    }
}