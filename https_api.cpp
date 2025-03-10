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

void APIConnection::send(string request) {
    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    int total_bytes = 0;
    while(total_bytes < request.size()){
      memcpy(buffer, request.c_str() + total_bytes, min(sizeof(buffer), request.size() - total_bytes));
      int bytes_written = SSL_write(this->conn, buffer, min(sizeof(buffer), request.size() - total_bytes));
      if (bytes_written <= 0) {
        cerr << "Error writing to socket. Code: " << bytes_written << endl;
        return;
      }
      total_bytes += bytes_written;
    }
}

string APIConnection::recieve_length(int n) {
  string response;
  char buffer[4096];
  int total_bytes = 0;
  while (total_bytes < n) {
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_received = SSL_read(this->conn, buffer, sizeof(buffer) - 1);
    total_bytes += bytes_received;
    response += buffer;
  }
  return response;
}
string APIConnection::recieve_sentinel(string sentinel) {
  string response;
  char buffer; 
  bool found_sentinel = false;

  while (!found_sentinel) { 
      ssize_t bytes_received = SSL_read(this->conn, &buffer, 1); 
      if (bytes_received <= 0) { 
          break;
      }
      response += buffer;
      
      if (response.size() >= sentinel.size() && response.substr(response.size() - sentinel.size()) == sentinel) {
        found_sentinel = true;
      }
  }
  return response;
}

string APIConnection::post(string body, vector<pair<string, string>> headers) {
    string request = "POST " + this->path + " HTTP/1.1\r\n"; 
    request += "Host: " + this->host + "\r\n"; 
    request += "Content-Length: " + to_string(body.size()) + "\r\n"; 

    for (pair<string, string> header : headers) {
        request += header.first + ": " + header.second + "\r\n";
    }

    request += "\r\n" + body; 

    send(request);
    string response = recieve_sentinel("\r\n\r\n");
    int length_start = response.find("Content-Length: ") + 16;
    int length_end = response.find("\r\n", length_start);

    int length = stoi(response.substr(length_start, length_end - length_start));
    return recieve_length(length);
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