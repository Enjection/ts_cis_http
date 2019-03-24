#pragma once

#include "request_context.h"
#include "auth_manager.h"
#include "net/queued_websocket_session.h"

#include <rapidjson/document.h>

void ws_handle_authenticate(
        const std::shared_ptr<auth_manager>& authentication_handler,
        const rapidjson::Document& data,
        websocket_queue& queue,
        request_context& ctx);

void ws_handle_token(
        const std::shared_ptr<auth_manager>& authentication_handler,
        const rapidjson::Document& data,
        websocket_queue& queue,
        request_context& ctx);

void ws_handle_logout(
        const std::shared_ptr<auth_manager>& authentication_handler,
        const rapidjson::Document& data,
        websocket_queue& queue,
        request_context& ctx);
