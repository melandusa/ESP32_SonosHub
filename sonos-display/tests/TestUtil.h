#pragma once

#include <fstream>
#include <sstream>
#include <string>

inline std::string readFixture(const std::string& name) {
    std::ifstream file(std::string(SONOS_FIXTURE_DIR) + "/" + name, std::ios::binary);
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}
