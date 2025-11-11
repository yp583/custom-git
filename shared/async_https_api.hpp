#ifndef ASYNC_HTTPS_API_HPP
#define ASYNC_HTTPS_API_HPP

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <functional>
#include <memory>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <sys/event.h>
#include <sys/time.h>

using namespace std;

typedef enum {
    CONNECTING,
    TLS_HANDSHAKE,
    WRITING_REQUEST,
    READING_RESPONSE,
    DONE,
    ERROR
} conn_state_t;

struct HTTPSRequest {
    int socket_fd;
    SSL* ssl;
    SSL_CTX* ssl_ctx;

    // HTTP State
    conn_state_t state;

    // Request data
    string host;
    string path;
    string send_buffer;
    size_t bytes_sent = 0;
    
    // Response data
    string recv_headers;
    string recv_body;
    int content_length = -1;

    // Callback for completion
    function<void(const string& response, bool success)> callback;
    
    HTTPSRequest(const string& h, const string& p) : host(h), path(p) {
        SSL_load_error_strings();
        SSL_library_init();
        ssl_ctx = SSL_CTX_new(TLS_client_method());
        ssl = nullptr;
    }
    ~HTTPSRequest() {
        if (ssl) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
        }
        if (ssl_ctx) {
            SSL_CTX_free(ssl_ctx);
        }
        if (socket_fd >= 0) {
            close(socket_fd);
        }
    }
};

class AsyncHTTPSConnection {
private:
    int kqueue_fd;
    unordered_map<int, unique_ptr<HTTPSRequest>> reqs;
    void handle_connect(HTTPSRequest* req, uint32_t filter);
    void handle_tls(HTTPSRequest* req, uint32_t filter);
    void handle_write(HTTPSRequest* req, uint32_t filter);
    void handle_read(HTTPSRequest* req, uint32_t filter);
    void cleanup(HTTPSRequest* req);
public:
    AsyncHTTPSConnection();
    void post_async(const string& host, const string& path, const string& body, const vector<pair<string, string>>& headers, function<void(const string&, bool)> callback);
    void run_loop();
    ~AsyncHTTPSConnection();
};

#endif 
