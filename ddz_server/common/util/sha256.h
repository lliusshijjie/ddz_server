#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace ddz {

std::array<uint8_t, 32> Sha256(const std::string& data);
std::string Sha256Hex(const std::string& data);

}  // namespace ddz

