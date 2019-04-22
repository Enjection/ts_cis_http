#pragma once

#include "net/http_session.h"
#include "handle_result.h"
#include "request_context.h"

namespace beast = boost::beast;

namespace http
{

//TODO write full implementation
class multipart_form_handler
{
public:
    handle_result operator()(
            beast::http::request<beast::http::empty_body>& req,
            request_context& ctx,
            net::http_session::request_reader& reader,
            net::http_session::queue& queue,
            const std::string& project,
            const std::string& dir);
};

} // namespace http
