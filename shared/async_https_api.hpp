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
    READING_RESPONSE_HEADERS,
    READING_RESPONSE,
    DONE,
    ERROR
} conn_state_t;

typedef enum {
    CONTENT_LENGTH,
    CHUNKED,
    CONNECTION_CLOSE,
} transfer_mode_t;

struct HTTPSRequest {
    int socket_fd;
    SSL* conn;
    SSL_CTX* ssl_ctx;

    // HTTP State
    conn_state_t state;
    sockaddr_in serv_addr;
    string host;
    string path;

    // Request data
    string send_buffer;
    size_t bytes_sent = 0;
    
    // Response data
    string recv_headers;
    string recv_body;


    transfer_mode_t transfer_mode = CONNECTION_CLOSE;
    size_t content_length = -1;
    size_t chunk_size = -1;


    // Callback for completion
    function<void(const string& response, bool success)> callback;
    
    HTTPSRequest(const string& h, const string& p) : host(h), path(p) {
        SSL_load_error_strings();
        SSL_library_init();
        ssl_ctx = SSL_CTX_new(TLS_client_method());
        conn = nullptr;
    }
    ~HTTPSRequest() {
        if (conn) {
            SSL_shutdown(conn);
            SSL_free(conn);
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
    void parse_response(HTTPSRequest* req, char buffer[], ssize_t bytes_recieved, string sentinel="\r\n");
    void handle_read_response_headers(HTTPSRequest* req, uint32_t filter, string sentinel = "\r\n\r\n");
    void handle_read_response(HTTPSRequest* req, uint32_t filter, string sentinel = "\r\n\r\n");
    void cleanup(HTTPSRequest* req);
public:
    AsyncHTTPSConnection();
    void post_async(const string& host, const string& path, const string& body, const vector<pair<string, string>>& headers, function<void(const string&, bool)> callback);
    void run_loop();
    ~AsyncHTTPSConnection();
};

#endif 
