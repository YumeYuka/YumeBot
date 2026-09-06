export module netease.crypto;

import std;

export auto md5_hex(std::span<const std::byte> input) -> std::string;

export auto md5_digest(std::span<const std::byte> input) -> std::array<std::byte, 16>;

export auto aes128_ecb_encrypt(std::span<const std::byte> data, std::string_view key) -> std::vector<std::byte>;

export auto eapi_params(std::string_view path, std::string_view json_payload) -> std::string;

export auto bytes_to_hex(std::span<const std::byte> bytes, bool uppercase = false) -> std::string;
