#include "file_handler.h"

#include "file_util.h"
#include "response.h"

file_handler::file_handler(const std::string& doc_root)
    : doc_root_(doc_root)
{}

handle_result file_handler::operator()(
        http::request<http::empty_body>& req,
        net::http_session::request_reader& reader,
        net::http_session::queue& queue,
        request_context& ctx)
{
    return single_file(req, reader, queue, ctx, req.target());
}

handle_result file_handler::single_file(
        http::request<http::empty_body>& req,
        net::http_session::request_reader& reader,
        net::http_session::queue& queue,
        request_context& /*ctx*/,
        std::string_view path)
{
        std::string full_path = path_cat(doc_root_, path);

        
        beast::error_code ec;
        http::file_body::value_type body;
        body.open(full_path.c_str(), beast::file_mode::scan, ec);
        if(ec == beast::errc::no_such_file_or_directory)
        {
            queue.send(response::not_found(std::move(req)));
            return handle_result::done;
        }

        // Handle an unknown error
        if(ec)
        {
            queue.send(response::server_error(std::move(req), ec.message()));
            return handle_result::done;
        }

        // Cache the size since we need it after the move
        auto const size = body.size();
        // Respond to GET request
        http::response<http::file_body> res{
            std::piecewise_construct,
            std::make_tuple(std::move(body)),
            std::make_tuple(http::status::ok, req.version())};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, mime_type(full_path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        queue.send(std::move(res));
        return handle_result::done;
}
