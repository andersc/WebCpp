#include <algorithm>
#include "HttpHeader.h"


using namespace WebCpp;

HttpHeader::Header HttpHeader::Header::defaultHeader = HttpHeader::Header();

HttpHeader::HttpHeader()
{

}

bool HttpHeader::Parse(const ByteArray &data)
{
    m_complete = false;
    ByteArray delimiter { { CRLFCRLF } };
    size_t pos;

    if(look_for(data, delimiter, pos))
    {
        auto arr = split(data, { CRLF }, pos);
        if(ParseHeaders(arr))
        {
            m_headerSize = pos;
            m_complete = true;
        }
    }

    return m_complete;
}

bool HttpHeader::IsComplete() const
{
    return m_complete;
}

size_t HttpHeader::GetHeaderSize() const
{
    return m_headerSize;
}

size_t HttpHeader::GetBodySize() const
{
    size_t size = 0;

    if(m_complete)
    {
        auto str = GetHeader(HeaderType::ContentLength);
        int num;
        if(string2int(str, num))
        {
            size = num;
        }
    }

    return size;
}

size_t HttpHeader::GetRequestSize() const
{
    return GetHeaderSize() + GetBodySize() + 4; // header + CRLFCRLF + body
}

HttpHeader::Method HttpHeader::GetMethod() const
{
    return m_method;
}

std::string HttpHeader::GetPath() const
{
    return m_path;
}

std::string HttpHeader::GetUri() const
{
    return m_uri;
}

bool HttpHeader::ParseHeaders(std::vector<ByteArray> &arr)
{
    for(auto &line: arr)
    {
        if(m_method == HttpHeader::Method::Undefined)
        {
            std::string s(line.begin(), line.end());
            auto methodArr = split(s, ' ');
            if(methodArr.size() < 3)
            {
                return false;
            }

            trim(methodArr[0]);
            m_method = HttpHeader::String2Method(methodArr[0]);
            if(m_method == HttpHeader::Method::Undefined)
            {
                return false;
            }

            trim(methodArr[1]);
            m_uri = methodArr[1];
            ParseQuery();

            trim(methodArr[2]);
            m_version = methodArr[2];
        }
        else
        {
            auto it = std::find(line.begin(), line.end(), ':');
            if(it != line.end())
            {
                std::string name = std::string(line.begin(), it);
                std::string value = std::string(it + 1, line.end());
                trim(name);
                trim(value);

                HttpHeader::Header header;
                header.name = name;
                header.type = String2HeaderType(name);
                header.value = value;

                m_headers.push_back(std::move(header));
            }
        }
    }

    return true;
}

HttpHeader::Method HttpHeader::String2Method(const std::string &str)
{
    std::string s = str;
    toUpper(s);

    switch(_(s.c_str()))
    {
        case _("OPTIONS"): return HttpHeader::Method::OPTIONS;
        case _("GET"):     return HttpHeader::Method::GET;
        case _("HEAD"):    return HttpHeader::Method::HEAD;
        case _("POST"):    return HttpHeader::Method::POST;
        case _("PUT"):     return HttpHeader::Method::PUT;
        case _("DELETE"):  return HttpHeader::Method::DELETE;
        case _("TRACE"):   return HttpHeader::Method::TRACE;
        case _("CONNECT"): return HttpHeader::Method::CONNECT;
        default:
            break;
    }

    return HttpHeader::Method::Undefined;
}

std::string HttpHeader::Method2String(HttpHeader::Method method)
{
    switch(method)
    {
        case HttpHeader::Method::OPTIONS: return "OPTIONS";
        case HttpHeader::Method::GET:     return "GET";
        case HttpHeader::Method::HEAD:    return "HEAD";
        case HttpHeader::Method::POST:    return "POST";
        case HttpHeader::Method::PUT:     return "PUT";
        case HttpHeader::Method::DELETE:  return "DELETE";
        case HttpHeader::Method::TRACE:   return "TRACE";
        case HttpHeader::Method::CONNECT: return "CONNECT";
        default:
            break;
    }

    return "";
}

