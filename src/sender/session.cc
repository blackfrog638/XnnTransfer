#include "session.h"
#include "core/executor.h"
#include "transfer.pb.h"
#include "util/data_block.h"
#include "util/hash.h"
#include <cstdint>
#include <spdlog/spdlog.h>
#include <string>

namespace sender {
asio::awaitable<void> Session::start() {
    co_await core::net::io::Session::start();
    metadata_request_.Clear();
    std::uint64_t total_size = 0;
    //!TODO: 这里的每个文件都读取了3次，考虑优化
    prepare_file_paths();

    for (std::size_t i = 0; i < file_paths_.size(); ++i) {
        auto& file_path = file_paths_[i];
        auto* file_info = metadata_request_.add_files();
        file_info->set_relative_path(file_path.relative.string());

        const auto file_size = std::filesystem::file_size(file_path.absolute);

        file_info->set_size(file_size);
        total_size += file_size;

        const auto file_hash = util::hash::sha256_file_hex(file_path.absolute);
        if (file_hash) {
            file_info->set_hash(*file_hash); // 解引用 optional
        }
        file_path.file_index = i;
    }
    metadata_request_.set_total_size(total_size);
    co_await send(metadata_request_);
}

void Session::prepare_file_paths() {
    file_paths_.clear();

    std::error_code status_ec;

    for (const auto& path : paths_) {
        status_ec.clear();
        const auto status = std::filesystem::symlink_status(path, status_ec);
        if (status_ec) {
            continue;
        }
        if (std::filesystem::is_regular_file(status)) {
            file_paths_.emplace_back(path.filename(), path, FilePath::Status::InProgress, 0);
            continue;
        }
        if (!std::filesystem::is_directory(status)) {
            continue;
        }

        std::error_code iter_ec;
        auto it = std::filesystem::recursive_directory_iterator(
            path, std::filesystem::directory_options::skip_permission_denied, iter_ec);
        if (iter_ec) {
            continue;
        }

        for (; it != std::filesystem::recursive_directory_iterator{}; it.increment(iter_ec)) {
            if (iter_ec) {
                break;
            }
            if (!is_regular_file(*it)) {
                continue;
            }
            file_paths_.emplace_back(it->path().lexically_relative(path),
                                     it->path(),
                                     FilePath::Status::InProgress,
                                     0);
        }
    }
}

std::optional<std::size_t> Session::find_file_index(const std::string& relative_path) const {
    for (std::size_t i = 0; i < file_paths_.size(); ++i) {
        if (file_paths_[i].relative.string() == relative_path) {
            return i;
        }
    }
    return std::nullopt;
}

SingleFileSender* Session::find_file_sender(const std::string& relative_path) {
    auto index = find_file_index(relative_path);
    if (!index || *index >= file_senders_.size()) {
        return nullptr;
    }
    return file_senders_[*index].get();
}

asio::awaitable<void> Session::handle_message(const MessageWrapper& message) {
    ConstDataBlock data(reinterpret_cast<const std::byte*>(message.payload().data()),
                        message.payload().size());

    if (message.type() == "transfer.TransferMetadataResponse") {
        transfer::TransferMetadataResponse response;
        if (!util::deserialize(data, response)) {
            co_return;
        }

        if (response.status() == transfer::TransferMetadataResponse_Status_READY) {
            for (const auto& file_path : file_paths_) {
                file_senders_.emplace_back(
                    std::make_unique<SingleFileSender>(executor_,
                                                       *this,
                                                       *metadata_request_.mutable_files(
                                                           static_cast<int>(file_path.file_index)),
                                                       file_path.absolute));
            }
            for (auto& sender : file_senders_) {
                executor_.spawn(sender->send_file());
            }
        } else if (response.status() == transfer::TransferMetadataResponse_Status_FAILURE) {
            spdlog::error(
                "[Session::handle_message] Transfer metadata response indicates failure: {}",
                response.message());
        }
    } else if (message.type() == "transfer.FileInfoResponse") {
        transfer::FileInfoResponse response;
        if (!util::deserialize(data, response)) {
            co_return;
        }
        auto file_index = find_file_index(response.relative_path());
        if (!file_index) {
            spdlog::warn("[Session::handle_message] Received response for unknown file: {}",
                         response.relative_path());
            co_return;
        }

        auto& file_path = file_paths_[*file_index];
        if (response.status() == transfer::FileInfoResponse_Status_SUCCESS) {
            spdlog::info("[Session::handle_message] File info accepted: {}",
                         response.relative_path());
        } else if (response.status() == transfer::FileInfoResponse_Status_SKIPPED) {
            file_path.status = FilePath::Status::Succeeded;
            spdlog::info("[Session::handle_message] File skipped: {}", response.relative_path());
        } else {
            file_path.status = FilePath::Status::Failed;
            spdlog::error("[Session::handle_message] File info error: {} - {}",
                          response.relative_path(),
                          response.message());
        }
    } else if (message.type() == "transfer.FileChunkResponse") {
        transfer::FileChunkResponse response;
        if (!util::deserialize(data, response)) {
            co_return;
        }

        auto* sender = find_file_sender(response.file_relative_path());

        bool success = (response.status() == transfer::FileChunkResponse_Status_RECEIVED);
        sender->update_chunk_status(response.chunk_index(), success);

        if (!success) {
            spdlog::error("[Session::handle_message] Chunk error for file {} chunk {}: {}",
                          response.file_relative_path(),
                          response.chunk_index(),
                          response.message());
        }
    }
    co_return;
}
} // namespace sender