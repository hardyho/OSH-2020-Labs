#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int execute(char **args){
    int i;
       	/* 没有输入命令 */
    if (!args[0]){
        return 0;
    }
        /* 内建命令 */
    if (strcmp(args[0], "cd") == 0) {
        if (args[1])
            chdir(args[1]);
        return 0;
    }
    if (strcmp(args[0], "pwd") == 0) {
        char wd[4096];
        puts(getcwd(wd, 4096));
        return 0;
    }
    if (strcmp(args[0], "export") == 0) {
        for (i = 1; args[i] != NULL; i++) {
            /*处理每个变量*/
            char *name = args[i];
            char *value = args[i]+ 1;
            while (*value != '\0' && *value != '=')
                value++;
            *value = '\0';
            value++;
            setenv(name, value, 1);
        }
        return 0;
    }
    if (strcmp(args[0], "exit") == 0) {
        exit(1);
    }
    /* 外部命令 */
    pid_t pid2 = fork();
    if (pid2 == 0) {
        prctl(PR_SET_PDEATHSIG,SIGKILL); // Been added here. In order to kill all son process when the Ctrl+C is detected.
        /* 子进程 */
        execvp(args[0], args);
        /* execvp失败 */
        return 255;
    }
        /* 父进程 */
    waitpid(pid2,NULL,0);
    return 0;
}


// This function is about redirection, including redirect to server
// When you want to redirect to other file, you may need to run the shell as root
int execute_redir(char **args){
    int i, j, In = 0, Out = 0, Cover, tcp_in_flag, tcp_out_flag, port_in, port_out, s_in, s_out;
    char *InFile,*OutFile,tcp_cmp[10], IP_in[16], IP_out[16];
    struct sockaddr_in addr_in, addr_out;
    pid_t pid;
    FILE *fp;
    tcp_in_flag = 0;
    tcp_out_flag = 0;
    for (i = 0; args[i]; i++){
        if (strcmp(args[i], ">") == 0){
            args[i] = NULL;
            //see if the objective start with "/dev/tcp/"
            //if yes, get the IP address and the Port number
            //this part is used in the same way below, when dealing with input redirection
            strncpy(tcp_cmp,args[++i],9);
            if (strcmp(tcp_cmp, "/dev/tcp/") == 0){
                tcp_out_flag = 1;
                for(j = 9; args[i][j]!='/';IP_out[j-10] = args[i][j++]);
                IP_out[j-9] = '\0';
                port_out = atoi(args[i]+j+1);
            }
            else OutFile = args[i];
            Cover = 1;
            Out++;
        }
        if (strcmp(args[i], ">>") == 0){
            args[i] = NULL;
            OutFile = args[++i];
            Cover = 0;
            Out++;
        }
        if (strcmp(args[i], "<") == 0){
            args[i] = NULL;
            strncpy(tcp_cmp,args[++i],9);
            if (strcmp(tcp_cmp, "/dev/tcp/") == 0){
                tcp_in_flag = 1;
                for(j = 9; args[i][j]!='/'; IP_in[j-10] = args[i][j++]);
                IP_in[j-9] = '\0';
                port_in = atoi(args[i]+j+1);
            }
            else InFile = args[i];
            In++;
        }
    }
    if (In == 0 && Out == 0) return execute(args);
    if (In && !tcp_in_flag){
        if ((fp = fopen(InFile,"r")) == NULL) {
            printf("ERROR: Wrong Input File.\n");
            return 0;
        }
        else fclose(fp);
    }
    if (In > 1 || Out > 1){
        printf("ERROR: Too much redirection.\n");
        return 0;
    }
    //create a son process 
    pid = fork();
    if (pid == 0){
        if (In){
            if (tcp_in_flag){
                //use socket() and connect() to connect to the server
                if((s_in = socket(AF_INET,SOCK_STREAM,0))<0){
                    printf("socket error\n");
                    exit(1);
                }
                addr_in.sin_family = AF_INET;
                addr_in.sin_port=htons(port_in);
                addr_in.sin_addr.s_addr = inet_addr(IP_in);
                if (-1 == connect(s_in, (struct sockaddr*)(&addr_in), sizeof(struct sockaddr))){
                    printf("connect error\n");
                    exit(1);
                }
                dup2(s_in, STDIN_FILENO);
            }
            else freopen(InFile, "r", stdin);
        }
        if (tcp_out_flag) printf("Connect Success. IP:%s,port:%d\n",IP_out,port_out);
        if (tcp_in_flag) printf("Connect Success. IP:%s,port:%d\n",IP_in,port_in);
        if (Out && !Cover) freopen(OutFile, "a", stdout);
        if (Out && Cover){
            if (tcp_out_flag){
                //use socket() and connect() to connect to the server
                if((s_out = socket(AF_INET,SOCK_STREAM,0))<0){
                    printf("socket error\n");
                    exit(1);
                }
                addr_out.sin_family = AF_INET;
                addr_out.sin_port = htons(port_out);
                addr_out.sin_addr.s_addr = inet_addr(IP_out);
                if (-1 == connect(s_out, (struct sockaddr*)(&addr_out), sizeof(struct sockaddr))){
                    printf("connect error\n");
                    exit(1);
                }
                dup2(s_out, STDOUT_FILENO);
            }
            else freopen(OutFile, "w", stdout);
        } 
        execute(args);
        if (tcp_in_flag) close(s_in);
        if (tcp_out_flag) close(s_out);
        exit(0);
    }
    else {
        waitpid(pid,NULL,0);
        return 0;
    }
}

