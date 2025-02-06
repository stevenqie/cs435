#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string>
#include <fstream>

#include <arpa/inet.h>

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int connect_to_server(const char* host, const char* port) {
    int sockfd; 
    struct addrinfo hints, *res; 
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM; 

    int status = getaddrinfo(host, port, &hints, &res);
    if (status) {
        //std::cout << "getaddrinfo failed" << std::endl;
        return -1; 
    }

    struct addrinfo* counter; 
    for (counter = res; counter != NULL; counter = counter->ai_next) {
        sockfd = socket(counter->ai_family, counter->ai_socktype, counter->ai_protocol);
        if (sockfd == -1) {
            perror("client: socket");
			continue;
        }

        if (connect(sockfd, counter->ai_addr, counter->ai_addrlen) == -1) {
            close(sockfd);
			perror("client: connect");
			continue;
        }

        break; 
    }
    if (counter == NULL) {
        //std::cout << "Failed to connect to server" << std::endl;
        return -1; 
    }

    // char s[INET6_ADDRSTRLEN];
    // inet_ntop(counter->ai_family, get_in_addr((struct sockaddr *)counter->ai_addr),
	// 		s, sizeof s);
	//printf("client: connecting to %s\n", s);

    freeaddrinfo(res);
    return sockfd;
}

void generate_and_send_request(int sockfd, std::string url, std::string file) {
    std::string request = "GET " + file + " HTTP/1.0\r\n";
    request += "User-Agent: Steven's Client\r\n";
    request += "Accept: */*\r\n";
    request += "Host:" + url + "\r\n";
    request += "Connection: Keep-Alive\r\n\r\n";

    ssize_t bytes_wrote = send(sockfd, request.c_str(), request.length(), 0);
    if (bytes_wrote == -1) {
        perror("send");
        close(sockfd);
        return; 
    }

    //std::cout << "Sent the request" << std::endl; 
    //close(sockfd);
}

bool parseHTTPResponse(int sockfd, std::ofstream &output) {
    char buffer[2048];
    bool headerParsed = false; 

    std::string response = "";
    while (true) {
        ssize_t bytes_read = recv(sockfd, buffer, 2048, 0);
        if (bytes_read == 0) {
            //this means the connection is closed or everything has beens ent 
            break; 
        } else if (bytes_read > 0) {
            if (!headerParsed) {
                buffer[bytes_read] = '\0';
                response += buffer; 
                size_t headerEnd = response.find("\r\n\r\n");
                if (headerEnd != std::string::npos) {
                    headerParsed = true; 
                    std::string header = response.substr(0, headerEnd);
                    std::string messagebody_start = response.substr(headerEnd + 4);

                    //check the different common repsonse headers 
                    if (header.find("200 OK") != std::string::npos) {
                        std::cout << "recevied a 200ok message" << std::endl; 
                    } else if (header.find("404 File not found") != std::string::npos) {
                        std::cout << "received a 404 message" << std::endl; 
                        output << "FILENOTFOUND" << std::endl; 
                        output.close();
                        close(sockfd);
                        return false; 
                    } else if (header.find("301 Moved Permanently") != std::string::npos) {
                        std::cout << "received a 301 message" << std::endl; 
                        return false; //currently not supported 
                    } else {
                        std::cout << "unknown response message: " << header << std::endl;
                        return false; 
                    }

                    //this means header was ok and everything is normal 
                    //std::string messagebody_start = header.substr(headerEnd + 4);
                    output.write(messagebody_start.c_str(), messagebody_start.length());
                }
            } else {
                //message body portion 
                output.write(buffer, bytes_read);
            }
        } else if (bytes_read == -1 && errno == EINTR) {
            continue; 
        } else {
            perror("recv");
            exit(1);
        }
    }

    output.close();
    close(sockfd);
    return true; 
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << "Usage: ./http_client http://hostname[:port]/path_to_file" << std::endl;
        return 1; 
    }

    //input checks 

    //creates output file called output
    std::ofstream output("output");
    if (!output.is_open()) {
        //std::cout << "could not open file" << std::endl; 
        return 1;
    }

    //checks if protocol is http 
    std::string http_url = argv[1];
    if (http_url.length() < 7 || http_url.substr(0, 7).compare("http://") != 0) {
        output << "INVALIDPROTOCOL" << std::endl; 
        output.close();
        return 1; 
    }

    std::string secondhalf = http_url.substr(7); 
    size_t slashpos = secondhalf.find("/");
    if (slashpos == std::string::npos || slashpos == secondhalf.length() - 1) {
        output << "FILENOTFOUND" << std::endl; 
        output.close();
        return 1; 
    }
    std::string serverurl = secondhalf.substr(0, slashpos);
    std::string serverfile = secondhalf.substr(slashpos + 1);
    // grab the port if its there 
    // "80" is the default port 
    std::string port = "80";
    if (serverurl.find(":") != std::string::npos) {
        port = serverurl.substr(serverurl.find(":") + 1);
        serverurl = serverurl.substr(0, serverurl.find(":"));
    }

    // to commment out later on 
    printf("serverurl: %s\n", serverurl.c_str());
    printf("port: %s\n", port.c_str());

    //connecting to the server 
    int sock_fd = connect_to_server(serverurl.c_str(), port.c_str());
    if (sock_fd == -1) {
        // failed to conenct to server
        // in this case, the program should write NOCONNECTION to the output file
        output << "NOCONNECTION" << std::endl; 
        output.close();
        return 1; 
    }

    //send the actual request 
    generate_and_send_request(sock_fd, serverurl, serverfile);

    //parse the response 
    bool res = parseHTTPResponse(sock_fd, output);
    if (!res) {
        return 1; 
    } 
    return 0;
}