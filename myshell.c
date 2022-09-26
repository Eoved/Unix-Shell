#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <fcntl.h>

void myPrint(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}

int main(int argc, char *argv[]) 
{
    char cmd_buff[1024];
    char *pinput;
    char error_message[30] = "An error has occurred\n";
    int batch = 0;
    FILE* file;

    if(argc == 2) {
        batch = 1;
        file = fopen(argv[1], "r");
        if(file == NULL) {
            myPrint(error_message);
            exit(0);
        }
    } else if(argc > 2) {
        myPrint(error_message);
            exit(0);
    }

    while (1) {
        //Checks batch vs regular mode
        if(batch) {
            int blank = 1;
            while(blank) {
                pinput = fgets(cmd_buff, 1024, file);
                if (!pinput) {
                    exit(0);
                }
                int len = strlen(pinput);
                for(int i = 0; i < len; i++) {
                    if(!isspace(pinput[i])) {
                        blank = 0;
                        break;
                    }
                }
            }
        } else {
            myPrint("myshell> ");
            pinput = fgets(cmd_buff, 1024, stdin);
            if (!pinput) {
                exit(0);
            }
        }
        int length = strlen(pinput);
        if(batch) {
            myPrint(pinput);
        }

        if(length > 512 && pinput[512] != '\n') {
            myPrint(error_message);
        } else if((length == 513 && pinput[512] == '\n') || length <= 512) {
            char* buf;
            while((buf = strtok_r(pinput, ";\n", &pinput)) != NULL) {
                //Check for a blank line
                int blank = 1;
                int len = strlen(buf);
                for(int i = 0; i < len; i++) {
                    if(!isspace(buf[i])) {
                        blank = 0;
                        break;
                    }
                }
                if(blank) {
                    continue;
                }

                //Check for redirection/advanced redirection and add spaces
                char commands[516];
                int redir = 0;
                int j = 0;
                int num_gr = 0;
                for(int i = 0; i < strlen(buf); i++) {
                    if((i < strlen(buf) - 1) && (buf[i] == '>') && (buf[i + 1] == '+')) {
                        commands[j] = ' ';
                        commands[j + 1] = buf[i];
                        commands[j + 2] = buf[i + 1];
                        commands[j + 3] = ' ';
                        j += 3;
                        i++;
                        redir = 2;
                        num_gr++;
                    } else if(buf[i] == '>') {
                        commands[j] = ' ';
                        commands[j + 1] = buf[i];
                        commands[j + 2] = ' ';
                        j += 2;
                        redir = 1;
                        num_gr++;
                    } else {
                        commands[j] = buf[i];
                    }
                    j++;
                }
                commands[j] = '\n';
                commands[j + 1] = '\0';

                char temp1[514];
                char temp2[514];
                strcpy(temp1, commands);
                strcpy(temp2, commands);
                int num_args = 0;
                char* args = strtok(temp1, " \t\n");
                while(args != NULL) {
                    num_args++;
                    args = strtok(NULL, " \t\n");
                }
                char* command_args[num_args + 1];
                int spot_redir = 0;
                int spot_adv_redir = 0;
                int spot = 0;
                args = strtok(temp2, " \t\n");
                while(args != NULL) {
                    if(!strcmp(args, ">")) {
                        spot_redir = spot;
                    } else if(!strcmp(args, ">+")) {
                        spot_adv_redir = spot;
                    }
                    command_args[spot] = args;
                    args = strtok(NULL, " \t\n");
                    spot++;
                }
                command_args[spot] = NULL;

                //Parse for built-in commands
                if(!strcmp(command_args[0], "exit")) {
                    if(num_args == 1 && redir == 0) {
                        exit(0);
                    } else {
                        myPrint(error_message);
                    }
                } else if(!strcmp(command_args[0], "pwd")) {
                    if(num_args == 1 && redir == 0) {
                        char dir[514];
                        getcwd(dir, 514);
                        myPrint(dir); 
                        myPrint("\n");
                    } else {
                        myPrint(error_message);
                    }
                } else if(!strcmp(command_args[0], "cd")) {
                    if(num_args == 1 && redir == 0) {
                        int cdr = chdir(getenv("HOME"));
                        if(cdr) {
                            myPrint(error_message);
                        }
                    } else if(num_args == 2 && redir == 0) {
                        char* path;
                        if(!strcmp(command_args[1], "~")) {
                            path = getenv("HOME");
                        } else {
                            path = command_args[1];
                        }
                        int cdr = chdir(path);
                        if(cdr) {
                            myPrint(error_message);
                        }
                    } else {
                        myPrint(error_message);
                    }
                } else {
                    int status;
                    pid_t pid = fork();
                    if(pid == 0) {
                        //Child
                        char** bufboi;
                        char* cmd[spot_redir + 1];
                        if(redir == 1) {
                            //Redirection
                            if(num_gr == 1 && (num_args - spot_redir) == 2) {
                                int fd = open(command_args[num_args - 1], O_RDWR | O_CREAT | O_EXCL, 0664);
                                if(fd >= 0) {
                                    dup2(fd, STDOUT_FILENO);
                                    int i = 0;
                                    while(i < spot_redir) {
                                        cmd[i] = command_args[i];
                                        i++;
                                    }
                                    cmd[i] = NULL;
                                    bufboi = cmd;
                                } else {
                                    myPrint(error_message);
                                    exit(0);
                                }
                            } else {
                                myPrint(error_message);
                                exit(0);
                            }
                        } else if(redir == 2) {
                            //Advanced Redirection
                            if(num_gr == 1 && (num_args - spot_adv_redir) == 2) {
                                int fd = open(command_args[num_args - 1], O_RDWR | O_CREAT | O_APPEND, 0664);
                                int temp_fd = open(".temp", O_RDWR | O_CREAT | O_TRUNC, 0664);
                                char move_buf[1];
                                int n;
                                while((n = read(fd, move_buf, 1)) > 0) {
                                    write(temp_fd, move_buf, 1);
                                }
                                int i = 0;
                                while(i < spot_adv_redir) {
                                    cmd[i] = command_args[i];
                                    i++;
                                }
                                cmd[i] = NULL;
                                bufboi = cmd;
                                //Truncate to clear file
                                close(fd);
                                fd = open(command_args[num_args - 1], O_RDWR | O_CREAT | O_TRUNC, 0664);
                                if(fd >= 0) {
                                    dup2(fd, STDOUT_FILENO);
                                    //execvp for advanced redirection
                                    if(execvp(bufboi[0], bufboi) == -1) {
                                        myPrint(error_message);
                                        exit(0);
                                    }
                                } else {
                                    myPrint(error_message);
                                    exit(0);
                                }
                            } else {
                                myPrint(error_message);
                                exit(0);
                            }
                        } else if(redir == 0) {
                            //Parse for other command
                            bufboi = command_args;
                        }
                        if(redir < 2) {
                            if(execvp(bufboi[0], bufboi) == -1) {
                                myPrint(error_message);
                                exit(0);
                            }
                        }
                    } else {
                        //Parent
                        waitpid(pid, &status, 0);
                        if(redir == 2) {
                            char move_buf[1];
                            int fd = open(command_args[num_args - 1], O_RDWR | O_APPEND, 0664);
                            int temp_fd = open(".temp", O_RDWR, 0664);
                            int n;
                            while((n = read(temp_fd, move_buf, 1)) > 0) {
                                write(fd, move_buf, 1);
                            }
                            close(fd);
                            close(temp_fd);
                            remove(".temp");
                        }
                    }
                }
            }
        } 
    }
}
