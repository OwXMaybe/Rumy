#pragma once

#include <stddef.h>

extern "C" {

typedef void CURL;
typedef int CURLcode;
typedef int CURLoption;
typedef int CURLINFO;

CURLcode curl_global_init(long flags);
CURL* curl_easy_init(void);
CURLcode curl_easy_setopt(CURL* h, CURLoption opt, ...);
CURLcode curl_easy_perform(CURL* h);
CURLcode curl_easy_getinfo(CURL* h, CURLINFO info, ...);
void curl_easy_cleanup(CURL* h);
const char* curl_easy_strerror(CURLcode c);
}

namespace curlc {

constexpr long GLOBAL_DEFAULT = 3;
constexpr CURLcode OK = 0;
constexpr CURLcode E_COULDNT_RESOLVE_HOST = 6;
constexpr CURLcode E_COULDNT_CONNECT = 7;
constexpr CURLcode E_WRITE_ERROR = 23;
constexpr CURLcode E_OPERATION_TIMEDOUT = 28;
constexpr CURLcode E_BAD_CONTENT_ENCODING = 61;
constexpr CURLcode E_FILESIZE_EXCEEDED = 63;
constexpr CURLoption WRITEDATA = 10001;
constexpr CURLoption URL = 10002;
constexpr CURLoption USERAGENT = 10018;
constexpr CURLoption ACCEPT_ENCODING = 10102;
constexpr CURLoption WRITEFUNCTION = 20011;
constexpr CURLoption FOLLOWLOCATION = 52;
constexpr CURLoption NOSIGNAL = 99;
constexpr CURLoption IPRESOLVE = 113;
constexpr CURLoption MAXFILESIZE = 114;
constexpr CURLoption TIMEOUT_MS = 155;
constexpr CURLoption CONNECTTIMEOUT_MS = 156;
constexpr long IPRESOLVE_V4 = 1;
constexpr CURLINFO RESPONSE_CODE = 0x200002;

}
