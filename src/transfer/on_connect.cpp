#include "transfer_manager.hpp"

void TransferManager::on_connect(beast::error_code ec,
                                  net::ip::tcp::resolver::results_type::endpoint_type ep) {
    if(ec) {
        handle_error(ec, "Connect failed");
        return;
    }
    beast::get_lowest_layer(ws_).expires_never();
    ws_.set_option(beast::websocket::stream_base::timeout::suggested(beast::role_type::client));
    ws_.set_option(beast::websocket::stream_base::decorator(
        [](beast::websocket::request_type& req) {
            req.set(beast::http::field::user_agent,
                std::string(BOOST_BEAST_VERSION_STRING) + " TransferManager");
        }));
    ws_.async_handshake(host_, "/",
        beast::bind_front_handler(&TransferManager::on_handshake, shared_from_this()));
}