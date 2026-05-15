#include "common/util/sha256.h"

#include <array>
#include <cstring>
#include <sstream>

namespace ddz {
namespace {

constexpr std::array<uint32_t, 64> kTable = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

inline uint32_t RotR(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}

inline uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}

inline uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

inline uint32_t BigSigma0(uint32_t x) {
    return RotR(x, 2) ^ RotR(x, 13) ^ RotR(x, 22);
}

inline uint32_t BigSigma1(uint32_t x) {
    return RotR(x, 6) ^ RotR(x, 11) ^ RotR(x, 25);
}

inline uint32_t SmallSigma0(uint32_t x) {
    return RotR(x, 7) ^ RotR(x, 18) ^ (x >> 3);
}

inline uint32_t SmallSigma1(uint32_t x) {
    return RotR(x, 17) ^ RotR(x, 19) ^ (x >> 10);
}

}  // namespace

std::array<uint8_t, 32> Sha256(const std::string& data) {
    std::array<uint32_t, 8> h = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };

    std::string msg = data;
    const uint64_t bit_len = static_cast<uint64_t>(msg.size()) * 8u;
    msg.push_back(static_cast<char>(0x80));
    while ((msg.size() % 64) != 56) {
        msg.push_back(static_cast<char>(0x00));
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<char>((bit_len >> (i * 8)) & 0xffu));
    }

    std::array<uint32_t, 64> w{};
    for (size_t offset = 0; offset < msg.size(); offset += 64) {
        for (size_t i = 0; i < 16; ++i) {
            const size_t base = offset + i * 4;
            w[i] = (static_cast<uint32_t>(static_cast<uint8_t>(msg[base])) << 24) |
                   (static_cast<uint32_t>(static_cast<uint8_t>(msg[base + 1])) << 16) |
                   (static_cast<uint32_t>(static_cast<uint8_t>(msg[base + 2])) << 8) |
                   (static_cast<uint32_t>(static_cast<uint8_t>(msg[base + 3])));
        }
        for (size_t i = 16; i < 64; ++i) {
            w[i] = SmallSigma1(w[i - 2]) + w[i - 7] + SmallSigma0(w[i - 15]) + w[i - 16];
        }

        uint32_t a = h[0];
        uint32_t b = h[1];
        uint32_t c = h[2];
        uint32_t d = h[3];
        uint32_t e = h[4];
        uint32_t f = h[5];
        uint32_t g = h[6];
        uint32_t hh = h[7];

        for (size_t i = 0; i < 64; ++i) {
            const uint32_t t1 = hh + BigSigma1(e) + Ch(e, f, g) + kTable[i] + w[i];
            const uint32_t t2 = BigSigma0(a) + Maj(a, b, c);
            hh = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }

    std::array<uint8_t, 32> out{};
    for (size_t i = 0; i < h.size(); ++i) {
        out[i * 4] = static_cast<uint8_t>((h[i] >> 24) & 0xffu);
        out[i * 4 + 1] = static_cast<uint8_t>((h[i] >> 16) & 0xffu);
        out[i * 4 + 2] = static_cast<uint8_t>((h[i] >> 8) & 0xffu);
        out[i * 4 + 3] = static_cast<uint8_t>(h[i] & 0xffu);
    }
    return out;
}

std::string Sha256Hex(const std::string& data) {
    const auto bytes = Sha256(data);
    std::ostringstream oss;
    oss.setf(std::ios::hex, std::ios::basefield);
    oss.fill('0');
    for (const uint8_t b : bytes) {
        oss.width(2);
        oss << static_cast<int>(b);
    }
    return oss.str();
}

}  // namespace ddz

