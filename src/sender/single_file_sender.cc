#include "single_file_sender.h"
#include "transfer.pb.h"
#include "util/hash.h"
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace {
ConstDataBlock make_block(const std::string& payload) {
    return {reinterpret_cast<const std::byte*>(payload.data()), payload.size()};
}
} // namespace

namespace sender {
SingleFileSender::SingleFileSender(core::Executor& executor,
                                   core::net::io::TcpSender& tcp_sender,
                                   std::string_view& session_id,
                                   std::filesystem::path& file_path,
                                   std::filesystem::path& relative_path)
    : executor_(executor)
    , tcp_sender_(tcp_sender)
    , session_id_(session_id)
    , relative_path_(relative_path)
    , file_path_(file_path) {
    std::error_code ec;
    file_size_ = std::filesystem::file_size(file_path_, ec);
    if (ec) {
        return;
    }

    chunks_count_ = (file_size_ + kChunkSize - 1) / kChunkSize;
    status_.assign(static_cast<std::size_t>(chunks_count_), ChunkStatus::Waiting);
    bytes_sent_ = 0;

    auto file_hash_hex = util::hash::sha256_file_hex(file_path_);
    if (!file_hash_hex) {
        return;
    }

    file_info_.set_size(file_size_);
    file_info_.set_relative_path(relative_path_.string());
    file_info_.set_hash(*file_hash_hex);
}

asio::awaitable<void> SingleFileSender::send() {
    if (chunks_count_ == 0) {
        co_return;
    }

    std::ifstream input(file_path_, std::ios::binary);
    if (!input) {
        co_return;
    }

    std::vector<std::byte> buffer(static_cast<std::size_t>(kChunkSize));

    for (uint64_t index = 0; index < chunks_count_; ++index) {
        const bool is_last_chunk = (index + 1 == chunks_count_);
        auto chunk_data = load_chunk(input, buffer, index, session_id_, is_last_chunk);
        if (!chunk_data) {
            break;
        }

        executor_.spawn(send_chunk(*chunk_data, index));
    }
    co_return;
}

std::optional<SingleFileSender::ChunkData> SingleFileSender::load_chunk(
    std::ifstream& input,
    std::vector<std::byte>& buffer,
    uint64_t index,
    std::string_view& session_id,
    bool is_last_chunk) {
    const auto slot = static_cast<std::size_t>(index);
    if (slot >= status_.size()) {
        return std::nullopt;
    }

    status_[slot] = ChunkStatus::Reading;
    input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    const auto read_bytes = static_cast<std::size_t>(input.gcount());
    if (read_bytes == 0) {
        status_[slot] = ChunkStatus::Failed;
        return std::nullopt;
    }

    ConstDataBlock chunk_block(buffer.data(), read_bytes);
    auto chunk_hash = util::hash::sha256_hex(chunk_block);
    if (!chunk_hash) {
        status_[slot] = ChunkStatus::Failed;
        return std::nullopt;
    }

    transfer::FileChunkRequest chunk;
    chunk.set_session_id(session_id);
    chunk.set_chunk_index(index);
    chunk.set_data(reinterpret_cast<const char*>(buffer.data()), read_bytes);
    chunk.set_hash(*chunk_hash);
    chunk.set_is_last_chunk(is_last_chunk);

    ChunkData result{};
    if (!chunk.SerializeToString(&result.payload)) {
        status_[slot] = ChunkStatus::Failed;
        return std::nullopt;
    }

    result.size = read_bytes;
    status_[slot] = ChunkStatus::Pending;
    return result;
}

asio::awaitable<void> SingleFileSender::send_chunk(const ChunkData& chunk, uint64_t index) {
    status_[static_cast<std::size_t>(index)] = ChunkStatus::Sending;
    co_await tcp_sender_.send(make_block(chunk.payload));
    status_[static_cast<std::size_t>(index)] = ChunkStatus::Sent;
    bytes_sent_ += chunk.size;
}

} // namespace sender