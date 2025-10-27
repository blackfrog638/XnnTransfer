#include "util/hash.h"
#include <fstream>
#include <memory>
#include <openssl/evp.h>
#include <spdlog/spdlog.h>
#include <vector>

namespace util::hash {
namespace {
struct MdCtxDeleter {
    void operator()(EVP_MD_CTX* ctx) const noexcept {
        if (ctx != nullptr) {
            EVP_MD_CTX_free(ctx);
        }
    }
};

using MdCtxPtr = std::unique_ptr<EVP_MD_CTX, MdCtxDeleter>;

[[nodiscard]] std::optional<MdCtxPtr> make_md_ctx() {
    MdCtxPtr ctx{EVP_MD_CTX_new()};
    if (!ctx) {
        spdlog::error("EVP_MD_CTX_new failed");
        return std::nullopt;
    }
    return ctx;
}

[[nodiscard]] std::string to_hex(std::span<const std::byte> data) {
    constexpr char kDigits[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(data.size() * 2);
    for (const auto value : data) {
        const auto byte = std::to_integer<unsigned int>(value);
        hex.push_back(kDigits[(byte >> 4U) & 0x0FU]);
        hex.push_back(kDigits[byte & 0x0FU]);
    }
    return hex;
}
} // namespace

std::optional<std::array<std::byte, kSha256Size>> sha256(ConstDataBlock data) {
    auto ctx_opt = make_md_ctx();
    if (!ctx_opt) {
        return std::nullopt;
    }
    auto& ctx = *ctx_opt;

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        spdlog::error("EVP_DigestInit_ex failed");
        return std::nullopt;
    }
    if (!data.empty()) {
        if (EVP_DigestUpdate(ctx.get(), data.data(), data.size()) != 1) {
            spdlog::error("EVP_DigestUpdate failed");
            return std::nullopt;
        }
    }

    std::array<std::byte, kSha256Size> digest{};
    unsigned int digest_size = 0;
    if (EVP_DigestFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(digest.data()), &digest_size)
        != 1) {
        spdlog::error("EVP_DigestFinal_ex failed");
        return std::nullopt;
    }
    if (digest_size != digest.size()) {
        spdlog::error("Unexpected SHA-256 digest size: {}", digest_size);
        return std::nullopt;
    }
    return digest;
}

std::optional<std::string> sha256_hex(ConstDataBlock data) {
    const auto digest = sha256(data);
    if (!digest) {
        return std::nullopt;
    }
    return to_hex(std::span<const std::byte>(digest->data(), digest->size()));
}

std::optional<std::array<std::byte, kSha256Size>> sha256_file(
    const std::filesystem::path& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        spdlog::error("Failed to open file: {}", file_path.string());
        return std::nullopt;
    }

    auto ctx_opt = make_md_ctx();
    if (!ctx_opt) {
        return std::nullopt;
    }
    auto& ctx = *ctx_opt;

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        spdlog::error("EVP_DigestInit_ex failed");
        return std::nullopt;
    }

    constexpr std::size_t kBufferSize = 64 * 1024; // 64KB buffer
    std::vector<std::byte> buffer(kBufferSize);

    while (file) {
        file.read(reinterpret_cast<char*>(buffer.data()), kBufferSize);
        const auto bytes_read = static_cast<std::size_t>(file.gcount());

        if (bytes_read > 0) {
            if (EVP_DigestUpdate(ctx.get(), buffer.data(), bytes_read) != 1) {
                spdlog::error("EVP_DigestUpdate failed");
                return std::nullopt;
            }
        }
    }

    if (file.bad()) {
        spdlog::error("Error reading file: {}", file_path.string());
        return std::nullopt;
    }

    std::array<std::byte, kSha256Size> digest{};
    unsigned int digest_size = 0;
    if (EVP_DigestFinal_ex(ctx.get(), reinterpret_cast<unsigned char*>(digest.data()), &digest_size)
        != 1) {
        spdlog::error("EVP_DigestFinal_ex failed");
        return std::nullopt;
    }
    if (digest_size != digest.size()) {
        spdlog::error("Unexpected SHA-256 digest size: {}", digest_size);
        return std::nullopt;
    }
    return digest;
}

std::optional<std::string> sha256_file_hex(const std::filesystem::path& file_path) {
    const auto digest = sha256_file(file_path);
    if (!digest) {
        return std::nullopt;
    }
    return to_hex(std::span<const std::byte>(digest->data(), digest->size()));
}
} // namespace util::hash
