#pragma once

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/file_base.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/assert.hpp>
#include <boost/optional.hpp>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <utility>
#include <string>
#include <filesystem>
#include <boost/regex.hpp>
#include <iostream>//FIXME

template<class File>
struct basic_multipart_form_body
{
    static_assert(boost::beast::is_file<File>::value,
        "File requirements not met");

    /// The type of File this body uses
    using file_type = File;

    class reader;

    class writer;

    class value_type;

    static std::uint64_t size(value_type const& body);
};

template<class File>
class basic_multipart_form_body<File>::value_type
{
    // This body container holds a handle to the file
    // when it is open, and also caches the size when set.

    friend class reader;
    friend class writer;
    friend struct basic_multipart_form_body;

    std::string boundary_;
    std::filesystem::path dir_;
    // This represents the open file
    File file_;

    // The cached file size
    std::uint64_t file_size_ = 0;
    
    struct value_t
    {
        explicit value_t(bool is_file_arg, std::string content_arg)
            : is_file(is_file_arg)
            , content(content_arg)
        {}
        bool is_file;
        std::string content;
    };

    std::multimap<std::string, value_t> values_;
    std::multimap<std::string, File> files_;
    using values_iterator = typename std::multimap<std::string, value_t>::iterator;
    using files_iterator = typename std::multimap<std::string, File>::iterator;
public:
    /** Destructor.

        If the file is open, it is closed first.
    */
    ~value_type() = default;

    /// Constructor
    value_type() = default;

    /// Constructor
    value_type(value_type&& other) = default;

    /// Move assignment
    value_type& operator=(value_type&& other) = default;

    /// Returns `true` if the file is open
    bool is_open() const
    {
        return file_.is_open();
    }

    /// Returns the size of the file if open
    std::uint64_t size() const
    {
        return file_size_;
    }

    /// Close the file if open
    void close();

    /** Open a file at the given path with the specified mode

        @param path The utf-8 encoded path to the file

        @param mode The file mode to use

        @param ec Set to the error, if any occurred
    */
    void open(char const* path, boost::beast::file_mode mode, boost::beast::error_code& ec);
    
    void set_boundary(const std::string& boundary);
    void set_tmp_dir(const std::string& dir_path, boost::beast::error_code& ec);
    
    const std::multimap<std::string, value_t>& get_values() const
    {
        return values_;
    }
    const std::multimap<std::string, File>& get_files() const
    {
        return files_;
    }
    /** Set the open file

        This function is used to set the open file. Any previously
        set file will be closed.

        @param file The file to set. The file must be open or else
        an error occurs

        @param ec Set to the error, if any occurred
    */
    void reset(File&& file, boost::beast::error_code& ec);
};

template<class File>
void basic_multipart_form_body<File>::value_type::close()
{
    boost::beast::error_code ignored;
    file_.close(ignored);
}

template<class File>
void basic_multipart_form_body<File>::value_type::open(
        char const* path,
        boost::beast::file_mode mode,
        boost::beast::error_code& ec)
{
    // Open the file
    file_.open(path, mode, ec);
    if(ec)
        return;

    // Cache the size
    file_size_ = file_.size(ec);
    if(ec)
    {
        close();
        return;
    }
}

template<class File>
void basic_multipart_form_body<File>::value_type::set_boundary(
        const std::string& boundary)
{
    boundary_ = boundary;
}

template<class File>
void basic_multipart_form_body<File>::value_type::set_tmp_dir(
        const std::string& dir_path,
        boost::beast::error_code& ec)
{
    dir_ = dir_path;
    std::error_code std_ec;
    bool is_dir = std::filesystem::is_directory(dir_, std_ec);
    if(!is_dir)
    {
        ec = make_error_code(boost::system::errc::not_a_directory);
    }
}

template<class File>
void basic_multipart_form_body<File>::value_type::reset(
        File&& file,
        boost::beast::error_code& ec)
{
    // First close the file if open
    if(file_.is_open())
    {
        boost::beast::error_code ignored;
        file_.close(ignored);
    }

    // Take ownership of the new file
    file_ = std::move(file);

    // Cache the size
    file_size_ = file_.size(ec);
}

// This is called from message::payload_size
template<class File>
std::uint64_t basic_multipart_form_body<File>::size(
        value_type const& body)
{
    // Forward the call to the body
    return body.size();
}

