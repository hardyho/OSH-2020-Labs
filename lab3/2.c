#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

pthread_mutex_t mutex_send = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
int ready = 1;
int client_num = 0;
int fd_client[32];
char *send_buffer;
int buffer_len = 1024;
int message_len;
int send_id;

struct Node{
    int id;
    struct Node *next;
} Used, Unused;

void *handle_chat(void *data) {
    int *client_id = (int *)data;
    char *buffer;
    int block_len = 1024;
    int length;
    int i , j, id, status;
    struct Node *temp, *current;
    buffer = malloc(block_len * sizeof(char));
    for (temp = Used.next; temp && (*client_id != fd_client[temp->id]); temp = temp->next);
    if (!temp){
        perror("handle_chat");
        return NULL;
    }
    id = temp->id;

    sprintf(buffer, "User %02d enters the room.\n", id);
    length = 25;
    pthread_mutex_lock(&mutex_send);
    while (!ready) {
        pthread_cond_wait(&cv, &mutex_send);
    }
    send_id = -1;
    message_len = length + 1;
    memcpy(send_buffer, buffer, message_len);
    ready = 0;
    pthread_mutex_unlock(&mutex_send);

    length = 16;
    memset(buffer, 0, 25 * sizeof(char));
    sprintf(buffer, "Message from %02d:", id);

    while (1){
        while (((status = recv(*client_id, buffer + length, 1, 0)) > 0) && (buffer[length] != '\n') && (length < block_len - 1)) length++;

        if (status <= 0){
            pthread_mutex_lock(&mutex_send);
            while (!ready) {
                pthread_cond_wait(&cv, &mutex_send);
            }
            for(temp = &Used; temp->next && (temp->next->id != id); temp = temp->next);
            current = temp->next;
            temp->next = current->next;
            current->next = Unused.next;
            Unused.next = current;
            client_num--;
            printf("Disconnected. Client number:%d\n",client_num);
            send_id = -1;
            sprintf(send_buffer, "User %02d has exited.\n", id);
            message_len = 20;
            ready = 0;
            pthread_mutex_unlock(&mutex_send);
            return NULL;
        }

        if (buffer[length] == '\n') {
            printf("message ready to send from %d\n",id);
            pthread_mutex_lock(&mutex_send);
            while (!ready) {
                pthread_cond_wait(&cv, &mutex_send);
            }
            if (length < buffer_len){
                send_id = id;
                message_len = length + 1;
                memcpy(send_buffer, buffer, message_len);
                ready = 0;
            }
            else{
                send_id = id;
                send_buffer = realloc(send_buffer, (length + 1) * sizeof(char));
                message_len = length + 1;
                buffer_len = message_len;
                memcpy(send_buffer, buffer, message_len);
                ready = 0;
            }
            pthread_mutex_unlock(&mutex_send);
            length = 16;
        }

        else {
            buffer = realloc(buffer, (block_len + 1024) * sizeof(char));
            block_len = block_len + 1024;
        }

    }
    return NULL;
}

void *send_message(){
    int i = 0, j;
    struct Node *temp;
    while(1){
        if(!ready){
            pthread_mutex_lock(&mutex_send);
            for (temp = Used.next; temp; temp = temp->next){
                if (temp && temp->id != send_id){
                    int already_send_length = 0;
                    int len;
                    while( already_send_length < message_len){
                        len = send(fd_client[temp->id], send_buffer, message_len, 0);
                        if (temp > 0) already_send_length += len;
                    }
                }
            }
            ready = 1;
            pthread_cond_signal(&cv);
            pthread_mutex_unlock(&mutex_send);
        }
    }
}

int main(int argc, char **argv) {
    int port = atoi(argv[1]);
    pthread_t thread[32], thread_message_handler;
    int i;
    int tag = 0;
    struct Node *temp;
    int fd, fd_temp;
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
    if (listen(fd, 32)) {
        perror("listen");
        return 1;
    }
    send_buffer = malloc(buffer_len * sizeof(char));
    pthread_create(&thread_message_handler, NULL, send_message, NULL);
    Unused.next = NULL;
    Used.next = NULL;
    for( i = 31; i >= 0; i--){
        temp = malloc(sizeof(struct Node));
        temp->id = i;
        temp->next = Unused.next;
        Unused.next = temp;
    }
    while(1){
        while (client_num < 32){
            //if (tag == 1 && client_num == 0) return 0;
            if ((fd_temp = accept(fd, NULL, NULL)) != -1) {
                pthread_mutex_lock(&mutex_send);
                while (!ready) {
                    pthread_cond_wait(&cv, &mutex_send);
                }
                temp = Unused.next;
                i = temp->id;
                fd_client[i] = fd_temp;
                pthread_create(&thread[i], NULL, handle_chat, (void *)&fd_client[i]);
                pthread_detach(thread[i]);
                client_num++;
                printf("Connected. Client number:%d\n",client_num);
                Unused.next = temp->next;
                temp->next = Used.next;
                Used.next = temp;
                tag = 1;
                pthread_mutex_unlock(&mutex_send);
            }
            else{
                perror("accept");
                return 1;
            }
        }
    }
    return 0;
}
