#include "async_https_api.hpp"
#include <memory>
#include <sys/event.h>
#include <fcntl.h>

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

    req->socket_fd = socket_fd;
    req->state = CONNECTING;

    // Register with kqueue
    struct kevent ev;
    EV_SET(&ev, socket_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, req.get());
    kevent(kqueue_fd, &ev, 1, nullptr, 0, nullptr);

    // Store it by FD for lookup during events
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
                case READING_RESPONSE:
                    handle_read(req, filter);
                    break;
                case DONE:
                    cleanup(req);
                    break;
                default:
                    break;
            }
        }
    }
}



// Handler for a socket that just connected (CONNECTING state)
void AsyncHTTPSConnection::handle_connect(HTTPSRequest* req, uint32_t filter) {
    // Implementation goes here
}

// Handler for a socket undergoing TLS handshake (TLS_HANDSHAKE state)
void AsyncHTTPSConnection::handle_tls(HTTPSRequest* req, uint32_t filter) {
    // Implementation goes here
}

// Handler for writing out the HTTP request (WRITING_REQUEST state)
void AsyncHTTPSConnection::handle_write(HTTPSRequest* req, uint32_t filter) {
    // Implementation goes here
}

// Handler for reading the HTTP response (READING_RESPONSE state)
void AsyncHTTPSConnection::handle_read(HTTPSRequest* req, uint32_t filter) {
    // Implementation goes here
}

// Cleans up resources and calls callback, when DONE (DONE state)
void AsyncHTTPSConnection::cleanup(HTTPSRequest* req) {
    // Implementation goes here
}