//]

//[example_http_file_body_3

/** Algorithm for retrieving buffers when serializing.

    Objects of this type are created during serialization
    to extract the buffers representing the body.
*/
template<class File>
class basic_multipart_form_body<File>::writer
{
    value_type& body_;      // The body we are reading from
    std::uint64_t remain_;  // The number of unread bytes
    char buf_[4096];        // Small buffer for reading

public:
    // The type of buffer sequence returned by `get`.
    //
    using const_buffers_type =
        boost::asio::const_buffer;

    // Constructor.
    //
    // `h` holds the headers of the message we are
    // serializing, while `b` holds the body.
    //
    // Note that the message is passed by non-const reference.
    // This is intentional, because reading from the file
    // changes its "current position" which counts makes the
    // operation logically not-const (although it is bitwise
    // const).
    //
    // The BodyWriter concept allows the writer to choose
    // whether to take the message by const reference or
    // non-const reference. Depending on the choice, a
    // serializer constructed using that body type will
    // require the same const or non-const reference to
    // construct.
    //
    // Readers which accept const messages usually allow
    // the same body to be serialized by multiple threads
    // concurrently, while readers accepting non-const
    // messages may only be serialized by one thread at
    // a time.
    //
    template<bool isRequest, class Fields>
    writer(boost::beast::http::header<isRequest, Fields>& h, value_type& b);

    // Initializer
    //
    // This is called before the body is serialized and
    // gives the writer a chance to do something that might
    // need to return an error code.
    //
    void init(boost::beast::error_code& ec);

    // This function is called zero or more times to
    // retrieve buffers. A return value of `boost::none`
    // means there are no more buffers. Otherwise,
    // the contained pair will have the next buffer
    // to serialize, and a `bool` indicating whether
    // or not there may be additional buffers.
    boost::optional<std::pair<const_buffers_type, bool>> get(
            boost::beast::error_code& ec);
};

//]

//[example_http_file_body_4

// Here we just stash a reference to the path for later.
// Rather than dealing with messy constructor exceptions,
// we save the things that might fail for the call to `init`.
//
template<class File>
template<bool isRequest, class Fields>
basic_multipart_form_body<File>::writer::writer(
        boost::beast::http::header<isRequest, Fields>& h,
        value_type& b)
    : body_(b)
{
    boost::ignore_unused(h);

    // The file must already be open
    BOOST_ASSERT(body_.file_.is_open());

    // Get the size of the file
    remain_ = body_.file_size_;
}

// Initializer
template<class File>
void basic_multipart_form_body<File>::writer::init(boost::beast::error_code& ec)
{
    // The error_code specification requires that we
    // either set the error to some value, or set it
    // to indicate no error.
    //
    // We don't do anything fancy so set "no error"
    ec.assign(0, ec.category());
}

// This function is called repeatedly by the serializer to
// retrieve the buffers representing the body. Our strategy
// is to read into our buffer and return it until we have
// read through the whole file.
//
template<class File>
auto basic_multipart_form_body<File>::writer::get(
        boost::beast::error_code& ec) ->
    boost::optional<std::pair<const_buffers_type, bool>>
{
    // Calculate the smaller of our buffer size,
    // or the amount of unread data in the file.
    auto const amount =  remain_ > sizeof(buf_) ?
        sizeof(buf_) : static_cast<std::size_t>(remain_);

    // Handle the case where the file is zero length
    if(amount == 0)
    {
        // Modify the error code to indicate success
        // This is required by the error_code specification.
        //
        // NOTE We use the existing category instead of calling
        //      into the library to get the generic category because
        //      that saves us a possibly expensive atomic operation.
        //
        ec.assign(0, ec.category());
        return boost::none;
    }

    // Now read the next buffer
    auto const nread = body_.file_.read(buf_, amount, ec);
    if(ec)
        return boost::none;

    // Make sure there is forward progress
    BOOST_ASSERT(nread != 0);
    BOOST_ASSERT(nread <= remain_);

    // Update the amount remaining based on what we got
    remain_ -= nread;

    // Return the buffer to the caller.
    //
    // The second element of the pair indicates whether or
    // not there is more data. As long as there is some
    // unread bytes, there will be more data. Otherwise,
    // we set this bool to `false` so we will not be called
    // again.
    //
    ec.assign(0, ec.category());
    return {{
        const_buffers_type{buf_, nread},    // buffer to return.
        remain_ > 0                         // `true` if there are more buffers.
        }};
}

