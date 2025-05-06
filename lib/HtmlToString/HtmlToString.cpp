#include "HtmlToString.h"

String htmlFileToString(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str().c_str();
}