#include "multipart_form_handler.h"

#include "response.h"
#include "beast_ext/multipart_form_body.h"
#include "cis/dirs.h"

namespace http
{

multipart_form_handler::multipart_form_handler(
        std::filesystem::path files_root)
    : files_root_(files_root)
{}

handle_result multipart_form_handler::operator()(
        std::shared_ptr<rights_manager> rights,
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
        if(auto project_rights 
                = rights->check_project_right(ctx.username, project);
                !((!project_rights) 
                || (project_rights && project_rights.value().write)))
        {
            ctx.res_status = beast::http::status::forbidden;
            return handle_result::error;
        }

        reader.async_read_body<multipart_form_body>(
                [&](beast::http::request<multipart_form_body>& req)
                {
                    boost::beast::error_code ec;
                    req.body().set_dir(
                            files_root_ / project / dir, ec);
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
    return handle_result::error;
}

} // namespace http
