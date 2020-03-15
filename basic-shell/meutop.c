#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

typedef struct process_table_element {
    int  pid;
    int  uid;
    char user [15];
    char procname [15];
    char state;
    struct process_table_element *next;
}proctb_el;

typedef struct process_table {
    proctb_el *head;
    proctb_el *tail;
}proctb;

typedef struct directory_name_element {
    char   dirname [10];
    struct directory_name_element *next;
}dirname_el;

typedef struct directory_name_list {
    dirname_el *head;
    dirname_el *tail;
}dirname_list;

void print_process_table (const proctb *proc_tb){
    fprintf(stdout, "PID    | User          | PROCNAME      | Estado |\n");
    fprintf(stdout, "-------|---------------|---------------|--------|\n");
    if (NULL == proc_tb)
        return;
    proctb_el *current_proc_element = proc_tb->head;
    while (NULL != current_proc_element){
        fprintf(stdout, "%7d| %14s| %14s| %c      |\n", current_proc_element->pid, current_proc_element->user, 
                                                        current_proc_element->procname, current_proc_element->state);
        current_proc_element = current_proc_element->next;
    }
    fprintf(stdout, "-------|---------------|---------------|--------|\n");
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
    memcpy(new_dir->dirname, dirname, 10);
    return dname_list;
}

void destroy_dirname_list (dirname_list *dl){
    dirname_el *current = dl->head;
    dirname_el *next = current->next;
    // Single element list
    if (current == next){
        free(current);
    }
    while (NULL != next){
        dl->head = next;
        free(current);
        current = next;
        next = next->next;
    }
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
    char *current_proc_path = (char*)malloc((str_size1+str_size2+str_size_stat)*sizeof(char));
    if (NULL == current_proc_path){
        fprintf(stderr, "Not enough memory.\n");
        exit(1);
    }
    sprintf(current_proc_path, "%s%s/stat", path1, path2);
    return current_proc_path;
}

proctb* append_proc_to_list (proctb_el proc, proctb* proc_table){
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

void guess_user_from_id(int id, char *name){
    struct passwd *user_info = getpwuid(id);
    memcpy(name, user_info->pw_name, 15);
}

proctb* create_proc_table (const dirname_list *dname_list) {
    struct stat buf;
    proctb_el current_proc;
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
            guess_user_from_id(current_proc.uid, current_proc.user);
            FILE* proc_stat = fopen(current_proc_path, "r");
            // Ignores first character "(" then reads everything that is not a ")", ignores last ")"
            fscanf(proc_stat, "%d %*c%[^)]%*c %c", &current_proc.pid, current_proc.procname, &current_proc.state);
        }
        else{
            fprintf(stderr, "Could not access %s\n", current_proc_path);
            break;
        }
        process_table = append_proc_to_list (current_proc, process_table);
        current = current->next;
    }
    return process_table;
}

int main (int argc, char* argv[]){
    dirname_list *dl = create_dirname_list("/proc/");
    proctb *pt = create_proc_table(dl);
    print_process_table(pt);
    destroy_dirname_list(dl);
    free(dl);
    return 0;
}