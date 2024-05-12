#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <mutex>
#include <vector>

std::mutex mtx;
std::vector<int> clients;

void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }

void usage() {
    FILE *file = fopen("../version.txt", "r");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        printf("tcp server %s\n", line);
    }

    fclose(file);

    printf("\n");
    printf("syntax: echo-client <port> [-e] [-b] [-si <src ip>]\n");
    printf("  -e : echo\t  -b : broadcast\n");
    printf("sample: echo-client 1234\n");
}

struct Param {
    bool echo{false};
    bool broadcast{false};
    uint16_t port{0};
    uint32_t srcIp{0};

    bool parse(int argc, char* argv[]) {
        for (int i = 1; i < argc;) {
            if (strcmp(argv[i], "-e") == 0) {
                echo = true;
                i++;
                continue;
            }

            if (strcmp(argv[i], "-b") == 0){
                broadcast = true;
                i++;
                continue;
            }

            if (strcmp(argv[i], "-si") == 0) {
                int res = inet_pton(AF_INET, argv[i + 1], &srcIp);
                switch (res) {
                case 1: break;
                case 0: fprintf(stderr, "not a valid network address\n"); return false;
                case -1: myerror("inet_pton"); return false;
                }
                i += 2;
                continue;
            }

            if (i < argc) port = atoi(argv[i++]);
        }
        return port != 0;
    }
} param;

void recvThread(int sd) {
    printf("connected\n");
    fflush(stdout);
    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];
    while (true) {
        ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
        if (res == 0 || res == -1) {
            fprintf(stderr, "recv return %ld", res);
            myerror(" ");
            break;
        }
        buf[res] = '\0';
        printf("%s", buf);
        fflush(stdout);
        if (param.echo) {
            res = ::send(sd, buf, res, 0);
            if (res == 0 || res == -1) {
                fprintf(stderr, "send return %ld", res);
                myerror(" ");
                break;
            }
        }
        if (param.broadcast){
            std::lock_guard<std::mutex> guard(mtx);
            for (int i = 0; i < clients.size(); ++i){
                int clientSocket = clients[i];
                if (clientSocket != sd) {
                    int error = 0;
                    socklen_t len = sizeof(error);
                    if (getsockopt(clientSocket, SOL_SOCKET, SO_ERROR, &error, &len) != 0 || error != 0) {
                        // fprintf(stderr, "Socket error or not valid: %d\n", clientSocket);
                        continue;
                    }

                    ssize_t send_res = ::send(clientSocket, buf, res, 0);
                    if (send_res == -1){
                        myerror("send");
                    }
                }
            }
        }
    }
    printf("disconnected\n");
    fflush(stdout);
    ::close(sd);
}

int main(int argc, char* argv[]) {
    if (!param.parse(argc, argv)) {
        usage();
        return -1;
    }

    //
    // socket
    //
    int sd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        myerror("socket");
        return -1;
    }

    //
    // setsockopt
    //
    {
        int optval = 1;
        int res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        if (res == -1) {
            myerror("setsockopt");
            return -1;
        }
    }

    //
    // bind
    //
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = param.srcIp;
        addr.sin_port = htons(param.port);

        ssize_t res = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
        if (res == -1) {
            myerror("bind");
            return -1;
        }
    }

    //
    // listen
    //
    {
        int res = listen(sd, 5);
        if (res == -1) {
            myerror("listen");
            return -1;
        }
    }

    while (true) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int newsd = ::accept(sd, (struct sockaddr *)&addr, &len);
        if (newsd == -1) {
            myerror("accept");
            break;
        }

        clients.push_back(newsd);
        std::thread* t = new std::thread(recvThread, newsd);
        t->detach();
    }
    ::close(sd);
}
