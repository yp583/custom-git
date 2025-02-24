#include <iostream> 
#include <cstring> 
#include <unistd.h> 
#include <arpa/inet.h> 
#include <netdb.h> 

using namespace std;

class APIConnection {
  private:
    int fd;
    int get_fd(const char* host, const char* path) {
      
      int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
      if (sockfd < 0) { 
        perror("Socket creation failed"); 
        return -1; 
      }

      struct hostent* server = gethostbyname(host); 
      if (server == nullptr) { 
          cerr << "No such host: " << host << endl; 
          close(sockfd); 
          return -1;
      }

      struct sockaddr_in serv_addr;
      memset(&serv_addr, 0, sizeof(serv_addr)); 
      serv_addr.sin_family = AF_INET;
      memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length); 
      serv_addr.sin_port = htons(80);
  
      if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) { 
          perror("Connection failed"); 
          close(sockfd); 
          return -1; 
      } 

      return sockfd;
    }
  public:
      APIConnection(string url, string path) {
        this->fd = get_fd(url.c_str(), path.c_str());
      }
      ~APIConnection() {
        close(fd);
      }
        
};
 
void http_get(const char* host, const char* path) { 
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (sockfd < 0) { 
        perror("Socket creation failed"); 
        return; 
    } 
 
    struct hostent* server = gethostbyname(host); 
    if (server == nullptr) { 
        cerr << "No such host: " << host << endl; 
        close(sockfd); 
        return;
    }
 
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr)); 
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length); 
    serv_addr.sin_port = htons(80);
 
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) { 
        perror("Connection failed"); 
        close(sockfd); 
        return; 
    } 
 
    // Sending HTTP GET request 
    string request = "GET " + string(path) + " HTTP/1.1\r\n"; 
    request += "Host: " + string(host) + "\r\n"; 
    request += "Connection: close\r\n\r\n"; 
 
    send(sockfd, request.c_str(), request.size(), 0); 
 
    // Receiving response 
    char buffer[4096]; 
    while (true) { 
        memset(buffer, 0, sizeof(buffer)); 
        ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0); 
        if (bytes_received <= 0) { 
            break; // Connection closed or error 
        } 
        cout << buffer; // Print response 
    } 
 
    close(sockfd); 
} 
 
int main() { 
    const char* url = "www.google.com"; // Replace with your URL 
    const char* path = "/"; // Replace with the specific path if needed 
    http_get(url, path); 
    return 0; 
} 