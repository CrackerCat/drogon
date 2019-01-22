/**
 *
 *  HttpResponseImpl.cc
 *  An Tao
 *  
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "HttpResponseImpl.h"

#include <drogon/HttpViewBase.h>
#include <drogon/HttpViewData.h>
#include <drogon/HttpAppFramework.h>
#include <trantor/utils/Logger.h>
#include <stdio.h>
#include <sys/stat.h>
#include <memory>

using namespace trantor;
using namespace drogon;

HttpResponsePtr HttpResponse::newHttpResponse()
{
    auto res = std::make_shared<HttpResponseImpl>();
    res->setStatusCode(k200OK);
    res->setContentTypeCode(CT_TEXT_HTML);
    return res;
}

HttpResponsePtr HttpResponse::newHttpJsonResponse(const Json::Value &data)
{
    auto res = std::make_shared<HttpResponseImpl>();
    res->setStatusCode(k200OK);
    res->setContentTypeCode(CT_APPLICATION_JSON);
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    builder["indentation"] = "";
    res->setBody(writeString(builder, data));
    return res;
}
HttpResponsePtr HttpResponse::newNotFoundResponse()
{
    HttpViewData data;
    data.insert("version", getVersion());
    auto res = HttpResponse::newHttpViewResponse("NotFound", data);
    res->setStatusCode(k404NotFound);
    //res->setCloseConnection(true);

    return res;
}
HttpResponsePtr HttpResponse::newLocationRedirectResponse(const std::string &path)
{
    auto res = std::make_shared<HttpResponseImpl>();
    res->setStatusCode(k302Found);
    res->redirect(path.c_str());
    return res;
}

HttpResponsePtr HttpResponse::newHttpViewResponse(const std::string &viewName, const HttpViewData &data)
{
    return HttpViewBase::genHttpResponse(viewName, data);
}

std::string HttpResponseImpl::web_response_code_to_string(int code)
{
    switch (code)
    {
    case 100:
        return "Continue";
    case 101:
        return "Switching Protocols";
    case 200:
        return "OK";
    case 201:
        return "Created";
    case 202:
        return "Accepted";
    case 203:
        return "Non-Authoritative Information";
    case 204:
        return "No Content";
    case 205:
        return "Reset Content";
    case 206:
        return "Partial Content";
    case 300:
        return "Multiple Choices";
    case 301:
        return "Moved Permanently";
    case 302:
        return "Found";
    case 303:
        return "See Other";
    case 304:
        return "Not Modified";
    case 305:
        return "Use Proxy";
    case 307:
        return "Temporary Redirect";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 402:
        return "Payment Required";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 405:
        return "Method Not Allowed";
    case 406:
        return "Not Acceptable";
    case 407:
        return "Proxy Authentication Required";
    case 408:
        return "Request Time-out";
    case 409:
        return "Conflict";
    case 410:
        return "Gone";
    case 411:
        return "Length Required";
    case 412:
        return "Precondition Failed";
    case 413:
        return "Request Entity Too Large";
    case 414:
        return "Request-URI Too Large";
    case 415:
        return "Unsupported Media Type";
    case 416:
        return "Requested range not satisfiable";
    case 417:
        return "Expectation Failed";
    case 500:
        return "Internal Server Error";
    case 501:
        return "Not Implemented";
    case 502:
        return "Bad Gateway";
    case 503:
        return "Service Unavailable";
    case 504:
        return "Gateway Time-out";
    case 505:
        return "HTTP Version not supported";
    default:
        if (code >= 100 && code < 200)
            return "Informational";

        if (code >= 200 && code < 300)
            return "Successful";

        if (code >= 300 && code < 400)
            return "Redirection";

        if (code >= 400 && code < 500)
            return "Bad Request";

        if (code >= 500 && code < 600)
            return "Server Error";

        return "Undefined Error";
    }
}
void HttpResponseImpl::makeHeaderString(const std::shared_ptr<std::string> &headerStringPtr) const
{
    char buf[64];
    assert(headerStringPtr);
    snprintf(buf, sizeof buf, "HTTP/1.1 %d ", _statusCode);
    headerStringPtr->append(buf);
    headerStringPtr->append(_statusMessage);
    headerStringPtr->append("\r\n");
    if (_sendfileName.empty())
    {
        snprintf(buf, sizeof buf, "Content-Length: %lu\r\n", static_cast<long unsigned int>(_bodyPtr->size()));
    }
    else
    {
        struct stat filestat;
        if (stat(_sendfileName.c_str(), &filestat) < 0)
        {
            LOG_SYSERR << _sendfileName << " stat error";
            return;
        }
        snprintf(buf, sizeof buf, "Content-Length: %llu\r\n", static_cast<long long unsigned int>(filestat.st_size));
    }

    headerStringPtr->append(buf);
    if (_headers.find("Connection") == _headers.end())
    {
        if (_closeConnection)
        {
            headerStringPtr->append("Connection: close\r\n");
        }
        else
        {

            //output->append("Connection: Keep-Alive\r\n");
        }
    }

    for (auto it = _headers.begin(); it != _headers.end(); ++it)
    {
        headerStringPtr->append(it->first);
        headerStringPtr->append(": ");
        headerStringPtr->append(it->second);
        headerStringPtr->append("\r\n");
    }

    headerStringPtr->append("Server: drogon/");
    headerStringPtr->append(drogon::getVersion());
    headerStringPtr->append("\r\n");
}

std::shared_ptr<std::string> HttpResponseImpl::renderToString() const
{
    if (_expriedTime >= 0)
    {
        if (_datePos != std::string::npos)
        {
            auto now = trantor::Date::now();
            bool isDateChanged = ((now.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC) != _httpStringDate);
            assert(_httpString);
            if (isDateChanged)
            {
                _httpStringDate = now.microSecondsSinceEpoch() / MICRO_SECONDS_PRE_SEC;
                auto newDate = getHttpFullDate(now);
                {
                    std::lock_guard<std::mutex> lock(*_httpStringMutex);
                    _httpString = std::make_shared<std::string>(*_httpString);
                    memcpy((void *)&(*_httpString)[_datePos], newDate, strlen(newDate));
                    return _httpString;
                }
            }
            {
                std::lock_guard<std::mutex> lock(*_httpStringMutex);
                return _httpString;
            }
        }
    }
    auto httpString = std::make_shared<std::string>();
    httpString->reserve(256);
    if (!_fullHeaderString)
    {
        makeHeaderString(httpString);
    }
    else
    {
        httpString->append(*_fullHeaderString);
    }

    //output cookies
    if (_cookies.size() > 0)
    {
        for (auto it = _cookies.begin(); it != _cookies.end(); ++it)
        {

            httpString->append(it->second.cookieString());
        }
    }

    //output Date header
    httpString->append("Date: ");
    auto datePos = httpString->length();
    httpString->append(getHttpFullDate(trantor::Date::date()));
    httpString->append("\r\n\r\n");

    LOG_TRACE << "reponse(no body):" << httpString->c_str();
    httpString->append(*_bodyPtr);
    if (_expriedTime >= 0)
    {
        std::lock_guard<std::mutex> lock(*_httpStringMutex);
        _datePos = datePos;
        _httpString = httpString;
    }
    return httpString;
}
