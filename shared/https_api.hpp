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

using namespace std;

class APIConnection {
private:
    int fd;
    SSL *conn;
    SSL_CTX *ssl_ctx;
    string host;
    string path;

    void start_conn();

public:
    APIConnection(string url, string path);
    string recieve_length(int n);
    string recieve_sentinel(string sentinel);
    string recieve_chunked();
    void send(string request);
    string post(string body, vector<pair<string, string>> headers);
    ~APIConnection();
};

#endif // HTTPS_API_HPP
