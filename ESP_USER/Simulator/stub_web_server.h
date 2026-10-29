/* stub_web_server.h - وب‌سرور شبیه‌سازی‌شده روی سوکت واقعی TCP
   فقط فارسی - مثل بقیهٔ این پوشه

   همان APIیی که خود برنامه صدا می‌زند پیاده شده است: on / begin / handleClient /
   hasArg / arg / client / send / sendHeader / setContentLength / sendContent /
   sendContent_P. پس هندلرهای واقعی پنل بدون یک خط تغییر اجرا می‌شوند.

   دو تفاوت آگاهانه با کتابخانهٔ برد:
   ۱) بدنهٔ پاسخ در حافظه بافر می‌شود و در پایان هندلر با Content-Length فرستاده
      می‌شود. برد در حالت setContentLength(CONTENT_LENGTH_UNKNOWN) چانک‌دار
      می‌فرستد؛ نتیجه برای مرورگر یکی است و اینجا لازم نیست مسیر چانک پیاده شود.
   ۲) هر پاسخ با Connection: close بسته می‌شود (بدون keep-alive)؛ سادگی و
      پیش‌بینی‌پذیری مهم‌تر از صرفه‌جویی سوکت در یک سیمولاتور است.

   نام‌گذاری متغیرها مطابق قانون پنل است (`type__name` و `TYPE__G__NAME`).
*/
#ifndef SIM_STUB_WEB_SERVER_H
#define SIM_STUB_WEB_SERVER_H

#include "stub_arduino.h"

#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <map>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include <vector>

/* ==================== متدهای HTTP ==================== */
enum
{
    HTTP_ANY = 0,
    HTTP_GET = 1,
    HTTP_POST = 2,
    HTTP_PUT = 3,
    HTTP_DELETE = 4
};

/* ==================== نشانی کلاینت ==================== */
#ifndef SIM_IPADDRESS_DEFINED
#define SIM_IPADDRESS_DEFINED
class IPAddress
{
public:
    IPAddress() { octets[0] = octets[1] = octets[2] = octets[3] = 0u; }
    IPAddress(uint8_t uint8_t__a, uint8_t uint8_t__b, uint8_t uint8_t__c, uint8_t uint8_t__d)
    {
        octets[0] = uint8_t__a;
        octets[1] = uint8_t__b;
        octets[2] = uint8_t__c;
        octets[3] = uint8_t__d;
    }
    uint8_t operator[](int int__index) const
    {
        return ((int__index >= 0) && (int__index < 4)) ? octets[int__index] : 0u;
    }

private:
    uint8_t octets[4];
};
#endif

class SimWebClient
{
public:
    explicit SimWebClient(const IPAddress &ip_address__address) : address(ip_address__address) {}
    IPAddress remoteIP(void) const { return address; }
    bool connected(void) const { return true; }
    void setTimeout(unsigned long ulong__ms) { (void)ulong__ms; }
    void stop(void) {}

private:
    IPAddress address;
};

typedef void (*SimHttpHandler)(void);
typedef bool (*SimResponseRewriter)(const char *char__ptr_uri, std::string &string__body);

/* ==================== وب‌سرور ==================== */
/* پایین‌تر تعریف می‌شود؛ simCall برای خواندن پارامترهای پرس‌وجو از آن استفاده می‌کند. */
inline void func__Sim_ParsePairs(const std::string &string__text,
                                 std::map<std::string, std::string> &map_string__out);

class SimWebServerT
{
public:
    explicit SimWebServerT(int int__port) : port(int__port) {}
    ~SimWebServerT() { closeListenSocket(); }

    void on(const char *char__ptr_uri, int int__method, SimHttpHandler func__handler)
    {
        Route route__route;
        route__route.uri = (char__ptr_uri != NULL) ? char__ptr_uri : "/";
        route__route.method = int__method;
        route__route.handler = func__handler;
        routes.push_back(route__route);
    }

    void begin(void);
    void handleClient(void);
    /* مسیرها در خود برنامه *پیش از* begin ثبت می‌شوند، پس begin نباید پاکشان
       کند؛ پاک‌کردن فقط هنگام ریستارت شبیه‌سازی‌شده لازم است */
    void clearRoutes(void) { routes.clear(); }

