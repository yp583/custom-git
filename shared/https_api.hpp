#ifndef HTTPS_API_HPP
#define HTTPS_API_HPP

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>
#include <unistd.h>
#include <vector>

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
    std::string recieve_length(int n);
    std::string recieve_sentinel(std::string sentinel);
    std::string recieve_chunked();
    void send(std::string request);
    std::string post(std::string body, std::vector<std::pair<std::string, std::string>> headers);
    ~APIConnection();
};

#endif // HTTPS_API_HPP
