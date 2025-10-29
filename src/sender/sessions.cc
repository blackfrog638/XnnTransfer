#include "asio/awaitable.hpp"
#include "session.h"
#include "single_file_sender.h"
#include "util/data_block.h"
#include "util/network.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>

namespace {
bool is_regular_file(const std::filesystem::directory_entry& entry) {
    std::error_code status_ec;
    const auto status = entry.symlink_status(status_ec);
    if (status_ec) {
        return false;
    }
    return std::filesystem::is_regular_file(status);
}
} // namespace

namespace sender {
void Session::prepare_filepaths() {
    file_paths_.clear();

    std::error_code status_ec;

    for (const auto& filepath : filepaths_) {
        status_ec.clear();
        const auto status = std::filesystem::symlink_status(filepath, status_ec);
        if (status_ec) {
            continue;
        }
        if (std::filesystem::is_regular_file(status)) {
            file_paths_.push_back({filepath.filename(), filepath});
            continue;
        }
        if (!std::filesystem::is_directory(status)) {
            continue;
        }

        std::error_code iter_ec;
        std::filesystem::recursive_directory_iterator
            it(filepath, std::filesystem::directory_options::skip_permission_denied, iter_ec);
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
            file_paths_.push_back({it->path().lexically_relative(filepath), it->path()});
        }
    }
}

void Session::prepare_metadata_request() {
    metadata_request_.set_session_id(session_id_);
    metadata_request_.set_source(util::get_local_ip(executor_.get_io_context()));
    metadata_request_.set_destination(std::string(destination_));

    uint64_t total_size = 0;

    for (auto& file_path : file_paths_) {
        std::unique_ptr<SingleFileSender> sender = std::make_unique<SingleFileSender>(
            executor_, tcp_sender_, session_id_view_, file_path.absolute, file_path.relative);
        total_size += sender->file_info().size();

        // Add file info to metadata request
        auto* file_info = metadata_request_.add_files();
        file_info->CopyFrom(sender->file_info());

        file_senders_.push_back(std::move(sender));
    }
    metadata_request_.set_total_size(total_size);
}

asio::awaitable<void> Session::send() {
    std::string metadata_payload;
    if (!metadata_request_.SerializeToString(&metadata_payload)) {
        co_return;
    }

    ConstDataBlock metadata_block{reinterpret_cast<const std::byte*>(metadata_payload.data()),
                                  metadata_payload.size()};

    co_await tcp_sender_.send(metadata_block);

    for (auto& file_sender : file_senders_) {
        executor_.spawn(file_sender->send());
    }

    co_return;
}

} // namespace sender