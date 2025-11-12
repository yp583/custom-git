#include "async_https_api.hpp"
#include <memory>
#include <sys/_types/_ssize_t.h>
#include <sys/event.h>
#include <fcntl.h>
#include <iostream>

using namespace std;

AsyncHTTPSConnection::AsyncHTTPSConnection(){
    this->kqueue_fd = kqueue();
    if (kqueue_fd == -1) {
        perror("kqueue");
        throw runtime_error("Failed to create kqueue");
    }
}
AsyncHTTPSConnection::~AsyncHTTPSConnection() {
    close(this->kqueue_fd);
}

void AsyncHTTPSConnection::post_async(const string& host, const string& path, const string& body, const vector<pair<string, string>>& headers, function<void(const string&, bool)> callback) {
   auto req = make_unique<HTTPSRequest>(host, path);
   int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
   
    fcntl(socket_fd, F_SETFL, O_NONBLOCK);

    struct hostent* server = gethostbyname(host.c_str());
    if (server == nullptr) {
        cerr << "No such host: " << host << endl;
        close(socket_fd);
        return;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(443);

    int result = connect(socket_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (result == -1 && errno != EINPROGRESS) {
        close(socket_fd);
        callback("", false);
        return;
    }

    req->socket_fd = socket_fd;
    req->state = CONNECTING;

    struct kevent ev;
    EV_SET(&ev, socket_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, req.get());
    kevent(kqueue_fd, &ev, 1, nullptr, 0, nullptr);

    reqs[socket_fd] = std::move(req);
}

void AsyncHTTPSConnection::run_loop(){
    struct kevent events[64];

    while (true) {
        int n = kevent(kqueue_fd, nullptr, 0, events, 64, nullptr);
        if (n == -1) {
            perror("kevent");
            break;
        }

        for (int i = 0; i < n; ++i) {
            auto *req = static_cast<HTTPSRequest*>(events[i].udata);
            uint32_t filter = events[i].filter;

            switch (req->state) {
                case CONNECTING:
                    handle_connect(req, filter);
                    break;
                case TLS_HANDSHAKE:
                    handle_tls(req, filter);
                    break;
                case WRITING_REQUEST:
                    handle_write(req, filter);
                    break;
                case READING_RESPONSE_HEADERS:
                    handle_read_response_headers(req, filter);
                    break;
                case READING_RESPONSE:
                    handle_read_response(req, filter);
                    break;
                case DONE:
                    cleanup(req);
                    break;
                case ERROR:
                    cerr << "ERROR occurred" << endl;
                    break;
                default:
                    break;
            }
        }
    }
}

void AsyncHTTPSConnection::handle_connect(HTTPSRequest* req, uint32_t filter) {
    switch (filter) {
        case EVFILT_WRITE: 
            {
                int error;
                socklen_t len = sizeof(error);
                getsockopt(req->socket_fd, SOL_SOCKET, SO_ERROR, &error, &len);
                
                if (error == 0) {
                    req->state = TLS_HANDSHAKE;
                } else {
                    req->state = ERROR;
                }
            }
            break;
        default: 
            {
                req->state = ERROR;
            }
            break;
    }
}
void AsyncHTTPSConnection::handle_tls(HTTPSRequest* req, uint32_t filter) {
    switch (filter) {
        case EVFILT_WRITE:
        {
            int ssl_result = SSL_connect(req->conn);
            if (ssl_result == 1) {
                req->state = WRITING_REQUEST;
                struct kevent ev;
                EV_SET(&ev, req->socket_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, req);
                kevent(kqueue_fd, &ev, 1, nullptr, 0, nullptr);
            } else {
                int ssl_error = SSL_get_error(req->conn, ssl_result);
                if (ssl_error == SSL_ERROR_WANT_READ) {
                    struct kevent ev;
                    EV_SET(&ev, req->socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, req);
                    kevent(kqueue_fd, &ev, 1, nullptr, 0, nullptr);
                } else if (ssl_error != SSL_ERROR_WANT_WRITE) {
                    req->state = ERROR;
                }
            }
            break;
        }
        case EVFILT_READ:
        {
            int ssl_result = SSL_connect(req->conn);
            if (ssl_result == 1) {
                req->state = WRITING_REQUEST;
            } else {
                int ssl_error = SSL_get_error(req->conn, ssl_result);
                if (ssl_error == SSL_ERROR_WANT_WRITE) {
                    struct kevent ev;
                    EV_SET(&ev, req->socket_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, req);
                    kevent(kqueue_fd, &ev, 1, nullptr, 0, nullptr);
                } else if (ssl_error != SSL_ERROR_WANT_READ) {
                    req->state = ERROR;
                }
            }
            break;
        }
        default:
            req->state = ERROR;
            break;
    }
}

void AsyncHTTPSConnection::handle_write(HTTPSRequest* req, uint32_t filter) {
    switch (filter) {
        case EVFILT_WRITE: 
            {
                const char* data = req->send_buffer.c_str() + req->bytes_sent;
                size_t remaining = req->send_buffer.size() - req->bytes_sent;
                
                int bytes_written = SSL_write(req->conn, data, remaining);
                if (bytes_written <= 0) {
                    cerr << "Error writing to socket. Code: " << req->bytes_sent << endl;
                    return;
                }
                if (bytes_written > 0) {
                    req->bytes_sent += bytes_written;
                    if (req->bytes_sent >= req->send_buffer.size()) {
                        req->state = READING_RESPONSE_HEADERS;
                        struct kevent ev;
                        EV_SET(&ev, req->socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, req);
                        kevent(kqueue_fd, &ev, 1, nullptr, 0, nullptr);
                    }
                } else {
                    int ssl_error = SSL_get_error(req->conn, bytes_written);
                    if (ssl_error == SSL_ERROR_WANT_READ) {
                        struct kevent ev;
                        EV_SET(&ev, req->socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, req);
                        kevent(kqueue_fd, &ev, 1, nullptr, 0, nullptr);
                    } else if (ssl_error != SSL_ERROR_WANT_WRITE) {
                        req->state = ERROR;
                    }
                }
                break;
            }
            break;
        default:
            {
                req->state = ERROR;
            }
            break;
    }
}
void AsyncHTTPSConnection::parse_response(HTTPSRequest* req, char buffer[], ssize_t bytes_received, string sentinel) {
    switch (req->transfer_mode) {
        case CONTENT_LENGTH:
            {
                req->recv_body.append(buffer, bytes_received);
                if (req->content_length >= req->recv_body.size()) {
                    req->state = DONE;
                }
            }
            break;
        case CHUNKED:
            {
                string buffer_str(buffer, bytes_received);
                size_t sentinel_pos = buffer_str.find(sentinel, 0);
                size_t prev_sentinel_pos = 0;
                while (sentinel_pos != std::string::npos) {
                    req->chunk_size = stoi(buffer_str.substr(prev_sentinel_pos, sentinel_pos - prev_sentinel_pos), nullptr, 16);
                    prev_sentinel_pos = sentinel_pos + sentinel.length();
                    sentinel_pos = buffer_str.find(sentinel, prev_sentinel_pos);
                }
            }
            break;
        case CONNECTION_CLOSE:
            {

            }
}
void AsyncHTTPSConnection::handle_read_response_headers(HTTPSRequest* req, uint32_t filter, string sentinel) {
    switch (filter) {
        case EVFILT_READ: 
            {
                char buffer[4096]; 
                ssize_t bytes_received = SSL_read(req->conn, &buffer, sizeof(buffer)); 
                if (bytes_received > 0) { 
                    req->recv_headers.append(buffer, bytes_received);
                
                    size_t pos = req->recv_headers.find(sentinel);
                    if (pos != string::npos) {
                        // Found end of headers - parse and transition
                        size_t header_end = pos + 4; // Include \r\n\r\n
                        string headers_only = req->recv_headers.substr(0, header_end);
                        transform(headers_only.begin(), headers_only.end(), headers_only.begin(), ::tolower);
                        string spillover_body = req->recv_headers.substr(header_end);

                        req->recv_headers = headers_only;
                        req->recv_body = spillover_body;

                        size_t te_pos = headers_only.find("Transfer-Encoding:");
                        if (te_pos != string::npos) {
                            size_t line_end = headers_only.find("\r\n", te_pos);
                            string te_line = headers_only.substr(te_pos, line_end - te_pos);
                            if (te_line.find("chunked") != string::npos) {
                                req->transfer_mode = CHUNKED;
                            }
                        }

                        if (req->transfer_mode != CHUNKED) {
                            size_t cl_pos = headers_only.find("Content-Length:");
                            if (cl_pos != string::npos) {
                                size_t value_start = cl_pos + 15; // Skip "Content-Length:"
                                size_t line_end = headers_only.find("\r\n", cl_pos);
                                string cl_value = headers_only.substr(value_start, line_end - value_start);
                                
                                // Trim whitespace and parse
                                cl_value.erase(0, cl_value.find_first_not_of(" \t"));
                                req->content_length = stoi(cl_value);
                            }
                        }
                        req->state = READING_RESPONSE;
                        this->handle_spillover_body(req)
                    }
                } else {
                    int ssl_error = SSL_get_error(req->conn, bytes_received);
                    if (ssl_error != SSL_ERROR_WANT_READ) {
                        req->state = ERROR;
                    }
                }
                
            }
            break;
        default:
            {
                req->state = ERROR;
            }
            break;
    }
    
}
void AsyncHTTPSConnection::handle_read_response(HTTPSRequest* req, uint32_t filter, string sentinel) {
    switch (filter) {
        case EVFILT_READ:
            {
                char buffer[4096]; 
                ssize_t bytes_received = SSL_read(req->conn, &buffer, sizeof(buffer)); 
                if (bytes_received > 0) { 
                    switch (req->transfer_mode) {
                        case CONTENT_LENGTH:
                            {
                                req->recv_body.append(buffer, bytes_received);
                                if (req->content_length >= req->recv_body.size()) {
                                    req->state = DONE;
                                }
                            }
                            break;
                        case CHUNKED:
                            {

                            }
                            break;
                        case CONNECTION_CLOSE:
                            {

                            }
                    } 
                } else {
                    int ssl_error = SSL_get_error(req->conn, bytes_received);
                    if (ssl_error != SSL_ERROR_WANT_READ) {
                        req->state = ERROR;
                    }
                }

            }
        default:
            {
                req->state = ERROR;
            }
            break;
    }
}

void AsyncHTTPSConnection::cleanup(HTTPSRequest* req) {
}