    bool hasArg(const char *char__ptr_name) const;
    String arg(const char *char__ptr_name) const;
    SimWebClient client(void) const { return SimWebClient(clientAddress); }
    void keepAlive(bool bool__enabled) { (void)bool__enabled; }

    void sendHeader(const char *char__ptr_name, const char *char__ptr_value)
    {
        if ((char__ptr_name != NULL) && (char__ptr_value != NULL))
        {
            headers[char__ptr_name] = char__ptr_value;
        }
    }
    void setContentLength(size_t size_t__length) { declaredLength = size_t__length; }

    void send(int int__code, const char *char__ptr_contentType, const char *char__ptr_body)
    {
        status = int__code;
        contentType = (char__ptr_contentType != NULL) ? char__ptr_contentType : "text/plain";
        responseBody = (char__ptr_body != NULL) ? char__ptr_body : "";
    }
    void send(int int__code, const char *char__ptr_contentType, const String &string__body)
    {
        send(int__code, char__ptr_contentType, string__body.c_str());
    }
    void sendContent(const String &string__body) { responseBody += string__body.c_str(); }
    void sendContent(const char *char__ptr_body)
    {
        if (char__ptr_body != NULL) { responseBody += char__ptr_body; }
    }
    void sendContent_P(const char *char__ptr_body, size_t size_t__length)
    {
        if (char__ptr_body != NULL)
        {
            responseBody.append(char__ptr_body, size_t__length);
        }
    }

    /* قلاب شبیه‌ساز: پیش از فرستادن پاسخ، بدنه را دست‌کاری می‌کند
       (برای تزریق تب سیمولاتور در صفحهٔ "/") */
    void setResponseRewriter(SimResponseRewriter func__rewriter) { rewriter = func__rewriter; }

    const char *requestUri(void) const { return requestUriText.c_str(); }
    int requests(void) const { return requestCount; }

    /* قلاب شبیه‌ساز: فراخوانی مسیر در همان فرآیند، بی‌سوکت.
       تست هاست با این کار می‌تواند سرسطرها و بدنهٔ پاسخ را ببیند. */
    void simClear(void)
    {
        args.clear();
        headers.clear();
        responseBody.clear();
        status = 200;
        contentType = "text/plain";
    }
    void simCall(const char *char__ptr_uri, int int__method, const char *char__ptr_query = "")
    {
        simClear();
        requestUriText = (char__ptr_uri != NULL) ? char__ptr_uri : "/";
        queryText = (char__ptr_query != NULL) ? char__ptr_query : "";
        method = int__method;
        if (!queryText.empty())
        {
            (void)func__Sim_ParsePairs(queryText, args);
        }
        dispatch();
    }
    std::string simHeader(const char *char__ptr_name) const
    {
        std::map<std::string, std::string>::const_iterator iterator__found;
        std::string string__name = (char__ptr_name != NULL) ? char__ptr_name : "";

        iterator__found = headers.find(string__name);
        return (iterator__found != headers.end()) ? iterator__found->second : std::string();
    }
    const std::string &simBody(void) const { return responseBody; }

private:
    struct Route
    {
        std::string uri;
        int method;
        SimHttpHandler handler;
    };

    bool serveOne(int int__fd);
    bool readRequest(int int__fd, std::string &string__raw);
    void parseRequest(const std::string &string__raw);
    void dispatch(void);
    void writeResponse(int int__fd);
    void closeListenSocket(void);
    void clearRequest(void);

    int port;
    int listenFd = -1;
    std::vector<Route> routes;

    std::string requestUriText;
    std::string queryText;
    std::map<std::string, std::string> args;
    int method = HTTP_GET;
    int status = 200;
    std::string contentType = "text/plain";
    std::string responseBody;
    std::map<std::string, std::string> headers;
    size_t declaredLength = 0u;
    IPAddress clientAddress;
    SimResponseRewriter rewriter = NULL;
    int requestCount = 0;
};

