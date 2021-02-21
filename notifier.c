#define _XOPEN_SOURCE (700)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

#define check_error(cond,message)\
do {\
    if (!(cond)) {\
        time_t now = 0;\
        time(&now);\
        char* tmp_time = ctime(&now);\
        tmp_time[strlen(tmp_time) - 1] = 0;\
        fprintf(log, "ERROR: %s  [%s]\n", message, tmp_time);\
        logs = 1;\
    }\
} while (0);

int logs;

void send_emails(char *msg);

int main() {

    FILE* temp = fopen(".notifier_pid", "w");
    if (temp != NULL) {
        fprintf(temp, "%d\n", getpid());
        fclose(temp);
    }

    truncate("logs", 0);
    truncate("email", 0);
    mkdir(".html_files", 0700);
    char* new_file = ".html_files/.link";
    char old_file[50];
    int first = 1;

    FILE *mails = fopen("mails", "r");
    if (mails == NULL) {
        fprintf(stderr, "error while opening \"mails\" file\n");
        exit(EXIT_FAILURE);
    }
    fclose(mails);
    
    while (1) {

        logs = 0;

        FILE *log = fopen("logs", "a");
        if (log == NULL) {
            send_emails("error while opening \"log\" file");
            exit(EXIT_FAILURE);
        }

        struct stat log_info;
        int log_size = 0;
        if (stat("logs", &log_info) != 1) {
            log_size = log_info.st_size;
        }
        
        double sleeping_time = 300;
        FILE *f = fopen("sleeping_time", "r");
        double tmp = 0;
        if (f != NULL) {
            fscanf(f, "%lf", &tmp);
            if (tmp == -1) {
                fclose(f);
                fclose(log);
                exit(EXIT_SUCCESS);
            }
            if (tmp >= 10) {
                sleeping_time = tmp;
            }
            fclose(f);
            f = NULL;
        }
        if (tmp < 10) {
            FILE *f = fopen("sleeping_time", "w");
            if (f != NULL) {
                fprintf(f, "%lld\n", (long long) sleeping_time);
                fclose(f);
            }
        }

        FILE *links = fopen("links", "r");
        check_error(links != NULL, "file \"links\" does not exist");

        FILE *email = fopen("email", "w");
        check_error(email != NULL, "file \"email\" does not exist");

        int updated = 0;
        char *link = 0;
        size_t n = 0;
        int i = 0;
        while (links != NULL && getline(&link, &n, links) > 1) {
                
            char* newline = strrchr(link, '\n');
            if (newline != NULL) {
                *newline = 0;
            }

            sprintf(old_file, ".html_files/.link%d", i);
            struct stat old_finfo;
            int ret_val = stat(old_file, &old_finfo);
            
            if (first || (ret_val == -1 && errno == ENOENT)) {
                pid_t child_pid = fork();
                if (child_pid == 0) {
                    check_error(execlp("wget", "wget", "-q", "--no-check-certificate", "-O", old_file, link, NULL) != -1, "execlp: wget failed");
                }
                wait(NULL);
            }
            else {
                pid_t child_pid = fork();
                if (child_pid == 0) {
                    check_error(execlp("wget", "wget", "-q", "--no-check-certificate", "-O", new_file, link, NULL) != -1, "execlp: wget failed");
                }
                wait(NULL);

                struct stat new_finfo;
                check_error(stat(new_file, &new_finfo) != -1, "wget failed");

                if (email != NULL && old_finfo.st_size != new_finfo.st_size) {
                    fprintf(email, "%s\n", link);
                    updated = 1;
                }
                unlink(old_file);
                rename(new_file, old_file);
            }
            free(link);
            link = NULL;
            n = 0;
            i++;
        }
        
        if (links != NULL) fclose(links);
        links = NULL;
        if (link != NULL) free(link);
        link = NULL;
        
        if (updated && email != NULL) {
            time_t now;
            time(&now);
            fprintf(email, "%s\n", ctime(&now));
        }

        if (logs && log != NULL && email != NULL) {
            fprintf(email, "\nThere are errors:\n");
            char c;
            fclose(log);
            log = fopen("logs", "r");
            fseek(log, log_size, SEEK_SET);
            while ((c = fgetc(log)) != EOF) {
                fprintf(email, "%c", c);
            }
        }

        if (email != NULL) fclose(email);
        email = NULL;

        if (log != NULL) fclose(log);
        log = NULL;
        
        if (updated || logs) {
            send_emails(NULL);
        }

        first = 0;
        sleep(sleeping_time);
    }

    exit(EXIT_SUCCESS);
}

void send_emails(char *msg) {

    if (msg != NULL) {
        FILE* tmp = fopen("email", "w");
        if (tmp == NULL) {
            exit(EXIT_FAILURE);
        }
        fprintf(tmp, "%s\n", msg);
        fclose(tmp);
    }
    
    FILE *mails = fopen("mails", "r");
    if (mails == NULL) {
        FILE* log = fopen("logs", "a");
        if (log == NULL) exit(EXIT_FAILURE);
        fprintf(log, "error while opening log file\nnotifier closed\n");
        fclose(log);
        exit(EXIT_FAILURE);
    }

    char *mail = 0;
    size_t n = 0;
    while (mails != NULL && getline(&mail, &n, mails) > 1) {
        char *newline = strrchr(mail, '\n');
        if (newline != NULL) {
            *newline = 0;
        }
        pid_t child_pid = fork();
        if (child_pid == 0) {
            int fd = open("email", O_RDONLY);
            dup2(fd, STDIN_FILENO);
            execlp("mail", "mail", "-s", "Notification", mail, NULL);
        }
        wait(NULL);
        free(mail);
        mail = NULL;
        n = 0;
    }

    if (mail != NULL) free(mail);
    fclose(mails);
}