#pragma once

#include "http_session.h"

class http_handler_interface
{
public:
    ~http_handler_interface() = default;
    virtual void handle_upgrade(
            tcp::socket&& socket,
            http::request<http::empty_body>&& req) const = 0;
    virtual void handle_header(
            http::request<http::empty_body>& req,
            http_session::request_reader& reader,
            http_session::queue& queue) const = 0;
};
