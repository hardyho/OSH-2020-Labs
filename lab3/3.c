#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>

int main(int argc, char **argv) {
    int client_num = 0;
        int fd_client[32];

    struct message_node{
        int length;
        char *message_buffer;
        int send_from;
        int number;
        int block_length;
        struct message_node *next;
    } head, *rear, *new_message, *unfinished[32], *message_ptr; //To save message, including completed messages and unfinished messages

    struct fd_node{
        int id;
        struct fd_node *next;
    } Used, Unused;  

    struct client_message_manager{
        struct message_node *current_message;
        int already_send_length;
        int to_send_length;
    } message_manager[32]; // For each clients, there's a message manager, which can handle what message should be sent

    int port = atoi(argv[1]);
    int i;
    struct fd_node *temp, *temp_1;
    int fd, fd_temp, max_client_fd = 0;
    struct timeval timeout;
    fd_set server,clients;
    int status;
    int message_latest_id = 0;
    char *buffer;
    int block_len, length;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        return 1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

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

    Unused.next = NULL;
    Used.next = NULL;
    for(i = 31; i >= 0; i--){
        temp = malloc(sizeof(struct fd_node));
        temp->id = i;
        temp->next = Unused.next;
        Unused.next = temp;
    }

    for(i = 0; i < 32; i++){
        message_manager[i].current_message = &head;
        message_manager[i].to_send_length = 0;
        message_manager[i].already_send_length = 0;
    }

    for(i = 0; i < 32; i++){
        unfinished[i] = NULL;
    }

    head.next = NULL;
    head.length = 0;
    head.number = 0;
    rear = &head;

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;



    printf("Server is ready.\n");

    while(1){
        if (client_num < 32){
            FD_ZERO(&server);
            FD_SET(fd, &server);
            switch(select(fd + 1, &server, NULL, NULL, &timeout)){
                case -1:
                    perror("select");
                    break;   
                case 0:
                    break; 
                default:
                    if (FD_ISSET(fd, &server)){
                        printf("Ready to accept.\n");
                        if ((fd_temp = accept(fd, NULL, NULL)) != -1){
                            fcntl(fd_temp, F_SETFL, fcntl(fd_temp, F_GETFL, 0) | O_NONBLOCK);
                            temp = Unused.next;
                            i = temp->id;
                            fd_client[i] = fd_temp;
                            client_num++;
                            Unused.next = temp->next;
                            temp->next = Used.next;
                            Used.next = temp;
                            message_manager[i].current_message = rear;
                            message_manager[i].to_send_length = 0;
                            printf("User%d Connecting to server. FD:%d\n", i, fd_client[i]);
                            printf("%d Users connected.\n\n",client_num);

                            new_message = malloc(sizeof(struct message_node));
                            buffer = malloc(sizeof(char) * 35);
                            sprintf(buffer, "User %02d has entered the room.\n", temp->id);
                            new_message->length = 30;
                            new_message->message_buffer = buffer;
                            new_message->send_from = -1;
                            new_message->number = message_latest_id++;
                            new_message->next = NULL;
                            rear->next = new_message;
                            rear = new_message;
                        }
                        else{
                            perror("accept");
                            return 1;
                        }
                    }
                    break;
                  //default
            } //switch
        }
        

        if (client_num > 0){
            FD_ZERO(&clients);
            max_client_fd = 0;
            for(temp = Used.next; temp; temp = temp->next){
                FD_SET(fd_client[temp->id], &clients);
                if (fd_client[temp->id] > max_client_fd) max_client_fd = fd_client[temp->id];
            }
            
            switch(select(max_client_fd + 1, &clients, NULL, NULL, &timeout)){
                case -1:{
                    perror("select");
                    break;
                }   
                case 0:{
                    break;
                }  
                default:{
                    printf("RECIEVE.\n");
                    for(temp_1 = &Used; temp_1->next; temp_1 = temp_1->next){
                        temp = temp_1->next;
                        if (FD_ISSET(fd_client[temp->id], &clients)){
                            //printf("%d Ready to recieve.\n",temp->id);
                            /* If there's an unfinished message*/
                            if (unfinished[temp->id] == NULL){ 
                                new_message = malloc(sizeof(struct message_node));
                                buffer = malloc(sizeof(char) * 1024);
                                block_len = 1024;
                                sprintf(buffer, "Message from %02d:", temp->id);
                                length = 16;
                                new_message->message_buffer = buffer;
                                new_message->send_from = temp->id;
                            }
                            /* Else create a new one*/
                            else {
                                new_message = unfinished[temp->id];
                                length = new_message->length;
                                block_len = new_message->block_length;
                                buffer = new_message->message_buffer;
                            }

                            int flag = 0;
                            while(1){ //End when the message is recieved completely or the client is disconnected. 
                                //printf("length:%d,block_length:%d\n", length, block_len);
                                while (((status = recv(fd_client[temp->id], buffer + length, 1, 0)) > 0) && (buffer[length] != '\n') && (length < block_len - 1)){
                                    length++;
                                    flag = 1;
                                    //printf("%d",flag);
                                }

                                if (status <= 0){
                                    if (flag == 0){
                                        printf("User%d disconnected.\n",temp->id);
                                        //Disconnect
                                        if (temp->next) temp_1->next = temp->next;
                                        else temp_1->next = NULL;
                                        temp->next = Unused.next;
                                        Unused.next = temp;
                                        client_num--;
                                        printf("%d Users connected.\n\n",client_num);
                                        new_message = malloc(sizeof(struct message_node));
                                        buffer = malloc(sizeof(char) * 35);
                                        sprintf(buffer, "User %02d has left the room.\n", temp->id);
                                        new_message->length = 27;
                                        new_message->message_buffer = buffer;
                                        new_message->send_from = -1;
                                        new_message->number = message_latest_id++;
                                        new_message->next = NULL;
                                        rear->next = new_message;
                                        rear = new_message;
                                        unfinished[temp->id] = NULL;
                                    }
                                    else{
                                        //printf("Reach the end.message:%s\n",buffer);
                                        new_message->length = length;
                                        new_message->block_length = block_len;
                                        unfinished[temp->id] = new_message;
                                        //printf("Unfinished[%d]:%s length:%d",temp->id, new_message->message_buffer, length);
                                    }
                                    goto End_Of_Loop;
                                }

                                else{
                                    if (buffer[length] == '\n'){
                                        printf("message ready to send from %d\n",temp->id);
                                        new_message->length = length + 1;
                                        new_message->number = message_latest_id++;
                                        new_message->next = NULL;
                                        rear->next = new_message;
                                        rear = new_message;
                                        printf("message:%s\n",rear->message_buffer);
                                        new_message = malloc(sizeof(struct message_node));
                                        buffer = malloc(sizeof(char) * 1024);
                                        block_len = 1024;
                                        sprintf(buffer, "Message from %02d:", temp->id);
                                        length = 16;
                                        new_message->message_buffer = buffer;
                                        new_message->send_from = temp->id;
                                    }
                                    else{
                                        printf("Realloc.\n");
                                        buffer = realloc(buffer, (block_len + 1024) * sizeof(char));
                                        block_len = block_len + 1024;
                                        new_message->message_buffer = buffer;
                                    }
                                }  
                            } //while                    
                        } //if FD_ISSET
                    }
                }
            }//switch

            End_Of_Loop: 1;

            FD_ZERO(&clients);
            max_client_fd = 0;
            for(temp = Used.next; temp; temp = temp->next){
                FD_SET(fd_client[temp->id], &clients);
                if (fd_client[temp->id] > max_client_fd) max_client_fd = fd_client[temp->id];
            }

            switch(select(max_client_fd + 1, NULL, &clients, NULL, &timeout)){
                case -1:{
                    perror("select");
                    break;
                }    
                case 0:{
                    break;
                }  
                default:{
                    for(temp = Used.next; temp; temp = temp->next){
                        if (FD_ISSET(fd_client[temp->id], &clients)){
                            //printf("%d Ready to send.",temp->id);
                            if (message_manager[temp->id].to_send_length == 0){
                                if (message_manager[temp->id].current_message->next){
                                    //printf("New message!\n");
                                    message_manager[temp->id].current_message = message_manager[temp->id].current_message->next;
                                    if (temp->id != message_manager[temp->id].current_message->send_from){
                                        message_manager[temp->id].to_send_length = message_manager[temp->id].current_message->length;
                                        message_manager[temp->id].already_send_length = 0;
                                    }     
                                }
                                else continue;
                            } 
                            if (message_manager[temp->id].to_send_length > 0){
                                //printf("The message:%s",message_manager[temp->id].current_message->message_buffer);
                                int len = send(fd_client[temp->id], message_manager[temp->id].current_message->message_buffer + message_manager[temp->id].already_send_length, message_manager[temp->id].to_send_length, 0);
                                if (len > 0){
                                    message_manager[temp->id].already_send_length += len;
                                    message_manager[temp->id].to_send_length -= len;
                                }
                            }  
                        }
                    } //for          
                } //default
            } //switch

            int min_message_number = 1000;
            for(temp = Used.next; temp; temp = temp->next){
                if (message_manager[temp->id].current_message->number < min_message_number) min_message_number = message_manager[temp->id].current_message->number;
            }
            while (head.next && head.next->number < min_message_number){
                message_ptr = head.next;
                head.next = message_ptr->next;
                free(message_ptr->message_buffer);
                free(message_ptr);
                //printf("message deleted.\n");
            }
            if (!head.next) rear = &head;
        } 
    }
}
