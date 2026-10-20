#include "ps5_elf_loader.hpp"
#include <array>
#include <cstring>

namespace ps5::emu {

namespace {

// Official Sony Orbis/Prospero NID Suffix (16 bytes):
// 51 8D 64 A6 35 DE D8 C1 E6 B0 39 B1 C3 E5 52 30
constexpr uint8_t kSonyNidSuffix[16] = {
    0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
    0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30
};

constexpr char kSonyBase64Table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

inline uint32_t Rol32(uint32_t value, uint32_t bits) {
    return (value << bits) | (value >> (32U - bits));
}

// Self-contained RFC 3174 SHA-1 implementation for deterministic Sony NID hashing
std::array<uint8_t, 20> ComputeSha1(const uint8_t* data, size_t len) {
    uint32_t h0 = 0x67452301U;
    uint32_t h1 = 0xEFCDAB89U;
    uint32_t h2 = 0x98BADCFEU;
    uint32_t h3 = 0x10325476U;
    uint32_t h4 = 0xC3D2E1F0U;

    std::vector<uint8_t> msg(data, data + len);
    msg.push_back(0x80U);
    while ((msg.size() % 64U) != 56U) {
        msg.push_back(0x00U);
    }

    uint64_t bit_len = static_cast<uint64_t>(len) * 8ULL;
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<uint8_t>((bit_len >> (i * 8)) & 0xFFU));
    }

    for (size_t chunk = 0; chunk < msg.size(); chunk += 64U) {
        uint32_t w[80]{};
        for (int i = 0; i < 16; ++i) {
            size_t idx = chunk + static_cast<size_t>(i) * 4U;
            w[i] = (static_cast<uint32_t>(msg[idx]) << 24U) |
                   (static_cast<uint32_t>(msg[idx + 1]) << 16U) |
                   (static_cast<uint32_t>(msg[idx + 2]) << 8U) |
                   static_cast<uint32_t>(msg[idx + 3]);
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = Rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1U);
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            uint32_t f = 0, k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999U;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1U;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCU;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6U;
            }
            uint32_t temp = Rol32(a, 5U) + f + e + k + w[i];
            e = d;
            d = c;
            c = Rol32(b, 30U);
            b = a;
            a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    std::array<uint8_t, 20> digest{};
    uint32_t words[5] = { h0, h1, h2, h3, h4 };
    for (int i = 0; i < 5; ++i) {
        digest[i * 4 + 0] = static_cast<uint8_t>((words[i] >> 24U) & 0xFFU);
        digest[i * 4 + 1] = static_cast<uint8_t>((words[i] >> 16U) & 0xFFU);
        digest[i * 4 + 2] = static_cast<uint8_t>((words[i] >> 8U) & 0xFFU);
        digest[i * 4 + 3] = static_cast<uint8_t>(words[i] & 0xFFU);
    }
    return digest;
}

} // namespace

std::string SonyNidHasher::ComputeNid(const std::string& symbol_name) {
    std::vector<uint8_t> payload(symbol_name.begin(), symbol_name.end());
    payload.insert(payload.end(), std::begin(kSonyNidSuffix), std::end(kSonyNidSuffix));

    auto digest = ComputeSha1(payload.data(), payload.size());

    // Sony reverses the first 8 bytes of the SHA-1 digest (little-endian uint64)
    // and encodes those 8 bytes into 11 Base64 characters (using '+' and '-').
    uint8_t rev[9]{};
    for (int i = 0; i < 8; ++i) {
        rev[i] = digest[7 - i];
    }
    rev[8] = 0;

    std::string nid;
    nid.reserve(11);
    nid.push_back(kSonyBase64Table[(rev[0] >> 2U) & 0x3FU]);
    nid.push_back(kSonyBase64Table[((rev[0] & 0x03U) << 4U) | ((rev[1] >> 4U) & 0x0FU)]);
    nid.push_back(kSonyBase64Table[((rev[1] & 0x0FU) << 2U) | ((rev[2] >> 6U) & 0x03U)]);
    nid.push_back(kSonyBase64Table[rev[2] & 0x3FU]);

    nid.push_back(kSonyBase64Table[(rev[3] >> 2U) & 0x3FU]);
    nid.push_back(kSonyBase64Table[((rev[3] & 0x03U) << 4U) | ((rev[4] >> 4U) & 0x0FU)]);
    nid.push_back(kSonyBase64Table[((rev[4] & 0x0FU) << 2U) | ((rev[5] >> 6U) & 0x03U)]);
    nid.push_back(kSonyBase64Table[rev[5] & 0x3FU]);

    nid.push_back(kSonyBase64Table[(rev[6] >> 2U) & 0x3FU]);
    nid.push_back(kSonyBase64Table[((rev[6] & 0x03U) << 4U) | ((rev[7] >> 4U) & 0x0FU)]);
    nid.push_back(kSonyBase64Table[((rev[7] & 0x0FU) << 2U)]);

    return nid;
}

} // namespace ps5::emu
