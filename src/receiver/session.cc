#include "session.h"
#include "asio/awaitable.hpp"
#include "util/data_block.h"
#include <algorithm>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace receiver {

asio::awaitable<void> Session::start() {
    co_await core::net::io::Session::start();
}

asio::awaitable<void> Session::handle_message(const MessageWrapper& message) {
    spdlog::debug("[receiver::Session] Received message type: {}", message.type());
    ConstDataBlock data(reinterpret_cast<const std::byte*>(message.payload().data()),
                        message.payload().size());

    if (message.type() == "transfer.TransferMetadataRequest") {
        transfer::TransferMetadataRequest request;
        if (!util::deserialize(data, request)) {
            spdlog::error("[receiver::Session] Failed to deserialize TransferMetadataRequest");
            co_return;
        }
        co_await handle_metadata(request);
        co_return;
    }
    if (message.type() == "transfer.FileChunkRequest") {
        transfer::FileChunkRequest request;
        if (!util::deserialize(data, request)) {
            spdlog::error("[receiver::Session] Failed to deserialize FileChunkRequest");
            co_return;
        }
        spdlog::info(
            "[receiver::Session] Received chunk for file: {}, chunk_index: {}, data_size: {}",
            request.file_relative_path(),
            request.chunk_index(),
            request.data().size());
        co_await handle_file_chunk(request);
        co_return;
    }
    co_return;
}

asio::awaitable<void> Session::handle_metadata(const transfer::TransferMetadataRequest& request) {
    receivers_map_.clear();
    file_paths_.clear();
    completed_files_ = 0;

    if (request.files().empty()) {
        transfer::TransferMetadataResponse response;
        response.set_status(transfer::TransferMetadataResponse::READY);
        co_await send(response);
        co_return;
    }

    bool prepare_failed = false;
    std::string failure_message;

    for (int i = 0; i < request.files_size(); ++i) {
        const auto& file_info = request.files(i);

        FilePath file_path;
        file_path.relative = std::filesystem::path(file_info.relative_path());
        file_path.absolute = std::filesystem::path(save_dir_) / file_path.relative;
        file_path.file_index = static_cast<std::size_t>(i);

        auto receiver = std::make_unique<SingleFileReceiver>(file_info.relative_path(),
                                                             file_info.hash(),
                                                             file_info.size());

        if (!receiver->prepare_storage(file_path.absolute)) {
            prepare_failed = true;
            failure_message = "Failed to prepare file: " + file_info.relative_path();
            spdlog::error("[receiver::Session] Failed to prepare storage for {} -> {}",
                          file_info.relative_path(),
                          file_path.absolute.string());
            break;
        }

        auto [it, inserted] = receivers_map_.emplace(file_info.relative_path(), std::move(receiver));
        if (!inserted) {
            prepare_failed = true;
            failure_message = "Duplicated file path: " + file_info.relative_path();
            spdlog::error("[receiver::Session] Duplicate file path in metadata: {}",
                          file_info.relative_path());
            break;
        }
        file_paths_.push_back(std::move(file_path));
    }

    transfer::TransferMetadataResponse response;
    if (prepare_failed) {
        receivers_map_.clear();
        file_paths_.clear();
        response.set_status(transfer::TransferMetadataResponse::FAILURE);
        response.set_message(failure_message);
    } else {
        response.set_status(transfer::TransferMetadataResponse::READY);
        spdlog::info("[receiver::Session] Prepared to receive {} files", file_paths_.size());
    }

    co_await send(response);
    co_return;
}

asio::awaitable<void> Session::handle_file_chunk(const transfer::FileChunkRequest& request) {
    transfer::FileChunkResponse chunk_response;
    chunk_response.set_file_relative_path(request.file_relative_path());
    chunk_response.set_chunk_index(request.chunk_index());

    auto receiver_it = receivers_map_.find(request.file_relative_path());
    if (receiver_it == receivers_map_.end()) {
        chunk_response.set_status(transfer::FileChunkResponse::FAILURE);
        chunk_response.set_message("Unknown file");
        co_await send(chunk_response);
        co_return;
    }

    auto& receiver = *receiver_it->second;
    if (!receiver.handle_chunk(request)) {
        chunk_response.set_status(transfer::FileChunkResponse::FAILURE);
        chunk_response.set_message("Chunk validation failed");
        co_await send(chunk_response);
        co_return;
    }

    chunk_response.set_status(transfer::FileChunkResponse::RECEIVED);
    co_await send(chunk_response);

    if (!receiver.is_complete()) {
        spdlog::debug("[receiver::Session] File {} not complete yet, completed_chunks={}",
                      receiver.relative_path(), receiver.completed_chunks());
        co_return;
    }

    spdlog::debug("[receiver::Session] File {} is complete, finalizing...", receiver.relative_path());
    auto [file_ok, expected_hash, actual_hash] = receiver.finalize_and_verify();

    transfer::FileInfoResponse info_response;
    info_response.set_relative_path(receiver.relative_path());
    if (file_ok) {
        info_response.set_status(transfer::FileInfoResponse::SUCCESS);
        spdlog::info("[receiver::Session] Completed file {}", receiver.relative_path());
    } else {
        info_response.set_status(transfer::FileInfoResponse::FAILURE);
        if (!expected_hash.empty() && !actual_hash.empty() && expected_hash != actual_hash) {
            info_response.set_message("Hash mismatch");
        } else if (!expected_hash.empty() && actual_hash.empty()) {
            info_response.set_message("Failed to compute file hash");
        } else {
            info_response.set_message("File incomplete");
        }
        spdlog::warn("[receiver::Session] File {} verification failed", receiver.relative_path());
    }

    co_await send(info_response);

    auto file_entry = std::find_if(file_paths_.begin(),
                                   file_paths_.end(),
                                   [&](const FilePath& path) {
                                       return path.relative.string() == receiver.relative_path();
                                   });
    if (file_entry != file_paths_.end()) {
        file_entry->status = file_ok ? FilePath::Status::Succeeded : FilePath::Status::Failed;
    }

    ++completed_files_;
    receivers_map_.erase(receiver_it);

    const bool all_done = completed_files_ >= file_paths_.size();
    const bool all_success = std::all_of(file_paths_.begin(),
                                         file_paths_.end(),
                                         [](const FilePath& path) {
                                             return path.status == FilePath::Status::Succeeded;
                                         });

    if (all_done) {
        transfer::TransferMetadataResponse completion_response;
        if (all_success) {
            completion_response.set_status(transfer::TransferMetadataResponse::SUCCESS);
        } else {
            completion_response.set_status(transfer::TransferMetadataResponse::FAILURE);
            completion_response.set_message("One or more files failed during transfer");
        }
        co_await send(completion_response);
        
        // 停止会话
        stop();
    }
    co_return;
}

} // namespace receiver