/* ==================== پیاده‌سازی ==================== */
inline std::string func__Sim_UrlDecode(const std::string &string__text)
{
    std::string string__decoded;
    for (size_t size_t__index = 0u; size_t__index < string__text.size(); size_t__index++)
    {
        char char__current = string__text[size_t__index];
        if ((char__current == '%') && ((size_t__index + 2u) < string__text.size()))
        {
            std::string string__hex = string__text.substr(size_t__index + 1u, 2u);
            string__decoded.push_back((char)strtol(string__hex.c_str(), NULL, 16));
            size_t__index += 2u;
        }
        else if (char__current == '+')
        {
            string__decoded.push_back(' ');
        }
        else
        {
            string__decoded.push_back(char__current);
        }
    }
    return string__decoded;
}

inline void func__Sim_ParsePairs(const std::string &string__text,
                                 std::map<std::string, std::string> &map_string__out)
{
    size_t size_t__start = 0u;
    while (size_t__start < string__text.size())
    {
        size_t size_t__end = string__text.find('&', size_t__start);
        if (size_t__end == std::string::npos) { size_t__end = string__text.size(); }

        std::string string__pair = string__text.substr(size_t__start, size_t__end - size_t__start);
        size_t size_t__equals = string__pair.find('=');
        if (size_t__equals != std::string::npos)
        {
            map_string__out[func__Sim_UrlDecode(string__pair.substr(0u, size_t__equals))] =
                func__Sim_UrlDecode(string__pair.substr(size_t__equals + 1u));
        }
        else if (!string__pair.empty())
        {
            map_string__out[func__Sim_UrlDecode(string__pair)] = "";
        }
        size_t__start = size_t__end + 1u;
    }
}

inline void SimWebServerT::begin(void)
{
    closeListenSocket();

    /* پورت از محیط می‌آید چون برنامه روی برد پورت ۸۰ را bind می‌کند و
       bind پورت زیر ۱۰۲۴ روی PC نیاز به دسترسی ریشه دارد */
    int int__port = port;
    const char *char__ptr_overridePort = getenv("SIMPLE_SIM_PORT");
    if ((char__ptr_overridePort != NULL) && (char__ptr_overridePort[0] != '\0'))
    {
        int__port = atoi(char__ptr_overridePort);
    }
    else if (int__port < 1024)
    {
        int__port = 3000;
        printf("[sim] پورت %d روی برد است و اینجا bind نمی‌شود؛ پورت %d استفاده می‌شود\n",
               port, int__port);
    }

    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0)
    {
        perror("socket");
        exit(1);
    }
    /* سوکت با exec بسته شود: «ریستارت» شبیه‌ساز برنامه را از نو اجرا می‌کند
       و اگر این توصیفگر باز بماند، پورت آزاد نمی‌شود و bind بعدی می‌شکند */
    (void)fcntl(listenFd, F_SETFD, FD_CLOEXEC);

    int int__reuse = 1;
    (void)setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &int__reuse, sizeof(int__reuse));

    struct sockaddr_in sockaddr__address;
    memset(&sockaddr__address, 0, sizeof(sockaddr__address));
    sockaddr__address.sin_family = AF_INET;
    /* 0.0.0.0 تا پیش‌نمایش بیرونی سندباکس هم کار کند */
    sockaddr__address.sin_addr.s_addr = htonl(INADDR_ANY);
    sockaddr__address.sin_port = htons((uint16_t)int__port);
    if (bind(listenFd, (struct sockaddr *)&sockaddr__address, sizeof(sockaddr__address)) != 0)
    {
        perror("bind");
        exit(1);
    }
    if (listen(listenFd, 8) != 0)
    {
        perror("listen");
        exit(1);
    }
    printf("[sim] پنل روی http://0.0.0.0:%d بالا آمد\n", int__port);
}

inline void SimWebServerT::closeListenSocket(void)
{
    if (listenFd >= 0)
    {
        close(listenFd);
        listenFd = -1;
    }
}

