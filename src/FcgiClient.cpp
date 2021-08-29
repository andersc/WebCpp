#include <cstring>
#include <iostream>
#include <iomanip> 
#include "FcgiClient.h"

#define FCGI_VERSION_1 1

using namespace WebCpp;

uint16_t FcgiClient::RequestID = 1;
FcgiClient::ResponseData FcgiClient::ResponseData::DefaultResponseData(-1,-1);

FcgiClient::FcgiClient(const std::string &address, const HttpConfig &config):
    m_connection(address),
    m_config(config)
{
    m_address = address;
}

bool FcgiClient::Init()
{
    bool retval = false;
    ClearError();

    if(m_connection.Init())
    {
        auto f1 = std::bind(&FcgiClient::OnDataReady, this, std::placeholders::_1);
        m_connection.SetDataReadyCallback(f1);

        auto f2 = std::bind(&FcgiClient::OnConnectionClosed, this);
        m_connection.SetCloseConnectionCallback(f2);

        m_connection.Run();
        retval = true;
    }
    else
    {
        SetLastError("Fcgi connection init failed: " + m_connection.GetLastError());
    }

    return retval;
}

bool FcgiClient::Connect()
{
    bool retval = false;
    ClearError();

    if(m_connection.Connect())
    {
        retval = true;
    }
    else
    {
        SetLastError("Fcgi failed to connect: " + m_connection.GetLastError());
    }


    return retval;
}

void FcgiClient::SetKeepConnection(bool keepConnection)
{
    m_keepConnection = keepConnection;
}

bool FcgiClient::GetKeepConnection()
{
    return m_keepConnection;
}

void FcgiClient::SetParam(FcgiClient::FcgiParam param, std::string name)
{
    m_fcgiParams[param] = name;
}

bool FcgiClient::SendRequest(const Request &request)
{
    if(m_connection.IsConnected() == false)
    {
        if(m_connection.Connect() == false)
        {
            SetLastError("Fcgi failed to restart the connection: " + m_connection.GetLastError());
            return false;
        }
    }

    ByteArray requestDataData;
    uint16_t ID = ++FcgiClient::RequestID;
    m_responseQueue.push_back(ResponseData(ID, request.GetConnectionID()));

    ByteArray beginPacket = BuildBeginRequestPacket(ID);
    requestDataData.insert(requestDataData.end(), beginPacket.begin(), beginPacket.end());

    ByteArray paramsData;
    const auto &header = request.GetHeader();
    for(auto &pair: m_fcgiParams)
    {
        FcgiClient::FcgiParam param = pair.first;
        std::string name = pair.second;
        std::string value = GetParam(param, header, m_config);
        ByteArray paramBytes = BuildParamPacket(name, value);
        paramsData.insert(paramsData.end(), paramBytes.begin(), paramBytes.end());
    }

    paramsData = BuildParamsPacket(ID, paramsData);
    ByteArray finalParam = BuildParamsPacket(ID, ByteArray());
    paramsData.insert(paramsData.end(), finalParam.begin(), finalParam.end());

    requestDataData.insert(requestDataData.end(), paramsData.begin(), paramsData.end());

    ByteArray stdinPacket = BuildStdinPacket(ID, ByteArray());
    requestDataData.insert(requestDataData.end(), stdinPacket.begin(), stdinPacket.end());

    if(!m_connection.Write(requestDataData))
    {
        return false;
    }

    return true;
}

std::string FcgiClient::GetParam(FcgiClient::FcgiParam param, const HttpHeader& header, const HttpConfig &config) const
{
    switch(param)
    {
        case FcgiParam::QUERY_STRING:
            return header.GetQuery();
        case FcgiParam::REQUEST_METHOD:
            return HttpHeader::Method2String(header.GetMethod());
            break;
        case FcgiParam::PATH_INFO:
            return "";
            break;
        case FcgiParam::CONTENT_TYPE:
            return header.GetHeader(HttpHeader::HeaderType::ContentType);
            break;
        case FcgiParam::CONTENT_LENGTH:
            return header.GetHeader(HttpHeader::HeaderType::ContentLength);
            break;
        case FcgiParam::SCRIPT_FILENAME:
            return config.RootFolder() + header.GetPath();
            break;
        case FcgiParam::SCRIPT_NAME:
            return header.GetPath();
        case FcgiParam::REQUEST_URI:
            return header.GetUri();
        case FcgiParam::DOCUMENT_URI:
            return "";
            break;
        case FcgiParam::DOCUMENT_ROOT:
            return config.RootFolder();
            break;
        case FcgiParam::SERVER_PROTOCOL:
            return header.GetVersion();
        case FcgiParam::GATEWAY_INTERFACE:
            return "CGI/1.1";
        case FcgiParam::REMOTE_ADDR:
            return header.GetRemoteAddress();
        case FcgiParam::REMOTE_PORT:
            return std::to_string(header.GetRemotePort());
            break;
        case FcgiParam::SERVER_ADDR:
            config.GetHttpServerAddress();
        case FcgiParam::SERVER_PORT:
            return std::to_string(config.GetHttpServerPort());
        case FcgiParam::SERVER_NAME:
            return config.GetServerName();
    }

    return "";
}

