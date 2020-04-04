#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>


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
    if (strcmp(args[0], "exit") == 0)
        return 1;
    /* 外部命令 */
    pid_t pid2 = fork();
    if (pid2 == 0) {
        /* 子进程 */
        execvp(args[0], args);
        /* execvp失败 */
        return 255;
    }
        /* 父进程 */
    waitpid(pid2,NULL,0);
    return 0;
}


int execute_redir(char **args){
    int i, In = 0, Out = 0, Cover;
    char *InFile,*OutFile;
    pid_t pid;
    FILE *fp;
    for (i=0; args[i]; i++){
        if (strcmp(args[i], ">") == 0){
            args[i] = NULL;
            OutFile = args[++i];
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
            InFile = args[++i];
            In++;
        }
    }
    if (In == 0 && Out == 0) return execute(args);
    if (In){
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
    pid = fork();
    if (pid == 0){
        if (In) freopen(InFile, "r", stdin);
        if (Out && Cover) freopen(OutFile, "w", stdout);
        if (Out && !Cover) freopen(OutFile, "a", stdout);
        execute(args);
        exit(0);
    }
    else {
        waitpid(pid,NULL,0);
        return 0;
    }
}


int pipecreate(char **args) {
    int i,status;
    pid_t pid;
    int fd[2],sfd;
    sfd = dup(STDIN_FILENO);
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


int main() {
    /* 输入的命令行 */
    char cmd[256];
    /* 命令行拆解成的各部分，以空指针结尾 */
    char *args[128];
    int i;
    while (1) {
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
	if (pipecreate(args) == 1) return 0;
    }
}



