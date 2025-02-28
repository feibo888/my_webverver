//
// Created by fb on 12/27/24.
//

#include "httprequest.h"
using namespace std;

const unordered_set<string> HttpRequest::DEFAULT_HTML
{
    "/index", "/register", "/login",
     "/welcome", "/video", "/picture",
};

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG
{
    {"/register.html", 0}, {"/login.html", 1},
};

void HttpRequest::Init()
{
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

bool HttpRequest::parse(Buffer& buff)
{
    LOG_DEBUG("Starting parse");
    const char CRLF[] = "\r\n";
    if (buff.ReadableBytes() <= 0)
    {
        return false;
    }
    while (buff.ReadableBytes() && state_ != FINISH)
    {
        //Peek: BeginPtr_() + readPos_;
        //buff: BeginPtr_() + writePos_;
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        string line(buff.Peek(), lineEnd);      //去除了\r\n的一行

        switch (state_)
        {
        case REQUEST_LINE:      //请求行
            if (!ParseRequestLine_(line))
            {
                return false;
            }
            ParsePath_();
            break;
        case HEADERS:           //请求头
            ParseHeader_(line);
            if (buff.ReadableBytes() <= 2)
            {
                state_ = FINISH;
            }
            break;
        case BODY:              //请求数据
            ParseBody_(line);
            break;
        default:
            break;
        }
        if (lineEnd == buff.BeginWriteConst())
        {
            buff.RetrieveUntil(lineEnd);
            state_ = FINISH;
            break;
        }
        buff.RetrieveUntil(lineEnd + 2);        //更新readPos_的位置到下一行，否则readPos_一直不会变，导致readBuf过大
    }
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;

}

std::string HttpRequest::path() const
{
    return path_;
}

std::string& HttpRequest::path()
{
    return path_;
}

std::string HttpRequest::method() const
{
    return method_;
}

std::string HttpRequest::version() const
{
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const
{
    assert(key != "");
    if (post_.count(key) == 1)
    {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const
{
    assert(key != "");
    if (post_.count(key) == 1)
    {
        return post_.find(key)->second;
    }
    return "";
}

bool HttpRequest::IsKeepAlive() const
{
    if (header_.count("Connection") == 1)
    {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;


    // auto it = header_.find("connection");
    // if (it != header_.end()) {
    //     std::string value = it->second;
    //     std::transform(value.begin(), value.end(), value.begin(), ::tolower);  // 转小写
    //     return value == "keep-alive" && version_ == "1.1";
    // }
    // return false;
}

bool HttpRequest::ParseRequestLine_(const std::string& line)
{
    LOG_DEBUG("Raw request line: |%s|", line.c_str()); // 调试日志

    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$" );
    smatch subMatch;
    if (regex_match(line, subMatch, patten))
    {
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;
        LOG_DEBUG("%s, %s, %s", method_.c_str(), path_.c_str(), version_.c_str());
        return true;
    }

    LOG_ERROR("RequestLine Error");
    return false;
}

void HttpRequest::ParseHeader_(const std::string& line)
{
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if (regex_match(line, subMatch, patten))
    {
        header_[subMatch[1]] = subMatch[2];
        // string header_name = subMatch[1];
        // std::transform(header_name.begin(), header_name.end(), header_name.begin(), ::tolower);  // 转小写
        // header_[header_name] = subMatch[2];
    }
    else
    {
        state_ = BODY;
    }
}

void HttpRequest::ParseBody_(const std::string& line)
{
    body_ = line;
    ParsePost_();
    state_ = FINISH;
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

void HttpRequest::ParsePath_()
{
    if (path_ == "/")
    {
        path_ = "index.html";
    }
    else
    {
        for (auto &item : DEFAULT_HTML)
        {
            if (item == path_)
            {
                path_ += ".html";
                break;
            }
        }
    }
}

void HttpRequest::ParsePost_()
{

    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded")
    {
        ParseFromUrlencoded_();

        if (DEFAULT_HTML_TAG.count(path_))
        {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            LOG_DEBUG("Tag:%d", tag);
            if (tag == 0 || tag == 1)
            {
                bool isLogin = (tag == 1);
                if (UserVerify(post_["username"], post_["password"], isLogin))
                {
                    path_ = "/welcome.html";
                }
                else
                {
                    path_ = "/error.html";
                }
            }
        }
    }
}

void HttpRequest::ParseFromUrlencoded_()
{
    if (body_.size() == 0)
    {
        return;
    }

    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for (; i < n; ++i)
    {
        char ch = body_[i];
        switch (ch)
        {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            if (i + 2 >= n) break;  // 防止越界
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i] = static_cast<char>(num);  // 替换为 ASCII 字符
            body_.erase(i + 1, 2);             // 删除 % 后的两个字符
            n = body_.size();                   // 更新总长度
            break;


        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
            break;
        default:
            break;
        }
    }
    // 处理最后一个键值对
    // if (j < n) {
    //     value = body_.substr(j, i - j);
    //     post_[key] = value;
    //     LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
    // }

    assert(j <= i);
    if(post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }

}

bool HttpRequest::UserVerify(const std::string& name, const std::string& pwd, bool isLogin)
{
    if (name == "" || pwd == "") return false;
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    cout << name << " " << pwd << endl;

    // 从数据库连接池中获取一个MySQL连接
    MYSQL* sql;
    SqlConnRAll guard(&sql, SqlConnPool::Instance());
    //sql = SqlConnPool::Instance()->GetConn();
    assert(sql);

    bool flag = false;
    unsigned int j = 0;     // 字段数量
    char order[256] = {0};      // SQL语句缓冲区
    MYSQL_FIELD* fields = nullptr;      // 字段信息
    MYSQL_RES* res = nullptr;           // 查询结果集

    // 如果是注册操作，默认flag为true
    if (!isLogin)
    {
        flag = true;
    }

    //查询用户及密码
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);
    cout << order << endl;

    // 执行查询
    if (mysql_query(sql, order))
    {
        mysql_free_result(res);
        return false;
    }

    // 获取查询结果集
    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    while (MYSQL_ROW row = mysql_fetch_row(res))
    {
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        //注册行为且用户名未被使用
        if (isLogin)
        {
            if (pwd == password)
            {
                flag = true;
            }
            else
            {
                flag = false;
                LOG_DEBUG("pwd error!");
            }
        }
        else
        {
            flag = false;
            LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    //注册行为且用户名未被使用
    if (!isLogin && flag == true)
    {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s', '%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if (mysql_query(sql, order))
        {
            LOG_DEBUG( "Insert error!");
            flag = false;
        }
        flag = true;
    }

    //SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG( "UserVerify success!!");
    return flag;

}

int HttpRequest::ConverHex(char ch)
{
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch;
}