//]

//[example_http_file_body_5

/** Algorithm for storing buffers when parsing.

    Objects of this type are created during parsing
    to store incoming buffers representing the body.
*/
template<class File>
class basic_multipart_form_body<File>::reader
{
    value_type& body_;  // The body we are writing to
    bool cr_ = false;
    bool if_file_ = false;
    bool content_disposition_parsed_ = false;
    enum class state
    {
        init,
        headers,
        body,
        next_block
    } parser_state_ = state::init;
    std::string block_buffer_;
    std::string next_block_buffer_;
    typename value_type::values_iterator current_content_;
    typename value_type::files_iterator current_file_;
    void handle_data(const char* data, size_t size);
    void handle_crlf();
    void parse_content_disposition();
public:
    // Constructor.
    //
    // This is called after the header is parsed and
    // indicates that a non-zero sized body may be present.
    // `h` holds the received message headers.
    // `b` is an instance of `basic_multipart_form_body`.
    //
    template<bool isRequest, class Fields>
    explicit reader(boost::beast::http::header<isRequest, Fields>&h, value_type& b);

    // Initializer
    //
    // This is called before the body is parsed and
    // gives the reader a chance to do something that might
    // need to return an error code. It informs us of
    // the payload size (`content_length`) which we can
    // optionally use for optimization.
    //
    void init(boost::optional<std::uint64_t> const&, boost::beast::error_code& ec);

    // This function is called one or more times to store
    // buffer sequences corresponding to the incoming body.
    //
    template<class ConstBufferSequence>
    std::size_t put(
        ConstBufferSequence const& buffers,
        boost::beast::error_code& ec);

    // This function is called when writing is complete.
    // It is an opportunity to perform any final actions
    // which might fail, in order to return an error code.
    // Operations that might fail should not be attempted in
    // destructors, since an exception thrown from there
    // would terminate the program.
    //
    void
    finish(boost::beast::error_code& ec);
};

//]

//[example_http_file_body_6

// We don't do much in the reader constructor since the
// file is already open.
//
template<class File>
template<bool isRequest, class Fields>
basic_multipart_form_body<File>::reader::reader(
        boost::beast::http::header<isRequest, Fields>& h,
        value_type& body)
    : body_(body)
{
    boost::ignore_unused(h);
}

template<class File>
void basic_multipart_form_body<File>::reader::init(
    boost::optional<std::uint64_t> const& content_length,
    boost::beast::error_code& ec)
{
    // The file must already be open for writing
    BOOST_ASSERT(body_.file_.is_open());

    // We don't do anything with this but a sophisticated
    // application might check available space on the device
    // to see if there is enough room to store the body.
    boost::ignore_unused(content_length);

    // The error_code specification requires that we
    // either set the error to some value, or set it
    // to indicate no error.
    //
    // We don't do anything fancy so set "no error"
    ec.assign(0, ec.category());
}

// This will get called one or more times with body buffers
template<class File>
template<class ConstBufferSequence>
std::size_t basic_multipart_form_body<File>::reader::put(
        ConstBufferSequence const& buffers,
        boost::beast::error_code& ec)
{
    // This function must return the total number of
    // bytes transferred from the input buffers.
    std::size_t nwritten = 0;

    // Loop over all the buffers in the sequence,
    // and write each one to the file.
    for(auto it = boost::asio::buffer_sequence_begin(buffers);
        it != boost::asio::buffer_sequence_end(buffers); ++it)
    {
        boost::asio::const_buffer buffer = *it;

        size_t offset = 0;
        std::string_view str_view(
                        static_cast<const char*>(buffer.data()),
                        buffer.size());
        while(offset != str_view.size())
        {
            if(!cr_)
            {
                auto pos = str_view.find('\r', offset);

                if(pos != str_view.npos)
                {
                    //copy [offset, pos]
                    handle_data(str_view.data() + offset,
                                pos - offset);
                    nwritten += pos - offset + 1;
                    offset = pos + 1;
                    cr_ = true;
                    continue;
                }
                else
                {
                    //copy [offset, buffer.size())
                    handle_data(str_view.data() + offset,
                                str_view.size() - offset);
                    nwritten += buffer.size() - offset;
                    break;
                }
            }
            else if(str_view[offset] == '\n')
            {
                //parse_block
                handle_crlf();
            }
            else
            {
                //add '\r'
                handle_data("\r", 1);
                //copy [offset]
                handle_data(str_view.data() + offset, 1);
            }
            ++offset;
            ++nwritten;
            cr_ = false;
        }
    }
    // Indicate success
    // This is required by the error_code specification
    ec.assign(0, ec.category());

    return nwritten;
}

