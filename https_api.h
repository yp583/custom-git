#include <string>
#include <vector>
#include <openssl/ssl.h>

class APIConnection {
private:
    int fd;
    SSL *conn;
    SSL_CTX *ssl_ctx;
    std::string host;
    std::string path;
    
    void start_conn();

public:
    APIConnection(std::string url, std::string path);
    std::string post(std::string body, std::vector<std::pair<std::string, std::string>> headers);
    ~APIConnection();
};