ByteArray FcgiClient::BuildBeginRequestPacket(uint16_t ID) const
{
    uint16_t length = sizeof(FCGI_BeginRequestBody);

    FCGI_BeginRequestRecord record = {};
    record.header.version = static_cast<uint8_t>(FCGI_VERSION_1);
    record.header.type = static_cast<uint8_t>(RequestType::FCGI_BEGIN_REQUEST);
    record.header.requestIdB0 = static_cast<uint8_t>(ID & 0xFF);
    record.header.requestIdB1 = static_cast<uint8_t>(ID >> 8 & 0xFF);
    record.header.contentLengthB0 = static_cast<uint8_t>(length & 0xFF);
    record.header.contentLengthB1 = static_cast<uint8_t>(length >> 8 & 0xFF);

    uint16_t role = static_cast<uint16_t>(RequestRole::FCGI_RESPONDER);
    record.body.roleB0 = static_cast<uint8_t>(role & 0xFF);
    record.body.roleB1 = static_cast<uint8_t>(role >> 8 & 0xFF);
    record.body.flags = (m_keepConnection ? 1 : 0);

    ByteArray data(sizeof(record));
    std::memcpy(data.data(), &record, sizeof(record));
    return data;
}

ByteArray FcgiClient::BuildParamPacket(const std::string &name, const std::string &value) const
{
    ByteArray data;
    size_t nlen = name.size();
    size_t vlen = value.size();
    char *ptr;
    size_t recordSize;

    if(nlen < 128)
    {
        FCGI_Name1 nameRecord;
        nameRecord.nameLengthB0 = nlen;
        ptr = reinterpret_cast<char *>(&nameRecord);
        recordSize = sizeof(nameRecord);
        data.insert(data.end(), ptr, ptr + recordSize);
    }
    else
    {
        FCGI_Name2 nameRecord;
        nameRecord.nameLengthB0 = nlen & 0xFF;
        nameRecord.nameLengthB1 = nlen >> 8 & 0xFF;
        nameRecord.nameLengthB2 = nlen >> 16 & 0xFF;
        nameRecord.nameLengthB3 = nlen >> 24 | 0x80;
        ptr = reinterpret_cast<char *>(&nameRecord);
        recordSize = sizeof(nameRecord);
        data.insert(data.end(), ptr, ptr + recordSize);
    }

    if(vlen < 128)
    {
        FCGI_Value1 valueRecord;
        valueRecord.valueLengthB0 = vlen;
        ptr = reinterpret_cast<char *>(&valueRecord);
        recordSize = sizeof(valueRecord);
        data.insert(data.end(), ptr, ptr + recordSize);
    }
    else
    {
        FCGI_Value2 valueRecord;
        valueRecord.valueLengthB0 = vlen & 0xFF;
        valueRecord.valueLengthB1 = vlen >> 8 & 0xFF;
        valueRecord.valueLengthB2 = vlen >> 16 & 0xFF;
        valueRecord.valueLengthB3 = vlen >> 24 | 0x80;
        ptr = reinterpret_cast<char *>(&valueRecord);
        recordSize = sizeof(valueRecord);
        data.insert(data.end(), ptr, ptr + recordSize);
    }

    data.insert(data.end(), name.begin(), name.end());
    data.insert(data.end(), value.begin(), value.end());

    return data;
}

ByteArray FcgiClient::BuildParamsPacket(uint16_t ID, const ByteArray &params) const
{
    ByteArray data;

    FCGI_Header header;
    size_t dataSize = params.size();
    header.version = static_cast<uint8_t>(FCGI_VERSION_1);
    header.type = static_cast<uint8_t>(RequestType::FCGI_PARAMS);
    header.requestIdB0 = static_cast<uint8_t>(ID & 0xFF);
    header.requestIdB1 = static_cast<uint8_t>(ID >> 8 & 0xFF);
    header.contentLengthB0 = dataSize & 0xFF;
    header.contentLengthB1 = dataSize >> 8 & 0xFF;

    char *ptr = reinterpret_cast<char *>(&header);

    data.insert(data.begin(), ptr, ptr + sizeof(header));

    if(params.size() > 0)
    {
        data.insert(data.end(), params.begin(), params.end());
    }

    return data;
}

ByteArray FcgiClient::BuildStdinPacket(uint16_t ID, const ByteArray &stdinData) const
{
    FCGI_Header header;
    size_t dataSize = stdinData.size();
    header.version = static_cast<uint8_t>(FCGI_VERSION_1);
    header.type = static_cast<uint8_t>(RequestType::FCGI_STDIN);
    header.requestIdB0 = static_cast<uint8_t>(ID & 0xFF);
    header.requestIdB1 = static_cast<uint8_t>(ID >> 8 & 0xFF);
    header.contentLengthB0 = dataSize & 0xFF;
    header.contentLengthB1 = dataSize >> 8 & 0xFF;

    ByteArray data;
    char *ptr = reinterpret_cast<char *>(&header);
    data.insert(data.begin(), ptr, ptr + sizeof(header));
    if(dataSize > 0)
    {
        data.insert(data.begin(), stdinData.begin(), stdinData.end());
    }

    return data;
}

