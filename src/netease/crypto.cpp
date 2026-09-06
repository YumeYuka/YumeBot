module netease.crypto;

import std;

namespace {

constexpr std::string_view k_eapi_key = "e82ckenh8dichen8";
constexpr std::string_view k_eapi_magic = "36cd479b6b5";

auto rotl8(int x, int n) -> int {
    return ((x << n) | (x >> (8 - n))) & 0xFF;
}

auto gf_mul(int a, int b) -> int {
    int p = 0;
    for (int i = 0; i < 8; ++i) {
        if (b & 1) {
            p ^= a;
        }
        const int hi = a & 0x80;
        a = (a << 1) & 0xFF;
        if (hi != 0) {
            a ^= 0x1B;
        }
        b >>= 1;
    }
    return p;
}

auto gf_pow(int base, int exponent) -> int {
    int result = 1;
    int value = base;
    int exp = exponent;
    while (exp > 0) {
        if (exp & 1) {
            result = gf_mul(result, value);
        }
        value = gf_mul(value, value);
        exp >>= 1;
    }
    return result;
}

auto build_aes_sbox() -> std::array<int, 256> {
    std::array<int, 256> sbox{};
    for (int x = 0; x < 256; ++x) {
        const int inv = x == 0 ? 0 : gf_pow(x, 254);
        sbox[x] = inv ^ rotl8(inv, 1) ^ rotl8(inv, 2) ^ rotl8(inv, 3) ^ rotl8(inv, 4) ^ 0x63;
    }
    return sbox;
}

constexpr std::array<int, 64> k_md5_shifts = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

auto md5_k(int index) -> std::uint32_t {
    return static_cast<std::uint32_t>(std::abs(std::sin(static_cast<double>(index + 1))) * 4294967296.0);
}

auto rotate_left_u32(std::uint32_t value, int bits) -> std::uint32_t {
    return (value << bits) | (value >> (32 - bits));
}

auto aes_encrypt_block(
    const std::uint8_t *input,
    std::uint8_t *output,
    const std::uint8_t *round_keys,
    const std::array<int, 256> &sbox
) -> void {
    int state[16];
    for (int i = 0; i < 16; ++i) {
        state[i] = input[i];
    }

    auto add_round_key = [&](int round) {
        for (int j = 0; j < 16; ++j) {
            state[j] ^= round_keys[16 * round + j];
        }
    };

    add_round_key(0);
    for (int round = 1; round <= 10; ++round) {
        for (int j = 0; j < 16; ++j) {
            state[j] = sbox[state[j]];
        }
        const int temp[16] = {
            state[0], state[5], state[10], state[15],
            state[4], state[9], state[14], state[3],
            state[8], state[13], state[2], state[7],
            state[12], state[1], state[6], state[11],
        };
        for (int j = 0; j < 16; ++j) {
            state[j] = temp[j];
        }
        if (round < 10) {
            for (int column = 0; column < 4; ++column) {
                const int i0 = 4 * column;
                const int x0 = state[i0];
                const int x1 = state[i0 + 1];
                const int x2 = state[i0 + 2];
                const int x3 = state[i0 + 3];
                state[i0] = gf_mul(x0, 2) ^ gf_mul(x1, 3) ^ x2 ^ x3;
                state[i0 + 1] = x0 ^ gf_mul(x1, 2) ^ gf_mul(x2, 3) ^ x3;
                state[i0 + 2] = x0 ^ x1 ^ gf_mul(x2, 2) ^ gf_mul(x3, 3);
                state[i0 + 3] = gf_mul(x0, 3) ^ x1 ^ x2 ^ gf_mul(x3, 2);
            }
        }
        add_round_key(round);
    }
    for (int j = 0; j < 16; ++j) {
        output[j] = static_cast<std::uint8_t>(state[j]);
    }
}

}  // namespace

auto bytes_to_hex(std::span<const std::byte> bytes, bool uppercase) -> std::string {
    static constexpr std::string_view digits_lower = "0123456789abcdef";
    static constexpr std::string_view digits_upper = "0123456789ABCDEF";
    const auto &digits = uppercase ? digits_upper : digits_lower;
    std::string out;
    out.resize(bytes.size() * 2);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        const auto value = static_cast<unsigned char>(bytes[i]);
        out[2 * i] = digits[value >> 4];
        out[2 * i + 1] = digits[value & 0x0F];
    }
    return out;
}

