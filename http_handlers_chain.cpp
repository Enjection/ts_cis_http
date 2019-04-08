#include "http_handlers_chain.h"

#include "net/listener.h"
#include "generic_error.h"

http_handlers_chain::http_handlers_chain()
{}

void http_handlers_chain::append_handler(const handler_t& handler)
{
    handlers_.push_back(handler);
}

void http_handlers_chain::append_ws_handler(const ws_handler_t& handler)
{
    ws_handlers_.push_back(handler);
}

void http_handlers_chain::set_error_handler(const handler_t& handler)
{
    error_handler_ = handler;
}

void http_handlers_chain::set_ws_error_handler(const ws_handler_t& handler)
{
    ws_error_handler_ = handler;
}

void http_handlers_chain::listen(boost::asio::io_context& ioc, const tcp::endpoint& endpoint)
{
    auto accept_handler = 
    [self = shared_from_this()](tcp::socket&& socket){
        std::make_shared<http_session>(
            std::move(socket),
            self)->run();
    };
    auto l = std::make_shared<listener>(
        ioc,
        accept_handler);
    beast::error_code ec;
    l->listen(endpoint, ec);
    if(ec)
    {
        throw generic_error(ec.message());
    }
    l->run();
}

void http_handlers_chain::handle_header(
            request_header_t& req,
            http_session::request_reader& reader,
            http_session::queue& queue) const
{
    context_t ctx{};
    for(auto& handler : handlers_)
    {
        auto result = handler(req, reader, queue, ctx);
        switch(result)
        {
            case handle_result::next:
                break;
            case handle_result::done:
                return;
            case handle_result::error:
                error_handler_(req, reader, queue, ctx);
                return;
        };
    }
}

void http_handlers_chain::handle_upgrade(
        tcp::socket&& socket,
        request_header_t&& req) const
{
    context_t ctx{};
    for(auto& handler : ws_handlers_)
    {
        auto result = handler(req, socket, ctx);
        switch(result)
        {
            case handle_result::next:
                break;
            case handle_result::done:
                return;
            case handle_result::error:
                ws_error_handler_(req, socket, ctx);
                return;
        };
    }
};
