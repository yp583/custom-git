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

void AsyncHTTPSConnection::post_async(const string& host, const string& path, const string& body, const vector<pair<string, string>>& headers, promise<HTTPSResponse> resp) {
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
        resp.set_exception(make_exception_ptr(runtime_error("Connection failed")));
        return;
    }

    req->socket_fd = socket_fd;
    req->state = CONNECTING;
    req->resp = std::move(resp);

    string request = "POST " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "Content-Length: " + to_string(body.size()) + "\r\n";
    for (const auto& header : headers) {
        request += header.first + ": " + header.second + "\r\n";
    }
    request += "\r\n" + body;
    req->send_buffer = request;

    struct kevent ev;
    EV_SET(&ev, socket_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, req.get());
    kevent(kqueue_fd, &ev, 1, nullptr, 0, nullptr);

    reqs[socket_fd] = std::move(req);
}

void AsyncHTTPSConnection::run_loop(){
    struct kevent events[64];

    while (!reqs.empty()) {
        int n = kevent(kqueue_fd, nullptr, 0, events, 64, nullptr);
        if (n == -1) {
            perror("kevent");
            break;
        }

        for (int i = 0; i < n; ++i) {
            auto *req = static_cast<HTTPSRequest*>(events[i].udata);
            uint32_t filter = events[i].filter;

            cerr << "Event: state=" << req->state << " filter=" << filter << endl;

            conn_state_t state_before = req->state;

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
                    cerr << "Cleaning up DONE request" << endl;
                    this->cleanup(req);
                    continue;
                case ERROR:
                    cerr << "ERROR occurred in state machine" << endl;
                    this->cleanup(req);
                    continue;
                default:
                    break;
            }

            if (state_before != DONE && state_before != ERROR) {
                if (req->state == DONE) {
                    cerr << "State transitioned to DONE, cleaning up" << endl;
                    this->cleanup(req);
                } else if (req->state == ERROR) {
                    cerr << "State transitioned to ERROR, cleaning up" << endl;
                    this->cleanup(req);
                }
            }
        }
    }
}