// This function is about pipe
// Using recursion
// Each time this function is executed, it examine if there's any pipe structure.
// It creates a son process and executes the left part of the pipe, which only contained one instruction, in the son process.
int pipecreate(char **args) {
    int i,status;
    pid_t pid;
    int fd[2],sfd;
    sfd = dup(STDIN_FILENO); // Seems that it's needed to save the STDIN and reload it after executed the whole instruction, otherwise there may be bug caused bu unknown reason
    for (i = 0; args[i] && *args[i]!='|'; i++);
    if (args[i]) {
        args[i] = NULL;
        pipe(fd);
	    pid = fork();
        if (pid != 0) {
	    waitpid(pid,NULL,0);
	    dup2(fd[0],STDIN_FILENO);
	    close(fd[1]);
	    close(fd[0]);
        pipecreate(&args[i+1]);
	    dup2(sfd,STDIN_FILENO);
	    return 0;
        }
        else {
	    close(fd[0]);
        dup2(fd[1],STDOUT_FILENO);
	    close(fd[1]);
        execute_redir(args); 
	    exit(0);
	    }	
    }
    else return execute_redir(args);
}

// This function is used to handle the signal Ctrl+C 
// Used in main()
void sig_handler(int sig){
    if (sig == SIGINT){
        printf("\n");
        exit(0);
    }
}

// This function is used to check if a char is allowed to occur in the name of an environment variable.
// Used in main() 
int allow_for_env(char a){
    if(((a > 47) && (a < 58)) || ((a > 64) && (a < 91)) || ((a > 96) && (a < 123)) || (a == 95)) return 1;
    else return 0;
}

int main() {
    pid_t pid;
    int status;
    char temp[32],env[256];
    /* 输入的命令行 */
    char cmd[256];
    /* 命令行拆解成的各部分，以空指针结尾 */
    char *args[128];
    int i, j, k;
    while (1) {
        // In order to deal with Ctrl+C, Create a son process here, exit when recieve Ctrl+C
        pid = fork();
        if (pid == 0){
            signal(SIGINT,sig_handler);
            /* 提示符 */
	        printf("# ");
            fflush(stdin);
	        fgets(cmd, 256, stdin);
	        /* 清理结尾的换行符 */
            for (i = 0; cmd[i] != '\n'; i++);
            cmd[i] = '\0';
	        /* 拆解命令行 */
            args[0] = cmd;
            for (i = 0; *args[i]; i++)
                for (args[i+1] = args[i] + 1; *args[i+1]; args[i+1]++)
                    if (*args[i+1] == ' ') {
                        *args[i+1] = '\0';
                        args[i+1]++;
                        break;
                    }
            args[i] = NULL;
            // This part is used to deal with environment variables
            // Use getenv() to replace environment variables with there values.
            for (i = 0; args[i]; i++){
                for (k = 0; args[i][k]; k++){
                    if (args[i][k] == '$'){
                        for (j = k+1; args[i][j] && allow_for_env(args[i][j]); temp[j++ - (1+k)] = args[i][j]);
                        temp[j - (1+k)] = '\0';
                        if (getenv(temp) == NULL){
                            printf("wrong env\n");
                            goto end_of_loop;
                            // In order to avoid any not considered bug, any use of environment variable that's not setted is not allowed
                            // In ordinary shell, it may be replace by NULL.
                        }
                        else{
                            strcpy(env, getenv(temp));
                            if (args[i][j]) strcat(env, args[i]+j);
                            strcpy(args[i]+k, env);
                        }
                    }
                }                
            }
            pipecreate(args);
            end_of_loop: 
            exit(0);
        }
        else{
            // Ignore the Ctrl+C in parent process to avoid exiting the program
            signal(SIGINT,SIG_IGN);
            waitpid(pid, &status, 0);
            if (WEXITSTATUS(status) == 1) return 0; // Special exitcode is set for the instruction 'exit', when detected, exit the program.
        }
    }
}