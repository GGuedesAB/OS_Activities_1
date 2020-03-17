#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <ncurses.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define PROC_NAME_MAX_LENGTH 25

typedef struct process_table_element {
    int  pid;
    int  uid;
    char user [15];
    char procname [PROC_NAME_MAX_LENGTH];
    char state;
    struct process_table_element *next;
}proctb_el;

typedef struct process_table {
    proctb_el *head;
    proctb_el *tail;
}proctb;

typedef struct directory_name_element {
    char   dirname [32];
    struct directory_name_element *next;
}dirname_el;

typedef struct directory_name_list {
    dirname_el *head;
    dirname_el *tail;
}dirname_list;

int count_processes(const proctb *);

char* process_table_to_string (const proctb *proc_tb){
    int lines = 3;
    int line_width = 61;
    lines += count_processes(proc_tb);
    char* process_table_as_string = (char*)malloc(lines*line_width*sizeof(char));
    sprintf(process_table_as_string, "PID    | User          | PROCNAME                | Estado |\n");
    sprintf(process_table_as_string, "%s-------|---------------|-------------------------|--------|\n", process_table_as_string);
    if (NULL == proc_tb)
        return NULL;
    proctb_el *current_proc_element = proc_tb->head;
    while (NULL != current_proc_element){
        sprintf(process_table_as_string, "%s%7d| %14s| %24s| %c      |\n", process_table_as_string, current_proc_element->pid, current_proc_element->user, 
                                                                           current_proc_element->procname, current_proc_element->state);
        current_proc_element = current_proc_element->next;
    }
    sprintf(process_table_as_string, "%s-------|---------------|-------------------------|--------|\n", process_table_as_string);
    return process_table_as_string;
}

int count_processes(const proctb *proc_tb){
    if (NULL == proc_tb)
        return 0;
    int number_of_processes = 0;
    proctb_el *current_proc_element = proc_tb->head;
    while (NULL != current_proc_element){
        number_of_processes++;
        current_proc_element = current_proc_element->next;
    }
    return number_of_processes;
}

int is_a_proc_number(const char *name){
    char *endptr;
    int proc_number = (int)strtol(name,&endptr,10);
    if (0 != strcmp(endptr,"")){
        return 0;
    }
    return proc_number;
}

dirname_list* append_dirname_to_list (const char *dirname, dirname_list *dname_list){
    dirname_el *new_dir = (dirname_el*)malloc(sizeof(dirname_el));
    if (NULL == new_dir){
        fprintf(stderr, "Not enough memory.\n");
        exit(1);
    }
    memcpy(new_dir->dirname, dirname, 32);
    if (NULL != dname_list->head){
        dname_list->tail->next = new_dir;
        dname_list->tail = new_dir;
        dname_list->tail->next = NULL;
    }
    else {
        dname_list->head = new_dir;
        dname_list->tail = new_dir;
        dname_list->tail->next = NULL;
    }
    return dname_list;
}

void destroy_dirname_list (dirname_list *dl){
    if (NULL == dl){
        return;
    }
    dirname_el *current = dl->head;
    dirname_el *next = current->next;
    // Single element list
    if (NULL == next){
        free(current);
        return;
    }
    while (NULL != next){
        dl->head = next;
        free(current);
        current = next;
        next = next->next;
    }
    free(current);
}

dirname_list* create_dirname_list (const char *path){
    DIR *proc = opendir(path);
    struct dirent *dirs_inside_proc;
    dirname_list *dname_list = (dirname_list*)malloc(sizeof(dirname_list));
    if (NULL == dname_list){
        fprintf(stderr, "Not enough memory.\n");
        exit(1);
    }
    dname_list->head=NULL;
    dname_list->tail=NULL;
    while((dirs_inside_proc = readdir(proc))){
        if (is_a_proc_number(dirs_inside_proc->d_name)){
            dname_list = append_dirname_to_list(dirs_inside_proc->d_name, dname_list);
        }
    }
    closedir(proc);
    return dname_list;
}

char* concatenate_paths_with_stat(const char *path1, const char *path2){
    const int str_size1 = strlen(path1);
    const int str_size2 = strlen(path2);
    const int str_size_stat = strlen("/stat");
    char *current_proc_path = (char*)malloc((str_size1+str_size2+str_size_stat+1)*sizeof(char));
    if (NULL == current_proc_path){
        fprintf(stderr, "Not enough memory.\n");
        exit(1);
    }
    sprintf(current_proc_path, "%s%s/stat", path1, path2);
    return current_proc_path;
}