void AsyncHTTPSConnection::handle_connect(HTTPSRequest* req, int16_t filter) {
    switch (filter) {
        case EVFILT_WRITE:
            {
                int error;
                socklen_t len = sizeof(error);
                getsockopt(req->socket_fd, SOL_SOCKET, SO_ERROR, &error, &len);

                if (error == 0) {
                    req->conn = SSL_new(req->ssl_ctx);
                    SSL_set_fd(req->conn, req->socket_fd);
                    SSL_set_tlsext_host_name(req->conn, req->host.c_str());
                    req->state = TLS_HANDSHAKE;
                } else {
                    cerr << "Socket connection failed with error: " << error << endl;
                    req->state = ERROR;
                }
            }
            break;
        default:
            {
                cerr << "handle_connect: unexpected filter" << endl;
                req->state = ERROR;
            }
            break;
    }
}
void AsyncHTTPSConnection::handle_tls(HTTPSRequest* req, int16_t filter) {
    switch (filter) {
        case EVFILT_WRITE:
        {
            int ssl_result = SSL_connect(req->conn);
            cerr << "SSL_connect result=" << ssl_result << endl;
            if (ssl_result == 1) {
                cerr << "TLS handshake complete from WRITE event!" << endl;
                req->state = WRITING_REQUEST;
            } else {
                int ssl_error = SSL_get_error(req->conn, ssl_result);
                cerr << "SSL_get_error=" << ssl_error << " (WANT_READ=2, WANT_WRITE=3)" << endl;
                if (ssl_error == SSL_ERROR_WANT_READ) {
                    struct kevent events[2];
                    EV_SET(&events[0], req->socket_fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
                    EV_SET(&events[1], req->socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, req);
                    kevent(kqueue_fd, events, 2, nullptr, 0, nullptr);
                } else if (ssl_error != SSL_ERROR_WANT_WRITE) {
                    cerr << "handle_tls WRITE: SSL error " << ssl_error << endl;
                    req->state = ERROR;
                }
            }
            break;
        }
        case EVFILT_READ:
        {
            int ssl_result = SSL_connect(req->conn);
            cerr << "SSL_connect (from READ) result=" << ssl_result << endl;
            if (ssl_result == 1) {
                cerr << "TLS handshake complete from READ event!" << endl;
                req->state = WRITING_REQUEST;
                struct kevent events[2];
                EV_SET(&events[0], req->socket_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
                EV_SET(&events[1], req->socket_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, req);
                kevent(kqueue_fd, events, 2, nullptr, 0, nullptr);
            } else {
                int ssl_error = SSL_get_error(req->conn, ssl_result);
                if (ssl_error == SSL_ERROR_WANT_WRITE) {
                    struct kevent events[2];
                    EV_SET(&events[0], req->socket_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
                    EV_SET(&events[1], req->socket_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, req);
                    kevent(kqueue_fd, events, 2, nullptr, 0, nullptr);
                } else if (ssl_error != SSL_ERROR_WANT_READ) {
                    cerr << "handle_tls READ: SSL error " << ssl_error << endl;
                    req->state = ERROR;
                }
            }
            break;
        }
        default:
            req->state = ERROR;
            cerr << "handle_tls: unexpected filter" << endl;
            break;
    }
}

void AsyncHTTPSConnection::handle_write(HTTPSRequest* req, int16_t filter) {
    switch (filter) {
        case EVFILT_WRITE:
            {
                cerr << "handle_write: starting" << endl;
                const char* data = req->send_buffer.c_str() + req->bytes_sent;
                size_t remaining = req->send_buffer.size() - req->bytes_sent;

                int bytes_written = SSL_write(req->conn, data, remaining);
                cerr << "SSL_write result=" << bytes_written << endl;

                if (bytes_written > 0) {
                    req->bytes_sent += bytes_written;
                    cerr << "Wrote " << bytes_written << " bytes, total=" << req->bytes_sent << "/" << req->send_buffer.size() << endl;
                    if (req->bytes_sent >= req->send_buffer.size()) {
                        cerr << "Request fully sent, transitioning to READING_RESPONSE_HEADERS" << endl;
                        req->state = READING_RESPONSE_HEADERS;
                        struct kevent events[2];
                        EV_SET(&events[0], req->socket_fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
                        EV_SET(&events[1], req->socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, req);
                        int ret = kevent(kqueue_fd, events, 2, nullptr, 0, nullptr);
                        cerr << "kevent returned " << ret << endl;
                    }
                } else {
                    int ssl_error = SSL_get_error(req->conn, bytes_written);
                    cerr << "SSL_write failed, ssl_error=" << ssl_error << endl;
                    if (ssl_error == SSL_ERROR_WANT_READ) {
                        struct kevent ev;
                        EV_SET(&ev, req->socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, req);
                        kevent(kqueue_fd, &ev, 1, nullptr, 0, nullptr);
                    } else if (ssl_error != SSL_ERROR_WANT_WRITE) {
                        req->state = ERROR;
                    }
                }
                cerr << "handle_write: ending, state=" << req->state << endl;
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
                if (req->recv_body.size() >= req->content_length) {
                    req->state = DONE;
                }
            }
            break;
        case CHUNKED:
            {
                req->chunked_buffer.append(buffer, bytes_received);

                size_t pos = 0;
                while (pos < req->chunked_buffer.size()) {
                    if (req->chunk_size == 0) {
                        size_t crlf_pos = req->chunked_buffer.find("\r\n", pos);
                        if (crlf_pos == string::npos) {
                            req->chunked_buffer = req->chunked_buffer.substr(pos);
                            return;
                        }

                        string chunk_size_str = req->chunked_buffer.substr(pos, crlf_pos - pos);
                        if (chunk_size_str.empty()) {
                            pos = crlf_pos + 2;
                            continue;
                        }

                        try {
                            req->chunk_size = stoi(chunk_size_str, nullptr, 16);
                        } catch (const exception& e) {
                            pos = crlf_pos + 2;
                            continue;
                        }
                        pos = crlf_pos + 2;

                        if (req->chunk_size == 0) {
                            req->state = DONE;
                            req->chunked_buffer.clear();
                            return;
                        }
                    } else {
                        size_t available = req->chunked_buffer.size() - pos;
                        size_t to_read = min(req->chunk_size, available);

                        req->recv_body.append(req->chunked_buffer, pos, to_read);
                        pos += to_read;
                        req->chunk_size -= to_read;

                        if (req->chunk_size == 0) {
                            if (pos + 2 <= req->chunked_buffer.size()) {
                                pos += 2;
                            } else {
                                req->chunked_buffer = req->chunked_buffer.substr(pos);
                                return;
                            }
                        } else {
                            req->chunked_buffer = req->chunked_buffer.substr(pos);
                            return;
                        }
                    }
                }
                req->chunked_buffer.clear();
            }
            break;
        case CONNECTION_CLOSE:
            {
                string buffer_str(buffer, bytes_received);
                req->recv_body.append(buffer_str);
            }
            break;
    }
}
void AsyncHTTPSConnection::handle_read_response_headers(HTTPSRequest* req, int16_t filter) {
    switch (filter) {
        case EVFILT_READ:
            {
                char buffer[4096];
                ssize_t bytes_received = SSL_read(req->conn, &buffer, sizeof(buffer));
                cerr << "SSL_read (headers) bytes=" << bytes_received << endl;
                if (bytes_received > 0) {
                    req->recv_headers.append(buffer, bytes_received);
                    cerr << "Headers so far (" << req->recv_headers.size() << " bytes)" << endl;
                
                    size_t pos = req->recv_headers.find("\r\n\r\n");
                    if (pos != string::npos) {
                        size_t header_end = pos + 4;
                        string headers_only = req->recv_headers.substr(0, header_end);
                        transform(headers_only.begin(), headers_only.end(), headers_only.begin(), ::tolower);
                        string spillover_body = req->recv_headers.substr(header_end);

                        req->recv_headers = headers_only;
                        // Don't set recv_body here - parse_response will append it

                        cerr << "=== HEADERS ===\n" << headers_only << "=== END HEADERS ===" << endl;

                        size_t te_pos = headers_only.find("transfer-encoding:");
                        if (te_pos != string::npos) {
                            size_t line_end = headers_only.find("\r\n", te_pos);
                            string te_line = headers_only.substr(te_pos, line_end - te_pos);
                            cerr << "Found: " << te_line << endl;
                            if (te_line.find("chunked") != string::npos) {
                                req->transfer_mode = CHUNKED;
                                cerr << "Using CHUNKED transfer mode" << endl;
                            }
                        }

                        if (req->transfer_mode != CHUNKED) {
                            size_t cl_pos = headers_only.find("content-length:");
                            if (cl_pos != string::npos) {
                                size_t value_start = cl_pos + 15;
                                size_t line_end = headers_only.find("\r\n", cl_pos);
                                string cl_value = headers_only.substr(value_start, line_end - value_start);

                                cl_value.erase(0, cl_value.find_first_not_of(" \t"));
                                req->content_length = stoi(cl_value);
                                req->transfer_mode = CONTENT_LENGTH;  // Actually set the mode!
                                cerr << "Using CONTENT_LENGTH mode, length=" << req->content_length << endl;

                                if (req->content_length == 0) {
                                    req->state = DONE;
                                    return;
                                }
                            }
                        }
                        req->state = READING_RESPONSE;
                        
                        if (!spillover_body.empty()) {
                            parse_response(req, const_cast<char*>(spillover_body.c_str()), spillover_body.size());
                        }
                    }
                } else {
                    int ssl_error = SSL_get_error(req->conn, bytes_received);
                    cerr << "SSL_read failed, ssl_error=" << ssl_error << endl;
                    if (ssl_error != SSL_ERROR_WANT_READ) {
                        cerr << "Setting ERROR state from handle_read_response_headers" << endl;
                        req->state = ERROR;
                    }
                }

            }
            break;
        default:
            {
                cerr << "handle_read_response_headers: unexpected filter" << endl;
                req->state = ERROR;
            }
            break;
    }

}

void AsyncHTTPSConnection::handle_read_response(HTTPSRequest* req, int16_t filter) {
    switch (filter) {
        case EVFILT_READ:
            {
                char buffer[4096];
                ssize_t bytes_received = SSL_read(req->conn, &buffer, sizeof(buffer));
                cerr << "SSL_read (body) bytes=" << bytes_received << " transfer_mode=" << req->transfer_mode << endl;
                if (bytes_received > 0) {
                    parse_response(req, buffer, bytes_received);
                    cerr << "After parse_response, state=" << req->state << endl;
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

void AsyncHTTPSConnection::cleanup(HTTPSRequest* req) {
    if (req->state == DONE){
        HTTPSResponse resp{req->recv_headers, req->recv_body};
        req->resp.set_value(resp);
    } else
    {
        req->resp.set_exception(make_exception_ptr(runtime_error("Error with https request")));
    }
    

    struct kevent ev;
    EV_SET(&ev, req->socket_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    kevent(kqueue_fd, &ev, 1, nullptr, 0, nullptr);
    EV_SET(&ev, req->socket_fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    kevent(kqueue_fd, &ev, 1, nullptr, 0, nullptr);
    
    reqs.erase(req->socket_fd);
}