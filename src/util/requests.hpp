#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

using namespace nlohmann;

enum class RequestType {
    VerificationRequest,
    VerificationResponse,
    FileMetadataBasic,
    FileChunk,
    Serverclosed
};

std::unordered_map<std::string, RequestType> request_type_map{
    {"verification_request", RequestType::VerificationRequest},
    {"verification_response", RequestType::VerificationResponse},
    {"file_metadata_basic", RequestType::FileMetadataBasic},
    {"file_chunk", RequestType::FileChunk},
    {"server_closed", RequestType::Serverclosed}};

inline RequestType find_type(const json& msg) {
    std::string key = msg["type"];
    return request_type_map[key];
}