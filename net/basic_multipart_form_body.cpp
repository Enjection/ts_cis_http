#include "basic_multipart_form_body.h"

#include <boost/beast/core/file.hpp>

using multipart_form_body = basic_multipart_form_body<boost::beast::file>;

static_assert(boost::beast::http::is_body<multipart_form_body>::value, "");
