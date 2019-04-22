#include "multipart_form_handler.h"

#include "response.h"
#include "beast_ext/multipart_form_body.h"
#include "cis/dirs.h"

namespace http
{

handle_result multipart_form_handler::operator()(
        beast::http::request<beast::http::empty_body>& req,
        request_context& ctx,
        net::http_session::request_reader& reader,
        net::http_session::queue& queue,
        const std::string& project,
        const std::string& dir)
{
    if(req.method() == beast::http::verb::post
            && req[beast::http::field::content_type].find("multipart/form-data") == 0)
    {
        //check project rights
        std::string boundary;
        auto boundary_begin = req[beast::http::field::content_type].find("=");
        if(boundary_begin != req[beast::http::field::content_type].npos)
        {
            boundary = req[beast::http::field::content_type].substr(
                    boundary_begin + 1,
                    req[beast::http::field::content_type].size());
        }
        reader.async_read_body<multipart_form_body>(
                [&](beast::http::request<multipart_form_body>& req)
                {
                    boost::beast::error_code ec;
                    req.body().set_boundary(boundary);
                    std::string fdir = cis::get_root_dir();
                    fdir = fdir + cis::projects + "/" + project + "/" + dir;
                    req.body().set_dir(fdir, ec);
                },
                [ctx](
                    beast::http::request<multipart_form_body>&& req,
                    net::http_session::queue& queue)
                {
                    beast::http::response<beast::http::empty_body> res{
                        beast::http::status::ok,
                        req.version()};
                        res.set(beast::http::field::server, BOOST_BEAST_VERSION_STRING);
                        res.set(beast::http::field::content_type, "text/html");
                        res.keep_alive(req.keep_alive());
                    queue.send(std::move(res));
                });
        return handle_result::done;
    }
    ctx.res_status = beast::http::status::not_found;
    reader.done();
    return handle_result::error;
}

} // namespace http
