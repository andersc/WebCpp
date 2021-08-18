#include <algorithm>
#include "Request.h"


using namespace WebCpp;

Request::Request(const HttpConfig& config):
    m_config(config)
{

}

Request::Request(int connID, const ByteArray &request, HttpHeader &&header, const HttpConfig& config):
    m_connID(connID),
    m_config(config),
    m_header(std::move(header))
{
    Init(request);
}

int Request::GetConnectionID() const
{
    return m_connID;
}

const HttpHeader &Request::GetHeader() const
{
    return m_header;
}

void Request::Init(const ByteArray &data)
{    
    ByteArray delimiter { { CRLFCRLF } };

    if(m_header.GetBodySize() > 0)
    {        
        ParseBody(data, m_header.GetHeaderSize() + 2);
    }    
}

void Request::ParseBody(const ByteArray &data, size_t headerSize)
{
    auto contentType = m_header.GetHeader(HttpHeader::HeaderType::ContentType);
    m_requestBody.Parse(data, headerSize, ByteArray(contentType.begin(), contentType.end()), m_config.GetTempFile());
}

const ByteArray& Request::GetData() const
{
    return m_data;
}

const RequestBody &Request::GetRequestBody() const
{
    return m_requestBody;
}

void Request::SetArg(const std::string &name, const std::string &value)
{
    m_args[name] = value;
}

std::string Request::GetArg(const std::string &name) const
{
    if(m_args.find(name) == m_args.end())
    {
        return "";
    }

    return m_args.at(name);
}