proctb* append_proc_to_table (proctb_el proc, proctb* proc_table){
    proctb_el *new_proc = (proctb_el*)malloc(sizeof(proctb_el));
    if (NULL == new_proc){
        fprintf(stderr, "Not enough memory.\n");
        exit(1);
    }
    new_proc->next = NULL;
    memcpy(new_proc, &proc, sizeof(proctb_el));
    if (NULL == proc_table->head){
        proc_table->head = new_proc;
        proc_table->tail = new_proc;
    }
    else {
        proc_table->tail->next = new_proc;
        proc_table->tail = new_proc;
    }
    return proc_table;
}

char* guess_user_from_id(int id){
    struct passwd *user_info = getpwuid(id);
    char* username = user_info->pw_name;
    return username;
}

proctb* create_process_table (const dirname_list *dname_list) {
    struct stat buf;
    proctb_el current_proc;
    current_proc.next=NULL;
    current_proc.pid=-1;
    current_proc.state=0;
    current_proc.uid=-1;
    proctb *process_table = (proctb*)malloc(sizeof(proctb));
    if (NULL == process_table){
        fprintf(stderr, "Not enough memory.\n");
        exit(1);
    }
    process_table->head = NULL;
    process_table->tail = NULL;
    dirname_el *current = dname_list->head;
    while (NULL != current) {
        char *current_proc_path = concatenate_paths_with_stat("/proc/",current->dirname);
        if (0 == stat(current_proc_path, &buf)){
            current_proc.uid = buf.st_uid;
            memcpy(current_proc.user, guess_user_from_id(current_proc.uid), 15);
            FILE* proc_stat = fopen(current_proc_path, "r");
            // Ignores first character "(" then reads first 24 chrs that are not ")", ignores last ")"
            fscanf(proc_stat, "%d %*c%24[^)]%*c %c", &current_proc.pid, current_proc.procname, &current_proc.state);
            fclose(proc_stat);
        }
        process_table = append_proc_to_table (current_proc, process_table);
        current = current->next;
        free(current_proc_path);
    }
    return process_table;
}

void destroy_process_table (proctb *ptb){
    if (NULL == ptb){
        return;
    }
    proctb_el *current = ptb->head;
    proctb_el *next = current->next;
    // Single element list
    if (NULL == next){
        free(current);
        return;
    }
    while (NULL != next){
        ptb->head = next;
        free(current);
        current = next;
        next = next->next;
    }
    free(current);
}

int main (int argc, char* argv[]){
    char buf[100] = {0}, *s = buf;
    int ch, pid = -1, sig = -1;
    WINDOW *w;

    if ((w = initscr()) == NULL) {
        fprintf(stderr, "Error: initscr()\n");
        exit(EXIT_FAILURE);
    }
    keypad(stdscr, TRUE);
    noecho();
    cbreak();      // disable line-buffering
    timeout(100);  // wait 100 milliseconds for input

    while (pid != 0) {
        erase();
        dirname_list *dl = create_dirname_list("/proc/");
        proctb *pt = create_process_table(dl);
        char* process_table = process_table_to_string(pt);
        mvprintw(0, 0, "%s", process_table);
        mvprintw(LINES-2, 0, "PID:%d SIG:%d", pid, sig);
        mvprintw(LINES-1, 0, ">: %s", buf);
        free(process_table);
        destroy_dirname_list(dl);
        free(dl);
        destroy_process_table(pt);
        free(pt);
        refresh();
        // getch (with cbreak and timeout as above)
        // waits 100ms and returns ERR if it doesn't read anything.
        if ((ch = getch()) != ERR) {
            if (ch == '\n') {
                *s = 0;
                sscanf(buf, "%d %d", &pid, &sig);
                kill(pid, sig);
                s = buf;
                *s = 0;
            }
            else if (ch == KEY_BACKSPACE) {
                if (s > buf)
                    *--s = 0;
            }
            else if (s - buf < (long)sizeof buf - 1) {
                *s++ = ch;
                *s = 0;
            }
        }
    }

    delwin(w);
    endwin();
    //printf("%s", process_table);
    return 0;
}