void FcgiClient::OnDataReady(ByteArray &data)
{
    FCGI_Header header;
    size_t headerSize = sizeof(header);
    size_t dataSize = data.size();
    if(dataSize >= headerSize)
    {
        std::memcpy(&header, data.data(), headerSize);
        uint16_t ID = header.requestIdB0 & 0x00FF & header.requestIdB1 & 0xFF00;
        auto &responseData = GetResponseData(ID);
        if(!responseData.IsEmpty())
        {
            responseData.data.insert(responseData.data.end(), data.begin(), data.end());
            std::memcpy(&header, data.data() + dataSize - headerSize, headerSize);
            RequestType type = static_cast<RequestType>(header.type);
            if(type == RequestType::FCGI_END_REQUEST)
            {
                ProcessResponse(ID);
            }
        }
    }


    std::cout << "data ready: " << StringUtil::ByteArray2String(data) << std::endl;
    size_t dataSize = data.size();
    size_t pos = 0;
    FCGI_Header header;
    ProtocolStatus result;
    uint32_t appResult;

    while((dataSize - pos) >= sizeof(header))
    {
        std::memcpy(&header, data.data() + pos, sizeof(header));
        //uint16_t ID = header.requestIdB0 & 0x00FF | header.requestIdB1 << 8 & 0xFF00;
        uint16_t contentLength = header.contentLengthB0 & 0x00FF | header.contentLengthB1 << 8 & 0xFF00;
        RequestType type = static_cast<RequestType>(header.type);
        pos += sizeof(header);

        switch(type)
        {
            case RequestType::FCGI_STDOUT:
                if((dataSize - pos) >= contentLength)
                {
                    m_content.insert(m_content.end(), data.begin() + pos, data.begin() + pos + contentLength);
                }
                break;
            case RequestType::FCGI_END_REQUEST:
                if((dataSize - pos) >= contentLength)
                {
                    FCGI_EndRequestBody endRequest;
                    std::memcpy(&endRequest, data.data() + pos, sizeof(endRequest));
                    result = static_cast<ProtocolStatus>(endRequest.protocolStatus);
                    appResult = endRequest.appStatusB0 & 0x000000FF |
                            endRequest.appStatusB1 << 8 & 0x0000FF00 |
                                                      endRequest.appStatusB2 << 16 & 0x00FF0000 |
                                                      endRequest.appStatusB2 << 24 & 0xFF000000;
                }
                break;
            default: break;
        }
        pos += contentLength;
        pos += header.paddingLength;
    }
}

void FcgiClient::OnConnectionClosed()
{
    std::cout << "connection closed" << std::endl;
}

ByteArray FcgiClient::ReadData()
{

}

FcgiClient::ResponseData &FcgiClient::GetResponseData(int ID)
{
    for(int i = 0;i < m_responseQueue.size(); i++)
    {
        if(m_responseQueue[i].ID == ID)
        {
            return m_responseQueue[i];
        }
    }

    return FcgiClient::ResponseData::DefaultResponseData;
}

void FcgiClient::ProcessResponse(int ID)
{
    auto &fcgiResponseData = GetResponseData(ID);
    auto &data = fcgiResponseData.data;
    ByteArray responseData;

    size_t dataSize = data.size();
    size_t pos = 0;
    FCGI_Header header;
    ProtocolStatus result;
    uint32_t appResult;

    while((dataSize - pos) >= sizeof(header))
    {
        std::memcpy(&header, data.data() + pos, sizeof(header));
        //uint16_t ID = header.requestIdB0 & 0x00FF | header.requestIdB1 << 8 & 0xFF00;
        uint16_t contentLength = header.contentLengthB0 & 0x00FF | header.contentLengthB1 << 8 & 0xFF00;
        RequestType type = static_cast<RequestType>(header.type);
        pos += sizeof(header);

        switch(type)
        {
            case RequestType::FCGI_STDOUT:
                if((dataSize - pos) >= contentLength)
                {
                    responseData.insert(responseData.end(), data.begin() + pos, data.begin() + pos + contentLength);
                }
                break;
            case RequestType::FCGI_END_REQUEST:
                if((dataSize - pos) >= contentLength)
                {
                    FCGI_EndRequestBody endRequest;
                    std::memcpy(&endRequest, data.data() + pos, sizeof(endRequest));
                    result = static_cast<ProtocolStatus>(endRequest.protocolStatus);
                    appResult = endRequest.appStatusB0 & 0x000000FF |
                            endRequest.appStatusB1 << 8 & 0x0000FF00 |
                                                      endRequest.appStatusB2 << 16 & 0x00FF0000 |
                                                      endRequest.appStatusB2 << 24 & 0xFF000000;
                }
                break;
            default: break;
        }
        pos += contentLength;
        pos += header.paddingLength;
    }

    HttpHeader httpHeader;
    httpHeader.Parse(responseData);

    Response response(fcgiResponseData.connID, m_config);
    response.Write(responseData);

    return m_content;
}



