#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>

#define MAX_LEN 514
#define EXIT "exit"
#define CD "cd"
#define PWD "pwd"
#define SPACE " "
#define TAB "\t"
    
void myPrint(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}

void print_error() {
    char error_message[30] = "An error has occurred\n";
    write(STDOUT_FILENO, error_message, strlen(error_message));
}

/*
   Clean whitespace, return arr of cmd + args
 */
char** clean_cmd(char* cmd, int* size) {
    int len = 0;
    int i = 0;
    char* delims = " \t\n";
    char* saveptr = NULL;
    char** cmd_buf = (char**) calloc(MAX_LEN, sizeof(char*));

    char* token = strtok_r(cmd, delims, &saveptr);
    while (token != NULL) {
        cmd_buf[i++] = token;
        token = strtok_r(NULL, delims, &saveptr);
    }

    i = 0;
    while (cmd_buf[i] != NULL) {
        len++;
        i++;
    }

    *size = len;
    char** full_cmd = (char**) malloc(sizeof(char*) * len);

    i = 0;
    while (cmd_buf[i] != NULL) {
        full_cmd[i] = cmd_buf[i];
        i++;
    }

    free(cmd_buf);

    return full_cmd;
}

/*
   Make temporary copy of file to help with advanced redirection
*/
void copy_file(char* fname) {
    FILE* oldfp = fopen(fname, "r");
    FILE* tempfp = fopen("temp", "w");
    int outstream = dup(STDOUT_FILENO);
    char buf[MAX_LEN];
    char* finput;

    int fd = fileno(tempfp);
    if (dup2(fd, STDOUT_FILENO) < 0) {
        print_error();
        exit(1);
    }

    while (1) {
        finput = fgets(buf, MAX_LEN, oldfp);
        if (finput == NULL) {
            break;
        }
        myPrint(finput);
    }

    fclose(oldfp);
    fclose(tempfp);
    dup2(outstream, STDOUT_FILENO);
}

/*
   Check if the called program should be redirected and if so, redirect
   STDOUT accordingly. Return program call with args without "> file" or
   NULL on failed redirection. Update size of returned arr by reference.
*/
char** handle_redir(char** prog_arr, int* prog_size, char* out_file) {
    char* buf = (char*) malloc(sizeof(char) * MAX_LEN);

    // flatten prog_arr
    int i, j, k;
    k = 0;
    for (i = 0; i < *prog_size; i++) {
        j = 0;
        while(prog_arr[i][j]) {
            buf[k++] = prog_arr[i][j++];
        }
    }
    buf[k] = '\0';

    char* redir_exist = strstr(buf, ">");
    char* adv_exist = strstr(buf, ">+");
    int adv = (adv_exist == NULL) ? 0:1;
    char* redir_marker = adv ? ">+":">";

    // no redirection so continue
    if (redir_exist == NULL) {
        free(buf);
        return prog_arr;
    }

    // no output file specified or multiple > present
    if (adv) {
        if ((strlen(adv_exist) == 2) || (strstr(adv_exist + 1, redir_marker))) {
            print_error();
            return NULL;
        }
    }
    else {
        if ((strlen(redir_exist) == 1) || (strstr(redir_exist + 1, redir_marker))) {
            print_error();
            return NULL;
        }
    }

    // separate program call and output file
    char** prog_and_file = (char**) calloc(2, sizeof(char*));
    int ct = 0;
    char* saveptr = NULL;
    char* token = strtok_r(buf, redir_marker, &saveptr);
    while (token != NULL) {
        prog_and_file[ct++] = token;
        token = strtok_r(NULL, redir_marker, &saveptr);
        if (ct > 2) {
            print_error();
            free(prog_and_file);
            return NULL;
        }
    }
    char fname[MAX_LEN];
    strcpy(fname, prog_and_file[1]);

    if (access(fname, F_OK) == 0) {
        if (adv) { // adv and file already exists -> out_file value is external flag
            strcpy(out_file, fname);
            copy_file(fname);
        }
        else {
            print_error();
            return NULL;
        }
    }

    char prog_name[MAX_LEN];
    strcpy(prog_name, prog_and_file[0]);

    free(buf);
    free(prog_and_file);

    if ((strcmp(prog_name, CD) == 0) || (strcmp(prog_name, PWD) == 0) || (strcmp(prog_name, EXIT) == 0)) {
        print_error();
        return NULL;
    }

    FILE* outfp = fopen(fname, "w");
    if (outfp == NULL) {
        print_error();
        return NULL;
    }
    int fd = fileno(outfp);
    if (fd < 0) {
        print_error();
        return NULL;
    }
    if (dup2(fd, STDOUT_FILENO) < 0) {
        print_error();
        exit(1);
    }
    close(fd);

    // find where >/>+ is in ret
    int ret_size = 0;
    char** ret = (char**) calloc(*prog_size, sizeof(char*));
    for (i = 0; i < *prog_size; i++) {
        ret[i] = prog_arr[i];
        if (strstr(prog_arr[i], redir_marker) != NULL) {
            ret_size = i + 1;
            break;
        }
    }

    ct = 0;
    while (ret[ret_size - 1][ct]) {
        if (ret[ret_size - 1][ct] == '>') {
            break;
        }
        ct++;
    }

    // replace the str with > unless it will be empty
    if (ct < 1) {
        ret_size--;
    }
    else {
        char* replacement = (char*) malloc(sizeof(char) * ct);
        for (i = 0; i < ct; i++) {
            replacement[i] = ret[ret_size - 1][i];
        }
        strcpy(ret[ret_size - 1], replacement);
        free(replacement);
    }

    *prog_size = ret_size;

    return ret;
}

