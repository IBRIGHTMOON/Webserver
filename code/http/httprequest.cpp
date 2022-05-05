#include "httprequest.h"
using namespace std;

std::unordered_map<std::string, std::string> users;

const unordered_set<string> HttpRequest::DEFAULT_HTML {
    "/index", "/register", "/login", "/music",
    "/welecome", "/video", "/picture",
};

const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
    {"/register.html", 0}, {"/login.html", 1},
};

void HttpRequest::Init() {
    method_ = path_ = version_ = body_ = "";
    state_ = REQUEST_LINE;
    header_.clear();
    post_.clear();
}

void HttpRequest::InitMysqlResult(SqlConnPool* connpool) {
    // 先从连接池中取出一个连接
    MYSQL* mysql = nullptr;
    SqlConnRAII mysqlconn(&mysql, connpool); 
    // 在user表中检索username，password数据
    if (mysql_query(mysql, "select username, password from user")) {
        return;
    }

    // 从表中检索完整的结果集
    MYSQL_RES* result = mysql_store_result(mysql);

    // 返回结果集中的列数
    int num_fileds = mysql_num_fields(result);

    // 返回所有字段结构的数组
    MYSQL_FIELD* fields = mysql_fetch_fields(result);

    // 从结果集中获取下一行，将对应的用户名和密码存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result)) {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
        //cout << temp1 << " " << temp2 << endl;
    }
}

bool HttpRequest::IsKeepAlive() const {
    if (header_.count("Proxy-Connection") == 1) {
        return header_.find("Proxy-Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";
    if (buff.readableBytes() <= 0) {
        return false;
    }
    while (buff.readableBytes() && state_ != FINISH) {
        const char* lineEnd = search(buff.curReadPtr(), buff.curWritePtrConst(), CRLF, CRLF + 2);
        std::string line(buff.curReadPtr(), lineEnd);
        switch (state_) {
        case REQUEST_LINE:
            if (!ParseRequestLine_(line)) {
                return false;
            }
            ParsePath_();
            break;
        case HEADERS:
            ParseHeader_(line);
            if (buff.readableBytes() <= 2) {
                state_ = FINISH;
            }
            break;
        case BODY:
            ParseBody_(line);
            break;
        default:
            break;
        }
        if (lineEnd == buff.curWritePtr()) break;
        buff.updateReadPtrUntilEnd(lineEnd + 2);
    }
    return true;
}

void HttpRequest::ParsePath_() {
    if (path_ == "/") {
        path_ = "/index.html";
    } else {
        for (auto &item : DEFAULT_HTML) {
            if (item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

bool HttpRequest::ParseRequestLine_(const string& line) {
    //regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    regex patten("^(.*) (.*) HTTP/(.*)$");
    smatch subMatch;
    if (regex_match(line, subMatch, patten)) {
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        state_ = HEADERS;
        return true;
    }
    return false;
}

void HttpRequest::ParseHeader_(const string& line) {
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if (regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    } else {
        state_ = BODY;
    }
}

void HttpRequest::ParseBody_(const string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;
}

int HttpRequest::ConverHex(char ch) {
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return ch;
}

void HttpRequest::ParsePost_() {
    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
    //if (method_ == "POST") {
        ParseFromeUrlencoded_();
        if (DEFAULT_HTML_TAG.count(path_)) {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            if (tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);
                int flag = UserVerify(post_["username"], post_["password"], isLogin);
                if (flag) {
                    if (flag == 1) path_ = "/welcome.html";
                    else if (flag == 2) path_ = "/login.html";
                } else {
                    path_ = "/error.html";
                }
            }
        }
    }
}

void HttpRequest::ParseFromeUrlencoded_() {
    if (body_.size() == 0) return;

    string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for (; i < n; ++i) {
        char ch = body_[i];
        switch (ch) {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            break;
        default:
            break;
        }
    }
    assert(j <= i);
    if (post_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}


int HttpRequest::UserVerify(const string& name, const string& pwd, bool isLogin) {
    if (name == "" || pwd == "") return false;

    int flag = 0;
    char order[256] = { 0 };
    if (isLogin) {
        if (users.count(name) && users[name] == pwd) {
            flag = 1;
        } else {
            flag = 0;
        }
    } else {
        MYSQL* sql;
        SqlConnRAII(&sql, SqlConnPool::Instance());
        assert(sql);
        bzero(order, 256);
//        cout << "qian" << endl;
        snprintf(order, 256, "insert into user(username, password) values('%s', '%s')", name.c_str(), pwd.c_str());
//        cout << "hou" << endl;
        if (mysql_query(sql, order)) {
            flag = 0;
        }
        flag = 2;
        SqlConnPool::Instance()->FreeConn(sql);
    }
    
    return flag;
}

std::string HttpRequest::path() const {
    return path_;
}

std::string& HttpRequest::path() {
    return path_;
}

std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if (post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if (post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}