auto md5_digest(std::span<const std::byte> input) -> std::array<std::byte, 16> {
    const auto bit_length = static_cast<std::uint64_t>(input.size()) * 8;
    std::vector<std::uint8_t> padded;
    padded.reserve(input.size() + 72);
    for (const auto byte : input) {
        padded.push_back(static_cast<std::uint8_t>(byte));
    }
    padded.push_back(0x80);
    while (padded.size() % 64 != 56) {
        padded.push_back(0);
    }
    for (int shift = 0; shift < 64; shift += 8) {
        padded.push_back(static_cast<std::uint8_t>((bit_length >> shift) & 0xFF));
    }

    std::uint32_t a0 = 0x67452301;
    std::uint32_t b0 = 0xEFCDAB89;
    std::uint32_t c0 = 0x98BADCFE;
    std::uint32_t d0 = 0x10325476;

    for (std::size_t offset = 0; offset < padded.size(); offset += 64) {
        std::uint32_t m[16];
        for (int i = 0; i < 16; ++i) {
            const auto p = offset + 4 * i;
            m[i] = padded[p]
                | (static_cast<std::uint32_t>(padded[p + 1]) << 8)
                | (static_cast<std::uint32_t>(padded[p + 2]) << 16)
                | (static_cast<std::uint32_t>(padded[p + 3]) << 24);
        }

        std::uint32_t a = a0;
        std::uint32_t b = b0;
        std::uint32_t c = c0;
        std::uint32_t d = d0;
        for (int i = 0; i < 64; ++i) {
            std::uint32_t f = 0;
            int g = 0;
            if (i < 16) {
                f = (b & c) | ((~b) & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | ((~d) & c);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            } else {
                f = c ^ (b | (~d));
                g = (7 * i) % 16;
            }
            f = f + a + md5_k(i) + m[g];
            a = d;
            d = c;
            c = b;
            b = b + rotate_left_u32(f, k_md5_shifts[i]);
        }
        a0 += a;
        b0 += b;
        c0 += c;
        d0 += d;
    }

    std::array<std::byte, 16> digest{};
    const std::uint32_t words[4] = {a0, b0, c0, d0};
    for (int i = 0; i < 4; ++i) {
        for (int shift = 0; shift < 32; shift += 8) {
            digest[i * 4 + shift / 8] = static_cast<std::byte>((words[i] >> shift) & 0xFF);
        }
    }
    return digest;
}

auto md5_hex(std::span<const std::byte> input) -> std::string {
    return bytes_to_hex(md5_digest(input), false);
}

auto aes128_ecb_encrypt(std::span<const std::byte> data, std::string_view key) -> std::vector<std::byte> {
    if (key.size() != 16) {
        throw std::invalid_argument("AES-128 key must be 16 bytes");
    }

    const auto sbox = build_aes_sbox();
    std::array<std::uint8_t, 176> round_keys{};
    for (std::size_t i = 0; i < 16; ++i) {
        round_keys[i] = static_cast<std::uint8_t>(key[i]);
    }

    constexpr std::array<int, 10> rcon = {1, 2, 4, 8, 16, 32, 64, 128, 27, 54};
    for (int i = 16; i < 176; i += 4) {
        int t0 = round_keys[i - 4];
        int t1 = round_keys[i - 3];
        int t2 = round_keys[i - 2];
        int t3 = round_keys[i - 1];
        if (i % 16 == 0) {
            const int r0 = sbox[t1] ^ rcon[i / 16 - 1];
            t0 = r0;
            t1 = sbox[t2];
            t2 = sbox[t3];
            t3 = sbox[round_keys[i - 4]];
        }
        round_keys[i] = round_keys[i - 16] ^ static_cast<std::uint8_t>(t0);
        round_keys[i + 1] = round_keys[i - 15] ^ static_cast<std::uint8_t>(t1);
        round_keys[i + 2] = round_keys[i - 14] ^ static_cast<std::uint8_t>(t2);
        round_keys[i + 3] = round_keys[i - 13] ^ static_cast<std::uint8_t>(t3);
    }

    const auto pad = 16 - (data.size() % 16);
    std::vector<std::byte> padded(data.begin(), data.end());
    padded.insert(padded.end(), pad, static_cast<std::byte>(pad));

    std::vector<std::byte> output;
    output.resize(padded.size());
    for (std::size_t offset = 0; offset < padded.size(); offset += 16) {
        std::uint8_t block_in[16];
        std::uint8_t block_out[16];
        for (int i = 0; i < 16; ++i) {
            block_in[i] = static_cast<std::uint8_t>(padded[offset + i]);
        }
        aes_encrypt_block(block_in, block_out, round_keys.data(), sbox);
        for (int i = 0; i < 16; ++i) {
            output[offset + i] = static_cast<std::byte>(block_out[i]);
        }
    }
    return output;
}

auto eapi_params(std::string_view path, std::string_view json_payload) -> std::string {
    const auto digest_input = std::string{"nobody"} + std::string{path} + "use" + std::string{json_payload} + "md5forencrypt";
    const auto digest_bytes = std::as_bytes(std::span<const char>{digest_input.data(), digest_input.size()});
    const auto digest = md5_hex(digest_bytes);
    const auto text = std::string{path} + "-" + std::string{k_eapi_magic} + "-" + std::string{json_payload}
        + "-" + std::string{k_eapi_magic} + "-" + digest;
    const auto text_bytes = std::as_bytes(std::span<const char>{text.data(), text.size()});
    const auto key_bytes = std::as_bytes(std::span<const char>{k_eapi_key.data(), k_eapi_key.size()});
    const auto encrypted = aes128_ecb_encrypt(text_bytes, k_eapi_key);
    return "params=" + bytes_to_hex(encrypted, true);
}