HttpHeader::HeaderType HttpHeader::String2HeaderType(const std::string &str)
{
    switch(_(str.c_str()))
    {
        case _("Accept"):              return HttpHeader::HeaderType::Accept;
        case _("Accept-Charset"):      return HttpHeader::HeaderType::AcceptCharset;
        case _("Accept-Encoding"):     return HttpHeader::HeaderType::AcceptEncoding;
        case _("Accept-Datetime"):     return HttpHeader::HeaderType::AcceptDatetime;
        case _("Accept-Language"):     return HttpHeader::HeaderType::AcceptLanguage;
        case _("Authorization"):       return HttpHeader::HeaderType::Authorization;
        case _("Cache-Control"):       return HttpHeader::HeaderType::CacheControl;
        case _("Connection"):          return HttpHeader::HeaderType::Connection;
        case _("Content-Encoding"):    return HttpHeader::HeaderType::ContentEncoding;
        case _("Content-Length"):      return HttpHeader::HeaderType::ContentLength;
        case _("Content-MD5"):         return HttpHeader::HeaderType::ContentMD5;
        case _("Content-Type"):        return HttpHeader::HeaderType::ContentType;
        case _("Cookie"):              return HttpHeader::HeaderType::Cookie;
        case _("Date"):                return HttpHeader::HeaderType::Date;
        case _("Expect"):              return HttpHeader::HeaderType::Expect;
        case _("Forwarded"):           return HttpHeader::HeaderType::Forwarded;
        case _("From"):                return HttpHeader::HeaderType::From;
        case _("HTTP2-Settings"):      return HttpHeader::HeaderType::HTTP2Settings;
        case _("Host"):                return HttpHeader::HeaderType::Host;
        case _("If-Match"):            return HttpHeader::HeaderType::IfMatch;
        case _("If-Modified-Since"):   return HttpHeader::HeaderType::IfModifiedSince;
        case _("If-None-Match"):       return HttpHeader::HeaderType::IfNoneMatch;
        case _("If-Range"):            return HttpHeader::HeaderType::IfRange;
        case _("If-Unmodified-Since"): return HttpHeader::HeaderType::IfUnmodifiedSince;
        case _("Max-Forwards"):        return HttpHeader::HeaderType::MaxForwards;
        case _("Origin"):              return HttpHeader::HeaderType::Origin;
        case _("Pragma"):              return HttpHeader::HeaderType::Pragma;
        case _("Prefer"):              return HttpHeader::HeaderType::Prefer;
        case _("Proxy-Authorization"): return HttpHeader::HeaderType::ProxyAuthorization;
        case _("Range"):               return HttpHeader::HeaderType::Range;
        case _("Referer"):             return HttpHeader::HeaderType::Referer;
        case _("TE"):                  return HttpHeader::HeaderType::TE;
        case _("Trailer"):             return HttpHeader::HeaderType::Trailer;
        case _("TransferEncoding"):    return HttpHeader::HeaderType::TransferEncoding;
        case _("User-Agent"):          return HttpHeader::HeaderType::UserAgent;
        case _("Upgrade"):             return HttpHeader::HeaderType::Upgrade;
        case _("Via"):                 return HttpHeader::HeaderType::Via;
        case _("Warning"):             return HttpHeader::HeaderType::Warning;
        default: break;
    }

    return HttpHeader::HeaderType::Undefined;
}

