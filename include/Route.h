#ifndef ROUTE_H
#define ROUTE_H

#include <functional>
#include <map>
#include <algorithm>
#include "Request.h"
#include "Response.h"


namespace WebCpp
{

class Route
{
public:
    using RouteFunc = std::function<bool(const Request&request, Response &response)>;

    Route(const std::string &path, HttpHeader::Method method);
    Route(const Route& other) = delete;
    Route& operator=(const Route& other) = delete;
    Route(Route&& other) = default;
    Route& operator=(Route&& other) = default;

    bool SetFunction(const RouteFunc& f);
    const RouteFunc& GetFunction() const;
    bool IsMatch(Request &request);

    std::string ToString() const;

protected:
    bool Parse(const std::string &path);
    struct Token
    {
        enum class Type
        {
            Default = 0,
            Variable,
            Group,
        };
        enum class View
        {
            Default = 0,
            Alpha,
            Numeric,
            String,
            Upper,
            Lower,
        };

        std::string text = "";
        std::vector<std::string> group;
        Type type = Type::Default;
        View view = View::Default;
        bool optional = false;

        size_t GetLength() const
        {
            return text.length();
        }

        void Clear()
        {
            text = "";
            type = Type::Default;
            view = View::Default;
            group.clear();
            group.shrink_to_fit();
        }
        bool IsEmpty()
        {
            return (text.empty() && group.size() == 0);
        }
        void SortGroup()
        {
            std::sort(group.begin(), group.end(), [](const std::string& first, const std::string& second)
            {
                if(first.size() != second.size())
                {
                    return first.size() > second.size();
                }
                return first > second;

            });
        }

        bool IsMatch(const char *ch, size_t length, size_t& pos);
        bool IsString(char ch) const;
        bool IsAlpha(char ch) const;
        bool IsNumeric(char ch) const;
        bool IsLower(char ch) const;
        bool IsUpper(char ch) const;

        static View String2View(const std::string &str);
        bool Compare(const char *ch1, const char *ch2, size_t size);
    };

    bool AddToken(Token &token, const std::string &str);

private:
    enum class State
    {
        Default = 0,
        Variable,
        Optional,
        VariableType,
        OrGroup,
    };

    RouteFunc m_func;
    std::vector<Token> m_tokens;
    HttpHeader::Method m_method;
    std::string m_path;
};

}

#endif // ROUTE_H