template<class File>
void basic_multipart_form_body<File>::reader::handle_data(
        const char* data,
        size_t size)
{
    if(size == 0)
    {
        return;
    }
    switch(parser_state_)
    {
        case state::init:
        {
            block_buffer_.append(data, size);
            break;
        }
        case state::headers:
        {
            block_buffer_.append(data, size);
            break;
        }
        case state::body:
        {
            if(!if_file_)
            {
                current_content_->second.content.append(data, size);
            }
            else
            {
                boost::system::error_code ec;
                current_file_->second.write(
                    data, size, ec);
            }
            break;
        }
        case state::next_block:
        {
            next_block_buffer_.append(data, size);
            if(next_block_buffer_.size() >= body_.boundary_.size() + 2)
            {
                if(next_block_buffer_.substr(0, 2 + body_.boundary_.size())
                        == "--" + body_.boundary_)
                {
                    parser_state_ = state::init;
                }
                else
                {
                    parser_state_ = state::body;
                }
                handle_data(next_block_buffer_.data(), next_block_buffer_.size());
                next_block_buffer_.clear();
            }
            break;
        }
    }
}

template<class File>
void basic_multipart_form_body<File>::reader::handle_crlf()
{
    switch(parser_state_)
    {
        case state::init:
        {
            if(block_buffer_ == "--" + body_.boundary_)
            {
                block_buffer_.clear();
                parser_state_ = state::headers;
            }
            else if(block_buffer_ != "--" + body_.boundary_ + "--")
            {
                //TODO err
            }
            break;
        }
        case state::headers:
        {
            if(!content_disposition_parsed_)
            {
                parse_content_disposition();
                content_disposition_parsed_ = true;
            }
            if(block_buffer_.empty())
            {
                content_disposition_parsed_ = false;
                parser_state_ = state::body;
            }
            block_buffer_.clear();
            break;
        }
        case state::body:
        {   
            parser_state_ = state::next_block;
            break;
        }
        case state::next_block:
        {
            break;
        }
    }
}

template<class File>
void basic_multipart_form_body<File>::reader::parse_content_disposition()
{
    static boost::regex r(R"rx(([^;=]+)(?:="([^"]+)")?(?:$|(?:; )))rx");
    auto start = block_buffer_.cbegin() + std::string("Content-Disposition: ").size();
    auto end = block_buffer_.cend();
    boost::match_results<std::string::const_iterator> what;
    boost::match_flag_type flags = boost::match_default;
    std::string name;
    std::string filename;
    while(regex_search(start, end, what, r, flags))
    {

        if(what[1] == "name")
        {
            name = what[2];
        }
        if(what[1] == "filename")
        {
            filename = what[2];
        }

        start = what[0].second;
        // update flags: 
        flags |= boost::match_prev_avail;
        flags |= boost::match_not_bob;
    }
    if(filename.empty())
    {
        current_content_ = body_.values_.emplace(
                std::piecewise_construct,
                std::make_tuple(name),
                std::make_tuple(false, ""));
        if_file_ = false;
    }
    else
    {
        current_content_ = body_.values_.emplace(
                std::piecewise_construct,
                std::make_tuple(name),
                std::make_tuple(true, filename));
        current_file_ = body_.files_.emplace(
                std::piecewise_construct,
                std::make_tuple(filename),
                std::make_tuple());
        boost::beast::error_code ec;
        current_file_->second.open(
                (body_.dir_ / filename).c_str(),
                boost::beast::file_mode::write,
                ec);
        if_file_ = true;
    }
}

// Called after writing is done when there's no error.
template<class File>
void basic_multipart_form_body<File>::reader::finish(
        boost::beast::error_code& ec)
{
    // This has to be cleared before returning, to
    // indicate no error. The specification requires it.
    ec.assign(0, ec.category());
}
