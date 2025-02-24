#include <iostream> 
#include <cstring> 
#include <unistd.h> 
#include <arpa/inet.h> 
#include <netdb.h> 
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <vector>
#include <fstream>

#include "https_api.h"

using namespace std;

void APIConnection::start_conn() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd < 0) { 
        perror("Socket creation failed"); 
        return; 
    }

    struct hostent* server = gethostbyname(this->host.c_str()); 
    if (server == nullptr) { 
        cerr << "No such host: " << this->host << endl; 
        close(sockfd); 
        return;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr)); 
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length); 
    serv_addr.sin_port = htons(443);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) { 
        perror("Connection failed"); 
        close(sockfd); 
        return; 
    } 

    //For HTTPS as it needs encryption
    SSL_load_error_strings();
    SSL_library_init();
    this->ssl_ctx = SSL_CTX_new(SSLv23_client_method());

    this->conn = SSL_new(this->ssl_ctx);
    SSL_set_fd(this->conn, sockfd);
    
    SSL_set_tlsext_host_name(this->conn, this->host.c_str());
    
    this->fd = sockfd;

    int err = SSL_connect(this->conn);
    if (err != 1) {
        unsigned long ssl_err = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(ssl_err, err_buf, sizeof(err_buf));
        cerr << "SSL connection failed: " << err_buf << endl;
        close(sockfd);
        return;
    }
}

APIConnection::APIConnection(string url, string path) {
    this->host = url;
    this->path = path;
    start_conn();
}

string APIConnection::post(string body, vector<pair<string, string>> headers) {
    string request = "POST " + this->path + " HTTP/1.1\r\n"; 
    request += "Host: " + this->host + "\r\n"; 
    request += "Content-Length: " + to_string(body.size()) + "\r\n"; 

    for (pair<string, string> header : headers) {
        request += header.first + ": " + header.second + "\r\n";
    }

    request += "\r\n" + body; 

    SSL_write(this->conn, request.c_str(), request.size());

    string response;
    char buffer[4096]; 
    int total_bytes = 0;
    while (true) { 
        memset(buffer, 0, sizeof(buffer)); 
        ssize_t bytes_received = SSL_read(this->conn, buffer, sizeof(buffer) - 1); 
        if (bytes_received <= 0) { 
            break;
        }
        response += buffer;
        total_bytes += bytes_received;
        
        if (response.find("\r\n\r\n") != string::npos) {
            size_t content_length_pos = response.find("Content-Length: ");
            if (content_length_pos != string::npos) {
                size_t content_length_end = response.find("\r\n", content_length_pos);
                string content_length_str = response.substr(
                    content_length_pos + 16, 
                    content_length_end - (content_length_pos + 16)
                );
                size_t content_length = stoul(content_length_str);
                size_t headers_end = response.find("\r\n\r\n") + 4;
                if (response.length() - headers_end >= content_length) {
                    break;
                }
            }
        }
    }
    return response;
}

APIConnection::~APIConnection() {
    SSL_shutdown(conn);
    SSL_free(conn);
    SSL_CTX_free(ssl_ctx);
    close(fd);
}

// int main() { 
//     const char* url = "api.openai.com"; // Replace with your URL 
//     const char* path = "/v1/embeddings"; // Replace with the specific path if needed 
//     ifstream env_file("./.env");
//     string line;
//     string api_key;
//     while (getline(env_file, line)) {
//       if (line.substr(0, 14) == "OPENAI_API_KEY") {
//           api_key = line.substr(15); //difference from 15 to 14 is because of = sign
//           break;
//       }
//     }
//     if (api_key.empty()) {
//         cerr << "Error: OPENAI_API_KEY not found in .env file" << endl;
//         return 1;
//     }
//     APIConnection openai_api(url, path); 
//     string text = "write a haiku about ai";
//     string body = "{\"model\": \"text-embedding-3-small\",\"input\": \"" + text + "\"}";
//     vector<pair<string, string>> headers = {{"Authorization", "Bearer " + api_key}, {"Content-Type", "application/json"}};
    
//     string response = openai_api.post(body, headers); 
//     cout << response << endl;
//     return 0; 
// } 