/*
   Appends contents of src to end of dest
*/
void append_file(char* src, char* dest) {
    FILE* srcfp = fopen(src, "r");
    FILE* destfp = fopen(dest, "a");
    int outstream = dup(STDOUT_FILENO);
    char buf[MAX_LEN];
    char* finput;

    int fd = fileno(destfp);
    if (dup2(fd, STDOUT_FILENO) < 0) {
        print_error();
        exit(1);
    }

    while (1) {
        finput = fgets(buf, MAX_LEN, srcfp);
        if (finput == NULL) {
            break;
        }
        myPrint(finput);
    }

    fclose(srcfp);
    fclose(destfp);
    dup2(outstream, STDOUT_FILENO);
}

/*
   Execute programs
 */
void exec_prog(char** prog_arr, int prog_size) {
    int outstream = dup(STDOUT_FILENO);
    int size = prog_size;
    char out_file[MAX_LEN];
    out_file[0] = '\0';
    char** prog_and_args = handle_redir(prog_arr, &size, out_file);

    if (prog_and_args == NULL) return;

    pid_t p = fork();
    if (p < 0) {
        print_error();
    }
    else if (p == 0) { // child process
        char** args = (char**) calloc((size + 1), sizeof(char*));
        char* prog = prog_and_args[0];

        for (int i = 0; i < size; i++) {
            args[i] = prog_and_args[i];
        }

        int status = execvp(prog, args);
        if (status < 0) {
            print_error();
            free(args);
            exit(1);
        }

        free(args);
        exit(0);
    }
    else { // parent process
        wait(NULL);

        // redirect back to stdout
        dup2(outstream, STDOUT_FILENO);
        close(outstream);

        // if advanced redirection and file already exists
        if (out_file[0] != '\0') {
            append_file("temp", out_file);
            remove("temp");
        }
    }
}

/*
   Handle general commands
 */
void handle_cmd(char* raw_cmd) {
    int cmd_size;
    char** full_cmd = clean_cmd(raw_cmd, &cmd_size);
    if (cmd_size < 1) return;
    char* cmd = full_cmd[0];

    if (strcmp(cmd, CD) == 0) {
        if (cmd_size > 2) {
            print_error();
            return;
        }
        char* path;
        int err;
        path = (cmd_size == 1) ? getenv("HOME") : full_cmd[1];
        err = chdir(path);
        if (err) {
            print_error();
        }
    }
    else if (strcmp(cmd, PWD) == 0) {
        if (cmd_size > 1) {
            print_error();
            return;
        }
        char dir_buf[MAX_LEN];
        myPrint(getcwd(dir_buf, MAX_LEN));
        myPrint("\n");
    }
    else if (strcmp(cmd, EXIT) == 0) {
        if (cmd_size > 1) {
            print_error();
            free(full_cmd);
            return;
        }
        free(full_cmd);
        exit(0);
    }
    else {
        exec_prog(full_cmd, cmd_size);
    }

    free(full_cmd);
}

/*
   Check if input string is all whitespace. Return 1 if so, 0 otherwise
*/
int is_whitespace(char* input) {
    int i = 0;
    while (input[i]) {
        if (!isspace(input[i])) {
            return 0;
        }
        i++;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    FILE* instream = stdin;
    int batch = 0;

    if (argc == 2) {
        FILE* fp = fopen(argv[1], "r");
        if (fp == NULL) {
            print_error();
            exit(1);
        }
        batch = 1;
        instream = fp;
    }
    else if (argc > 2) {
        print_error();
        exit(1);
    }

    char cmd_buff[MAX_LEN];
    char *pinput;
    char dir_buff[MAX_LEN];

    while (1) {
        if (getcwd(dir_buff, MAX_LEN) == NULL) {
            print_error();
            exit(1);
        }
        if (!batch) {
            myPrint(dir_buff);
            myPrint("> ");
        }
        pinput = fgets(cmd_buff, MAX_LEN, instream);
        if(pinput == NULL) exit(0);
        if (is_whitespace(pinput)) continue;

        if (strstr(pinput, "\n") == NULL) {
            while (strstr(pinput, "\n") == NULL) {
                myPrint(pinput);
                pinput = fgets(cmd_buff, MAX_LEN, instream);
            }
            myPrint(pinput);
            print_error();
            continue;
        }
        // myPrint(pinput);
        if (strstr(pinput, ";") != NULL) {
            char* semi_tok = strtok(pinput, ";");
            while (semi_tok != NULL) {
                handle_cmd(semi_tok);
                semi_tok = strtok(NULL, ";");
            }
            continue;
        }
        handle_cmd(pinput);
    }
}
