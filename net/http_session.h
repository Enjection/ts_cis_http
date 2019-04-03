#pragma once

#include <memory>
#include <functional>
#include <deque>

#include <boost/beast.hpp>
#include <boost/asio.hpp>

#include "fail.h"

class web_app;

namespace beast = boost::beast;                 // from <boost/beast.hpp>
namespace http = beast::http;                   // from <boost/beast/http.hpp>
namespace net = boost::asio;                    // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>

class http_session : public std::enable_shared_from_this<http_session>
{
    class http_session_queue
    {
        enum
        {
            // Maximum number of responses we will queue
            limit = 8
        };

        // The type-erased, saved work item
        struct work
        {
            virtual ~work() = default;
            virtual void operator()() = 0;
        };

        http_session& self_;
        std::deque<std::unique_ptr<work>> items_;
    public:
        explicit http_session_queue(http_session& self)
            : self_(self)
        {
            static_assert(limit > 0, "queue limit must be positive");
        }

        // Returns `true` if we have reached the queue limit
        bool is_full() const
        {
            return items_.size() >= limit;
        }

        // Called when a message finishes sending
        // Returns `true` if the caller should initiate a read
        bool on_write()
        {
            BOOST_ASSERT(!items_.empty());
            auto const was_full = is_full();
            items_.erase(items_.begin());
            if(! items_.empty())
            {
                (*items_.front())();
            }
            return was_full;
        }

        template<bool isRequest, class Body, class Fields>
        void send(http::message<isRequest, Body, Fields>&& msg)
        {
            // This holds a work item
            struct work_impl : work
            {
                http_session& self_;
                http::message<isRequest, Body, Fields> msg_;

                work_impl(
                    http_session& self,
                    http::message<isRequest, Body, Fields>&& msg)
                    : self_(self)
                    , msg_(std::move(msg))
                {
                }

                void
                operator()()
                {
                    http::async_write(
                        self_.socket_,
                        msg_,
                        net::bind_executor(
                            self_.strand_,
                            std::bind(
                                &http_session::on_write,
                                self_.shared_from_this(),
                                std::placeholders::_1,
                                msg_.need_eof())));
                }
            };

            // Allocate and store the work
            items_.push_back(
                boost::make_unique<work_impl>(self_, std::move(msg)));

            // If there was no previous work, start this one
            if(items_.size() == 1)
            {
                (*items_.front())();
            }
        }
    };
    class parser_handler_base
    {
    };
    template <class Body>
    class parser_handler : public parser_handler_base
    {
    public:
        parser_handler() = default;
        template <class OtherBody>
        parser_handler(
                parser_handler<OtherBody>&& other,
                std::function<void(http::request<http::string_body>&&, http_session_queue&)> cb)
            : parser_(std::move(other.parser_))
            , cb_(cb)
        {}
        http::request_parser<Body> parser_;
        std::function<void(http::request<http::string_body>&&, http_session_queue&)> cb_;
    };
    tcp::socket socket_;
    net::strand<
        net::io_context::executor_type> strand_;
    net::steady_timer timer_;
    beast::flat_buffer buffer_;
    std::shared_ptr<web_app const> app_;
    //std::unique_ptr<http::request_parser<http::empty_body>> req_parser_;
    std::unique_ptr<parser_handler_base> req_parser_;
    http_session_queue queue_;
    friend class http_session_queue;
public:
    using queue = http_session_queue;
    // Take ownership of the socket
    explicit http_session(
        tcp::socket socket,
        const std::shared_ptr<web_app const>& app);

    // Start the asynchronous operation
    void run();

    void do_read_header();
   
    template <class Body>
    void do_read_body()
    {
        http::async_read(socket_, buffer_,
            static_cast<parser_handler<Body>*>(req_parser_.get())->parser_,
            net::bind_executor(
                strand_,
                std::bind(
                    &http_session::on_read_body<Body>,
                    shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2)));
    }

    // Called when the timer expires.
    void on_timer(beast::error_code ec);

    void on_read_header(beast::error_code ec);
    
    template <class Body>
    void on_read_body(beast::error_code ec, std::size_t bytes_transferred)
    {
        // Happens when the timer closes the socket
        if(ec == net::error::operation_aborted)
        {
            return;
        }

        // This means they closed the connection
        if(ec == http::error::end_of_stream)
        {
            return do_close();
        }

        if(ec)
        {
            return fail(ec, "read");
        }
        
        auto* req_parser = static_cast<parser_handler<Body>*>(req_parser_.get());

        req_parser->cb_(
                std::move(req_parser->parser_.release()),
                queue_);
        
        // If we aren't at the queue limit, try to pipeline another request
        if(!queue_.is_full())
        {
            do_read_header();
        }
    }
    
    void on_write(beast::error_code ec, bool close);

    void do_close();
};
