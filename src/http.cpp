#include "http.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>

namespace net {

namespace {

size_t writeBody(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

size_t writeHeader(char* buffer, size_t size, size_t nitems, void* userp) {
    const size_t total = size * nitems;
    auto* headers = static_cast<std::map<std::string, std::string>*>(userp);
    std::string line(buffer, total);
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }
    const auto colon = line.find(':');
    if (colon != std::string::npos) {
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
            value.erase(value.begin());
        }
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        (*headers)[key] = value;
    }
    return total;
}

HttpResponse perform(const std::string& url,
                     const std::string& body,
                     bool post,
                     const std::string& contentType,
                     const std::vector<std::string>& extraHeaders) {
    HttpResponse resp;
    CURL* curl = curl_easy_init();
    if (!curl) {
        resp.error = "failed to initialize curl";
        return resp;
    }

    struct curl_slist* headerList = nullptr;
    if (post && !contentType.empty()) {
        headerList = curl_slist_append(headerList, ("Content-Type: " + contentType).c_str());
    }
    for (const auto& h : extraHeaders) {
        headerList = curl_slist_append(headerList, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "purple-loader/1.0");
    if (headerList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }
    if (post) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writeHeader);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resp.headers);

    const CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
        resp.error = curl_easy_strerror(code);
    } else {
        resp.ok = true;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp.status);
    }

    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);
    return resp;
}

}  // namespace

HttpResponse httpPost(const std::string& url,
                      const std::string& body,
                      const std::string& contentType,
                      const std::vector<std::string>& extraHeaders) {
    return perform(url, body, true, contentType, extraHeaders);
}

HttpResponse httpGet(const std::string& url, const std::vector<std::string>& extraHeaders) {
    return perform(url, {}, false, {}, extraHeaders);
}

std::string urlEncode(const std::string& value) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        return value;
    }
    char* escaped = curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
    std::string result = escaped ? escaped : value;
    if (escaped) {
        curl_free(escaped);
    }
    curl_easy_cleanup(curl);
    return result;
}

}  // namespace net