inline void SimWebServerT::handleClient(void)
{
    if (listenFd < 0) { return; }

    for (int int__served = 0; int__served < 4; int__served++)
    {
        struct pollfd pollfd__waiting;
        pollfd__waiting.fd = listenFd;
        pollfd__waiting.events = POLLIN;
        pollfd__waiting.revents = 0;
        if ((poll(&pollfd__waiting, 1, 0) <= 0) || ((pollfd__waiting.revents & POLLIN) == 0))
        {
            return;
        }

        struct sockaddr_in sockaddr__peer;
        socklen_t socklen__peerLength = sizeof(sockaddr__peer);
        int int__fd = accept(listenFd, (struct sockaddr *)&sockaddr__peer, &socklen__peerLength);
        if (int__fd >= 0) { (void)fcntl(int__fd, F_SETFD, FD_CLOEXEC); }
        if (int__fd < 0) { return; }

        uint32_t uint32_t__peerAddress = ntohl(sockaddr__peer.sin_addr.s_addr);
        clientAddress = IPAddress((uint8_t)((uint32_t__peerAddress >> 24) & 0xFFu),
                                  (uint8_t)((uint32_t__peerAddress >> 16) & 0xFFu),
                                  (uint8_t)((uint32_t__peerAddress >> 8) & 0xFFu),
                                  (uint8_t)(uint32_t__peerAddress & 0xFFu));
        (void)serveOne(int__fd);
        close(int__fd);
    }
}

inline bool SimWebServerT::serveOne(int int__fd)
{
    struct timeval timeval__timeout;
    timeval__timeout.tv_sec = 1;
    timeval__timeout.tv_usec = 0;
    (void)setsockopt(int__fd, SOL_SOCKET, SO_RCVTIMEO, &timeval__timeout, sizeof(timeval__timeout));

    std::string string__raw;
    if (!readRequest(int__fd, string__raw)) { return false; }

    parseRequest(string__raw);
    dispatch();
    if (rewriter != NULL)
    {
        (void)rewriter(requestUriText.c_str(), responseBody);
    }
    writeResponse(int__fd);

    printf("[sim] %s %s -> %d (%lu بایت)\n", (method == HTTP_POST) ? "POST" : "GET",
           requestUriText.c_str(), status, (unsigned long)responseBody.size());
    fflush(stdout);
    requestCount++;
    return true;
}

inline bool SimWebServerT::readRequest(int int__fd, std::string &string__raw)
{
    char char__buffer[4096];
    size_t size_t__headerEnd = std::string::npos;
    size_t size_t__contentLength = 0u;

    for (;;)
    {
        ssize_t ssize_t__got = recv(int__fd, char__buffer, sizeof(char__buffer), 0);
        if (ssize_t__got <= 0) { break; }

        string__raw.append(char__buffer, (size_t)ssize_t__got);
        if (size_t__headerEnd == std::string::npos)
        {
            size_t__headerEnd = string__raw.find("\r\n\r\n");
            if (size_t__headerEnd == std::string::npos) { continue; }

            size_t size_t__lengthPos = string__raw.find("Content-Length:");
            if (size_t__lengthPos == std::string::npos)
            {
                size_t__lengthPos = string__raw.find("content-length:");
            }
            if (size_t__lengthPos != std::string::npos)
            {
                size_t__contentLength =
                    (size_t)strtoul(string__raw.c_str() + size_t__lengthPos + 15u, NULL, 10);
            }
        }
        if ((size_t__headerEnd != std::string::npos) &&
            (string__raw.size() >= (size_t__headerEnd + 4u + size_t__contentLength)))
        {
            return true;
        }
    }
    return size_t__headerEnd != std::string::npos;
}

inline void SimWebServerT::clearRequest(void)
{
    args.clear();
    headers.clear();
    requestUriText.clear();
    queryText.clear();
    responseBody.clear();
    status = 200;
    contentType = "text/plain";
    declaredLength = 0u;
}

