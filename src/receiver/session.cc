#include "session.h"
#include "asio/awaitable.hpp"
#include "single_file_receiver.h"
#include "util/data_block.h"
#include <spdlog/spdlog.h>

namespace receiver {

asio::awaitable<void> Session::start() {
    co_await core::net::io::Session::start();
}

asio::awaitable<void> Session::handle_message(const MessageWrapper& message) {
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
        spdlog::info("[receiver::Session] Received chunk for file: {}, chunk_index: {}, data_size: "
                     "{}, is_last: {}",
                     request.file_relative_path(),
                     request.chunk_index(),
                     request.data().size(),
                     request.is_last_chunk());
        co_await handle_file_chunk(request);
        co_return;
    }
    co_return;
}

asio::awaitable<void> Session::handle_metadata(const transfer::TransferMetadataRequest& request) {
    spdlog::info("[receiver::Session] Received metadata request: {} files, total_size={}",
                 request.files_size(),
                 request.total_size());

    bool ok = true;
    std::vector<std::unique_ptr<SingleFileReceiver>> receivers;
    receivers.reserve(static_cast<size_t>(request.files_size()));

    for (const auto& f : request.files()) {
        auto r = std::make_unique<SingleFileReceiver>(f.relative_path(), f.hash(), f.size());
        if (!r->is_valid()) {
            spdlog::error("[receiver::Session] Failed to prepare receiver for {}",
                          f.relative_path());
            ok = false;
            break;
        }
        receivers.emplace_back(std::move(r));
    }

    transfer::TransferMetadataResponse response;
    if (!ok) {
        response.set_status(transfer::TransferMetadataResponse::FAILURE);
        response.set_message("Failed to prepare storage for files");
        co_await send(response);
        co_return;
    }

    for (auto& r : receivers) {
        receivers_map_.emplace(r->relative_path(), std::move(r));
    }

    response.set_status(transfer::TransferMetadataResponse::READY);
    response.set_message("Ready to receive");
    co_await send(response);

    for (const auto& f : request.files()) {
        transfer::FileInfoResponse file_resp;
        file_resp.set_relative_path(f.relative_path());
        if (receivers_map_.find(f.relative_path()) != receivers_map_.end()) {
            file_resp.set_status(transfer::FileInfoResponse_Status_SUCCESS);
            file_resp.set_message("Accepted");
        } else {
            file_resp.set_status(transfer::FileInfoResponse_Status_ERROR);
            file_resp.set_message("Receiver not prepared");
        }
        co_await send(file_resp);
    }

    co_return;
}

asio::awaitable<void> Session::handle_file_chunk(const transfer::FileChunkRequest& request) {
    const auto& rel = request.file_relative_path();
    auto it = receivers_map_.find(rel);
    transfer::FileChunkResponse chunk_resp;
    chunk_resp.set_file_relative_path(rel);
    chunk_resp.set_chunk_index(request.chunk_index());

    if (it == receivers_map_.end()) {
        chunk_resp.set_status(transfer::FileChunkResponse_Status::FileChunkResponse_Status_ERROR);
        chunk_resp.set_message("Unknown file");
        co_await send(chunk_resp);
        co_return;
    }

    auto& receiver = *it->second;
    bool success = receiver.handle_chunk(request);
    if (success) {
        chunk_resp.set_status(transfer::FileChunkResponse_Status_RECEIVED);
        chunk_resp.set_message("OK");
    } else {
        chunk_resp.set_status(transfer::FileChunkResponse_Status_ERROR);
        chunk_resp.set_message("Chunk handling failed");
    }
    co_await send(chunk_resp);

    if (request.is_last_chunk()) {
        auto [ok, expected_hash, actual_hash] = it->second->finalize_and_verify();
        if (!ok) {
            spdlog::error("[receiver::Session] Hash mismatch for {}: expected={}, got={}",
                          rel,
                          expected_hash.empty() ? std::string("(none)") : expected_hash,
                          actual_hash.empty() ? std::string("(none)") : actual_hash);
            // 可选：在这里发送一个 FileInfoResponse 表示 ERROR（目前不改变协议）
        } else {
            spdlog::info("[receiver::Session] File {} received and hash OK", rel);
        }
    }

    co_return;
}

} // namespace receiver
