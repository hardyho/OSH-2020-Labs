#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

struct Pipe {
    int fd_send;
    int fd_recv;
};

void *handle_chat(void *data) {
    struct Pipe *pipe = (struct Pipe *)data;
    char *buffer;
    int block_len = 1024;
    int length = 8;
    int i = 0;
    int status;
    buffer = malloc(block_len * sizeof(char));
    strcpy(buffer,"Message:");
    ssize_t len;
    while (1){
        // To handle `/n`, recv one char each time
        while (((status = recv(pipe->fd_send, buffer + length, 1, 0)) > 0) && (buffer[length] != '\n') && (length < block_len - 1)) length++;
        if (status <= 0) return NULL;
        if (buffer[length] == '\n') {
            while ((length - i * 1024) >= 1024){
                send(pipe->fd_recv, buffer + 1024 * i, 1024, 0);
                i++;
            }
            send(pipe->fd_recv, buffer + 1024 * i, length % 1024 + 1, 0);
            length = 8;
            i = 0;
        }                          
        else { // If not enough space, realloc
            buffer = realloc(buffer, (block_len + 1024) * sizeof(char));
            block_len = block_len + 1024;
        }
    }
    return NULL;

}

int main(int argc, char **argv) {
    int port = atoi(argv[1]);
    int fd;
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        return 1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    socklen_t addr_len = sizeof(addr);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind");
        return 1;
    }
    if (listen(fd, 2)) {
        perror("listen");
        return 1;
    }
    int fd1 = accept(fd, NULL, NULL);
    int fd2 = accept(fd, NULL, NULL);
    if (fd1 == -1 || fd2 == -1) {
        perror("accept");
        return 1;
    }
    pthread_t thread1, thread2;
    struct Pipe pipe1;
    struct Pipe pipe2;
    pipe1.fd_send = fd1;
    pipe1.fd_recv = fd2;
    pipe2.fd_send = fd2;
    pipe2.fd_recv = fd1;
    pthread_create(&thread1, NULL, handle_chat, (void *)&pipe1);
    pthread_create(&thread2, NULL, handle_chat, (void *)&pipe2);
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    return 0;
}
