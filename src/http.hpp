#pragma once

#include <map>
#include <string>
#include <vector>

namespace net {

struct HttpResponse {
    bool ok = false;              // true if the request completed (transport level)
    long status = 0;             // HTTP status code
    std::string body;            // response body
    std::map<std::string, std::string> headers;  // lower-cased response headers
    std::string error;           // transport error message, if any
};

// Perform an HTTPS POST. `body` is sent as-is with the given content type.
HttpResponse httpPost(const std::string& url,
                      const std::string& body,
                      const std::string& contentType,
                      const std::vector<std::string>& extraHeaders = {});

// Perform an HTTPS GET.
HttpResponse httpGet(const std::string& url,
                     const std::vector<std::string>& extraHeaders = {});

// URL-encode a single component.
std::string urlEncode(const std::string& value);

}  // namespace net
