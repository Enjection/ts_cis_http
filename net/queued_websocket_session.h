#pragma once

#include <deque>

#include "basic_websocket_session.h"

namespace net
{

class queued_websocket_session;

class websocket_queue
{
public:
    websocket_queue(queued_websocket_session& self);
    void send_binary(boost::asio::const_buffer buffer, std::function<void()> on_write);
    void send_text(boost::asio::const_buffer buffer, std::function<void()> on_write);
    bool is_full();
    bool on_write();
private:
    enum
    {
        limit = 64
    };
    struct message
    {
        bool text;
        boost::asio::const_buffer buffer;
        std::function<void()> on_write;
    };
    queued_websocket_session& self_;
    std::deque<message> messages_;
    void send();
};

class queued_websocket_session
    : public basic_websocket_session
{
public:
    using request_handler_t = std::function<void(
            bool, boost::beast::flat_buffer&, size_t, std::shared_ptr<websocket_queue>)>;
    static void accept_handler(
            boost::asio::ip::tcp::socket&& socket,
            boost::beast::http::request<boost::beast::http::empty_body>&& req,
            request_handler_t handler);
    explicit queued_websocket_session(
            boost::asio::ip::tcp::socket socket,
            request_handler_t handler);
#ifndef NDEBUG
    ~queued_websocket_session();
#endif
    void on_accept_success() override;
    std::shared_ptr<queued_websocket_session> shared_from_this();

    void do_read();

    void on_read(
        boost::beast::error_code ec,
        std::size_t bytes_transferred);

    void on_write(
        boost::beast::error_code ec,
        std::size_t bytes_transferred);

    std::shared_ptr<websocket_queue> get_queue();
private:
    request_handler_t handler_;
    websocket_queue queue_;
    boost::beast::flat_buffer in_buffer_;
    friend class websocket_queue;
};

} // namespace net
