#include "child_process.h"

#include "cis_dirs.h"
#include "file_util.h"

child_process::child_process(
        boost::asio::io_context& ctx,
        boost::process::environment env)
    : ctx_(ctx)
    , env_(env)
    , start_dir_(path_cat(cis::get_root_dir(), cis::CORE))
{
    buffer_.resize(1024);
}

child_process::~child_process()
{
    if(proc_ != nullptr)
    {
        delete proc_;
    }
}

void child_process::run(
        const std::string& programm,
        std::vector<std::string> args,
        std::function<void(int, std::vector<char>&, const std::error_code&)> cb)
{
    namespace bp = boost::process;
    auto self = shared_from_this();
    proc_ = new bp::child(
        bp::search_path(programm),
        env_,
        ctx_,
        //strand_.context(), //TODO
        bp::std_out > boost::asio::buffer(buffer_),
        bp::start_dir = start_dir_.c_str(),
        bp::args = args,
        bp::on_exit = 
        [&, cb, self](int exit, const std::error_code& ec)
        {
            cb(exit, buffer_, ec);
        });
}