inline void SimWebServerT::parseRequest(const std::string &string__raw)
{
    clearRequest();

    size_t size_t__lineEnd = string__raw.find("\r\n");
    std::string string__requestLine =
        (size_t__lineEnd == std::string::npos) ? string__raw : string__raw.substr(0u, size_t__lineEnd);

    size_t size_t__firstSpace = string__requestLine.find(' ');
    size_t size_t__secondSpace = string__requestLine.find(' ', size_t__firstSpace + 1u);
    std::string string__methodText = string__requestLine.substr(0u, size_t__firstSpace);
    std::string string__target = string__requestLine.substr(
        size_t__firstSpace + 1u, size_t__secondSpace - size_t__firstSpace - 1u);

    method = (string__methodText == "POST") ? HTTP_POST : HTTP_GET;

    size_t size_t__question = string__target.find('?');
    requestUriText = (size_t__question == std::string::npos)
                         ? string__target
                         : string__target.substr(0u, size_t__question);
    if (size_t__question != std::string::npos)
    {
        queryText = string__target.substr(size_t__question + 1u);
        func__Sim_ParsePairs(queryText, args);
    }

    size_t size_t__headerEnd = string__raw.find("\r\n\r\n");
    if (size_t__headerEnd == std::string::npos) { return; }

    std::string string__headerBlock =
        string__raw.substr(size_t__lineEnd + 2u, size_t__headerEnd - size_t__lineEnd - 2u);
    bool bool__formBody =
        string__headerBlock.find("application/x-www-form-urlencoded") != std::string::npos;
    std::string string__body = string__raw.substr(size_t__headerEnd + 4u);
    if (!string__body.empty())
    {
        /* رفتار ESP8266WebServer: بدنهٔ فرم به آرگومان‌ها باز می‌شود و
           بدنهٔ غیرفرم (مثل JSON) به‌صورت arg("plain") در دسترس است */
        if (bool__formBody)
        {
            func__Sim_ParsePairs(string__body, args);
        }
        else
        {
            args["plain"] = string__body;
        }
    }
}

inline void SimWebServerT::dispatch(void)
{
    for (size_t size_t__index = 0u; size_t__index < routes.size(); size_t__index++)
    {
        const Route &route__current = routes[size_t__index];
        if (route__current.uri != requestUriText) { continue; }
        if ((route__current.method != HTTP_ANY) && (route__current.method != method)) { continue; }

        if (route__current.handler != NULL)
        {
            route__current.handler();
        }
        return;
    }
    status = 404;
    contentType = "text/plain";
    responseBody = "not found (sim)\n";
}

inline void SimWebServerT::writeResponse(int int__fd)
{
    std::string string__statusText;
    switch (status)
    {
        case 200: string__statusText = "200 OK"; break;
        case 400: string__statusText = "400 Bad Request"; break;
        case 401: string__statusText = "401 Unauthorized"; break;
        case 403: string__statusText = "403 Forbidden"; break;
        case 404: string__statusText = "404 Not Found"; break;
        default:  string__statusText = "200 OK"; status = 200; break;
    }

    std::string string__response = "HTTP/1.1 " + string__statusText + "\r\n";
    string__response += "Content-Type: " + contentType + "\r\n";
    string__response += "Content-Length: " + std::to_string(responseBody.size()) + "\r\n";
    string__response += "Connection: close\r\n";
    for (std::map<std::string, std::string>::const_iterator iterator__header = headers.begin();
         iterator__header != headers.end();
         ++iterator__header)
    {
        string__response += iterator__header->first + ": " + iterator__header->second + "\r\n";
    }
    string__response += "\r\n";
    string__response += responseBody;

    size_t size_t__sent = 0u;
    while (size_t__sent < string__response.size())
    {
        ssize_t ssize_t__wrote = ::send(int__fd,
                                        string__response.data() + size_t__sent,
                                        string__response.size() - size_t__sent,
                                        MSG_NOSIGNAL);
        if (ssize_t__wrote <= 0)
        {
            if ((errno == EINTR) || (errno == EAGAIN)) { continue; }
            break;
        }
        size_t__sent += (size_t)ssize_t__wrote;
    }
}

inline bool SimWebServerT::hasArg(const char *char__ptr_name) const
{
    return (char__ptr_name != NULL) && (args.find(char__ptr_name) != args.end());
}

inline String SimWebServerT::arg(const char *char__ptr_name) const
{
    if (char__ptr_name == NULL) { return String(); }

    std::map<std::string, std::string>::const_iterator iterator__found = args.find(char__ptr_name);
    return (iterator__found == args.end()) ? String() : String(iterator__found->second);
}

typedef SimWebServerT ESP8266WebServer;

#endif /* SIM_STUB_WEB_SERVER_H */