std::string HttpHeader::HeaderType2String(HttpHeader::HeaderType headerType)
{
    switch(headerType)
    {
        case HttpHeader::HeaderType::Accept:              return "Accept";
        case HttpHeader::HeaderType::AcceptCharset:       return "Accept-Charset";
        case HttpHeader::HeaderType::AcceptEncoding:      return "Accept-Encoding";
        case HttpHeader::HeaderType::AcceptDatetime:      return "Accept-Datetime";
        case HttpHeader::HeaderType::AcceptLanguage:      return "Accept-Language";
        case HttpHeader::HeaderType::Authorization:       return "Authorization";
        case HttpHeader::HeaderType::CacheControl:        return "Cache-Control";
        case HttpHeader::HeaderType::Connection:          return "Connection";
        case HttpHeader::HeaderType::ContentEncoding:     return "Content-Encoding";
        case HttpHeader::HeaderType::ContentLength:       return "Content-Length";
        case HttpHeader::HeaderType::ContentMD5:          return "Content-MD5";
        case HttpHeader::HeaderType::ContentType:         return "Content-Type";
        case HttpHeader::HeaderType::Cookie:              return "Cookie";
        case HttpHeader::HeaderType::Date:                return "Date";
        case HttpHeader::HeaderType::Expect:              return "Expect";
        case HttpHeader::HeaderType::Forwarded:           return "Forwarded";
        case HttpHeader::HeaderType::From:                return "From";
        case HttpHeader::HeaderType::HTTP2Settings:       return "HTTP2-Settings";
        case HttpHeader::HeaderType::Host:                return "Host";
        case HttpHeader::HeaderType::IfMatch:             return "If-Match";
        case HttpHeader::HeaderType::IfModifiedSince:     return "If-Modified-Since";
        case HttpHeader::HeaderType::IfNoneMatch:         return "If-None-Match";
        case HttpHeader::HeaderType::IfRange:             return "If-Range";
        case HttpHeader::HeaderType::IfUnmodifiedSince:   return "If-Unmodified-Since";
        case HttpHeader::HeaderType::MaxForwards:         return "Max-Forwards";
        case HttpHeader::HeaderType::Origin:              return "Origin";
        case HttpHeader::HeaderType::Pragma:              return "Pragma";
        case HttpHeader::HeaderType::Prefer:              return "Prefer";
        case HttpHeader::HeaderType::ProxyAuthorization:  return "Proxy-Authorization";
        case HttpHeader::HeaderType::Range:               return "Range";
        case HttpHeader::HeaderType::Referer:             return "Referer";
        case HttpHeader::HeaderType::TE:                  return "TE";
        case HttpHeader::HeaderType::Trailer:             return "Trailer";
        case HttpHeader::HeaderType::TransferEncoding:    return "TransferEncoding";
        case HttpHeader::HeaderType::UserAgent:           return "User-Agent";
        case HttpHeader::HeaderType::Upgrade:             return "Upgrade";
        case HttpHeader::HeaderType::Via:                 return "Via";
        case HttpHeader::HeaderType::Warning:             return "Warning";
        default: break;
    }

    return "";
}

void HttpHeader::ParseQuery()
{
    auto pos = m_uri.find('?');

    if(pos != std::string::npos)
    {
        m_path = std::string(m_uri.begin(), m_uri.begin() + pos);
        std::string query = std::string(m_uri.begin() + pos + 1, m_uri.end());
        auto q = split(query, '&');
        for(auto &token: q)
        {
            auto parr = split(token, '=');
            if(parr.size() == 2)
            {
                urlDecode(parr[0]);
                urlDecode(parr[1]);
                m_query[parr[0]] = parr[1];
            }
        }
    }
    else
    {
        m_path = m_uri;
    }
}


const std::vector<HttpHeader::Header> &HttpHeader::GetHeaders() const
{
    return m_headers;
}

std::string HttpHeader::GetHeader(HeaderType headerType) const
{
    return GetHeader(HttpHeader::HeaderType2String(headerType));
}

std::string HttpHeader::GetHeader(const std::string &headerType) const
{
    for(auto &header: m_headers)
    {
        if(header.name == headerType)
        {
            return header.value;
        }
    }

    return "";
}

std::string HttpHeader::GetVersion() const
{
    return m_version;
}

std::string HttpHeader::GetHost() const
{
    return m_host;
}
