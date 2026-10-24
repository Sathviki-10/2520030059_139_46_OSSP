#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#define MAX_PROCESSES 50
#define MAX_FILENAME 256
#define BUFFER_SIZE 4096

typedef enum {
    READY,
    RUNNING,
    WAITING,
    TERMINATED
} ProcessState;

typedef struct {
    int number;
    pid_t pid;
    pid_t parent_pid;
    char filename[MAX_FILENAME];

    int pipe_fd[2];

    ProcessState state;

    int burst_time;
    int completion_time;
    int turnaround_time;
    int waiting_time;

    size_t memory_used;
} Process;

Process processes[MAX_PROCESSES];

int process_count = 0;
int execution_started = 0;

volatile sig_atomic_t start_execution = 0;
volatile sig_atomic_t terminate_process = 0;

pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

char thread_data[BUFFER_SIZE];

void sigusr1_handler(int signal_number)
{
    if (signal_number == SIGUSR1) {
        start_execution = 1;
    }
}

void sigterm_handler(int signal_number)
{
    if (signal_number == SIGTERM) {
        terminate_process = 1;
    }
}

const char *state_name(ProcessState state)
{
    switch (state) {
        case READY:
            return "READY";

        case RUNNING:
            return "RUNNING";

        case WAITING:
            return "WAITING";

        case TERMINATED:
            return "TERMINATED";

        default:
            return "UNKNOWN";
    }
}

void install_signal_handlers()
{
    struct sigaction sa1;
    struct sigaction sa2;

    memset(&sa1, 0, sizeof(sa1));
    memset(&sa2, 0, sizeof(sa2));

    sa1.sa_handler = sigusr1_handler;
    sa2.sa_handler = sigterm_handler;

    sigemptyset(&sa1.sa_mask);
    sigemptyset(&sa2.sa_mask);

    sa1.sa_flags = 0;
    sa2.sa_flags = 0;

    sigaction(SIGUSR1, &sa1, NULL);
    sigaction(SIGTERM, &sa2, NULL);
}

void initialize_system()
{
    printf("\n");
    printf("===============================================\n");
    printf(" Linux Process Creation, Execution and Termination\n");
    printf("===============================================\n");

    install_signal_handlers();
}

void wait_for_execution_signal()
{
    while (!start_execution && !terminate_process) {
        pause();
    }
}

void process_file(int process_number, const char *filename, int write_fd)
{
    int fd;
    struct stat file_info;

    char *buffer;
    ssize_t bytes_read;

    fd = open(filename, O_RDWR);

    if (fd == -1) {
        char message[256];

        snprintf(
            message,
            sizeof(message),
            "P%d could not open file %s\n",
            process_number,
            filename
        );

        write(write_fd, message, strlen(message));

        return;
    }

    if (fstat(fd, &file_info) == -1) {
        close(fd);
        return;
    }

    buffer = malloc(BUFFER_SIZE);

    if (buffer == NULL) {
        close(fd);
        return;
    }

    memset(buffer, 0, BUFFER_SIZE);

    bytes_read = read(
        fd,
        buffer,
        BUFFER_SIZE - 1
    );

    if (bytes_read < 0) {
        free(buffer);
        close(fd);
        return;
    }

    buffer[bytes_read] = '\0';

    pthread_mutex_lock(&file_mutex);

    strncpy(
        thread_data,
        buffer,
        BUFFER_SIZE - 1
    );

    thread_data[BUFFER_SIZE - 1] = '\0';

    pthread_mutex_unlock(&file_mutex);

    lseek(fd, 0, SEEK_END);

    const char *text =
        "\n[Processed by Linux Process Management System]";

    write(
        fd,
        text,
        strlen(text)
    );

    char message[512];

    snprintf(
        message,
        sizeof(message),
        "P%d completed file processing: %s | PID: %d | Bytes read: %ld | Inode: %lu\n",
        process_number,
        filename,
        getpid(),
        (long)bytes_read,
        (unsigned long)file_info.st_ino
    );

    write(
        write_fd,
        message,
        strlen(message)
    );

    free(buffer);

    close(fd);
}

void *thread_function(void *arg)
{
    char *data = (char *)arg;

    pthread_mutex_lock(&file_mutex);

    size_t length = strlen(data);

    if (length > 0) {
        thread_data[0] = data[0];
    }

    pthread_mutex_unlock(&file_mutex);

    return NULL;
}

void child_process(
    int process_number,
    const char *filename,
    int write_fd
)
{
    pthread_t worker;

    start_execution = 0;
    terminate_process = 0;

    install_signal_handlers();

    wait_for_execution_signal();

    if (terminate_process) {
        close(write_fd);
        _exit(0);
    }

    pthread_create(
        &worker,
        NULL,
        thread_function,
        (void *)"process"
    );

    pthread_join(
        worker,
        NULL
    );

    process_file(
        process_number,
        filename,
        write_fd
    );

    close(write_fd);

    _exit(0);
}

void create_process()
{
    char filename[MAX_FILENAME];

    if (process_count >= MAX_PROCESSES) {
        printf("\nMaximum process limit reached.\n");
        return;
    }

    printf(
        "\nEnter filename for P%d: ",
        process_count + 1
    );

    if (fgets(
            filename,
            sizeof(filename),
            stdin
        ) == NULL) {
        return;
    }

    filename[
        strcspn(filename, "\n")
    ] = '\0';

    if (strlen(filename) == 0) {
        printf("Filename cannot be empty.\n");
        return;
    }

    int fd = open(
        filename,
        O_CREAT | O_RDWR,
        0644
    );

    if (fd == -1) {
        perror("File creation failed");
        return;
    }

    close(fd);

    int pipe_fd[2];

    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");

        close(pipe_fd[0]);
        close(pipe_fd[1]);

        return;
    }

    if (pid == 0) {

        close(pipe_fd[0]);

        child_process(
            process_count + 1,
            filename,
            pipe_fd[1]
        );
    }

    close(pipe_fd[1]);

    processes[process_count].number =
        process_count + 1;

    processes[process_count].pid =
        pid;

    processes[process_count].parent_pid =
        getpid();

    strcpy(
        processes[process_count].filename,
        filename
    );

    processes[process_count].pipe_fd[0] =
        pipe_fd[0];

    processes[process_count].pipe_fd[1] =
        -1;

    processes[process_count].state =
        READY;

    processes[process_count].burst_time =
        1 + (strlen(filename) % 5);

    processes[process_count].completion_time =
        0;

    processes[process_count].turnaround_time =
        0;

    processes[process_count].waiting_time =
        0;

    processes[process_count].memory_used =
        0;

    printf("\n");
    printf("-----------------------------------------\n");
    printf("Process created successfully\n");
    printf("-----------------------------------------\n");

    printf(
        "Process Number : P%d\n",
        processes[process_count].number
    );

    printf(
        "PID            : %d\n",
        processes[process_count].pid
    );

    printf(
        "Parent PID     : %d\n",
        processes[process_count].parent_pid
    );

    printf(
        "File           : %s\n",
        processes[process_count].filename
    );

    printf(
        "State          : READY\n"
    );

    printf("-----------------------------------------\n");

    process_count++;
}

void show_status()
{
    int i;

    if (process_count == 0) {
        printf("\nNo processes have been created.\n");
        return;
    }

    printf("\n");
    printf("=================================================================\n");
    printf("PROCESS STATUS\n");
    printf("=================================================================\n");

    printf(
        "%-5s %-10s %-10s %-25s %-10s\n",
        "P",
        "PID",
        "PPID",
        "FILE",
        "STATE"
    );

    printf(
        "-----------------------------------------------------------------\n"
    );

    for (i = 0; i < process_count; i++) {

        printf(
            "%-5d %-10d %-10d %-25s %-10s\n",
            processes[i].number,
            processes[i].pid,
            processes[i].parent_pid,
            processes[i].filename,
            state_name(processes[i].state)
        );
    }

    printf(
        "=================================================================\n"
    );
}

void execute_process(int index, int current_time)
{
    Process *p = &processes[index];

    int burst =
        p->burst_time;

    p->state =
        RUNNING;

    printf("\n");
    printf("-----------------------------------------\n");

    printf(
        "FCFS Scheduler\n"
    );

    printf(
        "P%d executing\n",
        p->number
    );

    printf(
        "PID      : %d\n",
        p->pid
    );

    printf(
        "File     : %s\n",
        p->filename
    );

    printf(
        "Time     : %d - %d\n",
        current_time,
        current_time + burst
    );

    printf("-----------------------------------------\n");

    kill(
        p->pid,
        SIGUSR1
    );

    char message[512];

    ssize_t bytes =
        read(
            p->pipe_fd[0],
            message,
            sizeof(message) - 1
        );

    if (bytes > 0) {

        message[bytes] =
            '\0';

        printf(
            "%s",
            message
        );
    }

    close(
        p->pipe_fd[0]
    );

    waitpid(
        p->pid,
        NULL,
        0
    );

    p->completion_time =
        current_time + burst;

    p->turnaround_time =
        p->completion_time;

    p->waiting_time =
        p->turnaround_time -
        p->burst_time;

    p->memory_used =
        BUFFER_SIZE +
        sizeof(pthread_t);

    p->state =
        TERMINATED;

    printf(
        "P%d completed after execution.\n",
        p->number
    );
}

void execute_processes()
{
    int i;

    int current_time = 0;

    double total_turnaround = 0;
    double total_waiting = 0;

    if (process_count == 0) {
        printf(
            "\nNo processes have been created.\n"
        );
        return;
    }

    if (execution_started) {
        printf(
            "\nExecution has already been completed.\n"
        );
        return;
    }

    execution_started = 1;

    printf("\n");
    printf("===============================================\n");
    printf(" FCFS PROCESS EXECUTION\n");
    printf("===============================================\n");

    printf(
        "\nExecution order is based on process creation order.\n"
    );

    for (i = 0; i < process_count; i++) {

        execute_process(
            i,
            current_time
        );

        current_time +=
            processes[i].burst_time;
    }

    printf("\n");
    printf("===============================================\n");
    printf(" EXECUTION RESULTS\n");
    printf("===============================================\n");

    printf(
        "%-5s %-10s %-10s %-10s %-10s\n",
        "P",
        "CT",
        "TAT",
        "WT",
        "STATE"
    );

    printf(
        "-----------------------------------------------\n"
    );

    for (i = 0; i < process_count; i++) {

        printf(
            "%-5d %-10d %-10d %-10d %-10s\n",
            processes[i].number,
            processes[i].completion_time,
            processes[i].turnaround_time,
            processes[i].waiting_time,
            state_name(processes[i].state)
        );

        total_turnaround +=
            processes[i].turnaround_time;

        total_waiting +=
            processes[i].waiting_time;
    }

    printf(
        "-----------------------------------------------\n"
    );

    printf(
        "Average Turnaround Time : %.2f\n",
        total_turnaround /
        process_count
    );

    printf(
        "Average Waiting Time    : %.2f\n",
        total_waiting /
        process_count
    );
}

void show_memory()
{
    int i;

    if (process_count == 0) {
        printf(
            "\nNo processes have been created.\n"
        );
        return;
    }

    printf("\n");
    printf("===============================================\n");
    printf(" PROCESS MEMORY INFORMATION\n");
    printf("===============================================\n");

    for (i = 0; i < process_count; i++) {

        char path[100];

        snprintf(
            path,
            sizeof(path),
            "/proc/%d/status",
            processes[i].pid
        );

        FILE *fp =
            fopen(path, "r");

        printf(
            "\nP%d | PID: %d\n",
            processes[i].number,
            processes[i].pid
        );

        if (fp == NULL) {

            printf(
                "Process is completed.\n"
            );

            printf(
                "Project allocated memory: %zu bytes\n",
                processes[i].memory_used
            );

            continue;
        }

        char line[256];

        while (fgets(
            line,
            sizeof(line),
            fp
        )) {

            if (
                strncmp(
                    line,
                    "VmSize:",
                    7
                ) == 0
                ||
                strncmp(
                    line,
                    "VmRSS:",
                    6
                ) == 0
            ) {

                printf(
                    "%s",
                    line
                );
            }
        }

        fclose(fp);

        printf(
            "Project allocated memory: %zu bytes\n",
            processes[i].memory_used
        );
    }

    printf("\n");
    printf("Memory concepts used:\n");
    printf("malloc()\n");
    printf("free()\n");
    printf("Process address space\n");
    printf("Linux /proc memory information\n");
    printf("Copy-on-Write through fork()\n");
}

void show_files()
{
    int i;

    if (process_count == 0) {
        printf(
            "\nNo processes have been created.\n"
        );
        return;
    }

    printf("\n");
    printf("===============================================\n");
    printf(" PROCESS FILE INFORMATION\n");
    printf("===============================================\n");

    for (i = 0; i < process_count; i++) {

        struct stat file_stat;

        printf(
            "\nP%d\n",
            processes[i].number
        );

        printf(
            "PID      : %d\n",
            processes[i].pid
        );

        printf(
            "Filename : %s\n",
            processes[i].filename
        );

        if (
            stat(
                processes[i].filename,
                &file_stat
            ) == -1
        ) {

            printf(
                "File information unavailable.\n"
            );

            continue;
        }

        printf(
            "Size     : %ld bytes\n",
            (long)file_stat.st_size
        );

        printf(
            "Inode    : %lu\n",
            (unsigned long)file_stat.st_ino
        );

        printf(
            "Mode     : %o\n",
            file_stat.st_mode & 0777
        );
    }

    printf("\n");
    printf("File system operations used:\n");
    printf("open()\n");
    printf("read()\n");
    printf("write()\n");
    printf("close()\n");
    printf("stat()\n");

    printf("\n");
    printf(
        "Each process is associated with exactly one file.\n"
    );
}

void terminate_processes()
{
    int i;

    if (process_count == 0) {
        printf(
            "\nNo processes have been created.\n"
        );
        return;
    }

    printf("\n");
    printf("===============================================\n");
    printf(" PROCESS TERMINATION\n");
    printf("===============================================\n");

    for (i = 0; i < process_count; i++) {

        if (
            processes[i].state ==
            TERMINATED
        ) {

            printf(
                "P%d already completed.\n",
                processes[i].number
            );

            continue;
        }

        kill(
            processes[i].pid,
            SIGTERM
        );

        waitpid(
            processes[i].pid,
            NULL,
            0
        );

        close(
            processes[i].pipe_fd[0]
        );

        processes[i].state =
            TERMINATED;

        printf(
            "P%d with PID %d terminated.\n",
            processes[i].number,
            processes[i].pid
        );
    }

    printf(
        "\nAll processes terminated successfully.\n"
    );
}

void show_help()
{
    printf("\n");
    printf("Available commands\n");
    printf("--------------------------------------\n");

    printf(
        "create      Create one process and one file\n"
    );

    printf(
        "status      Display process information\n"
    );

    printf(
        "execute     Execute processes using FCFS\n"
    );

    printf(
        "memory      Display memory information\n"
    );

    printf(
        "files       Display file and inode information\n"
    );

    printf(
        "terminate   Terminate remaining processes\n"
    );

    printf(
        "help        Display commands\n"
    );

    printf(
        "exit        Exit the program\n"
    );
}

void cleanup()
{
    int i;

    for (i = 0; i < process_count; i++) {

        if (
            processes[i].state !=
            TERMINATED
        ) {

            kill(
                processes[i].pid,
                SIGTERM
            );

            waitpid(
                processes[i].pid,
                NULL,
                0
            );
        }
    }

    pthread_mutex_destroy(
        &file_mutex
    );
}

int main()
{
    char command[100];

    initialize_system();

    printf(
        "\nType 'help' to see available commands.\n"
    );

    while (1) {

        printf(
            "\nos> "
        );

        fflush(stdout);

        if (
            fgets(
                command,
                sizeof(command),
                stdin
            ) == NULL
        ) {

            break;
        }

        command[
            strcspn(
                command,
                "\n"
            )
        ] = '\0';

        if (
            strcmp(
                command,
                "create"
            ) == 0
        ) {

            create_process();

        } else if (
            strcmp(
                command,
                "status"
            ) == 0
        ) {

            show_status();

        } else if (
            strcmp(
                command,
                "execute"
            ) == 0
        ) {

            execute_processes();

        } else if (
            strcmp(
                command,
                "memory"
            ) == 0
        ) {

            show_memory();

        } else if (
            strcmp(
                command,
                "files"
            ) == 0
        ) {

            show_files();

        } else if (
            strcmp(
                command,
                "terminate"
            ) == 0
        ) {

            terminate_processes();

        } else if (
            strcmp(
                command,
                "help"
            ) == 0
        ) {

            show_help();

        } else if (
            strcmp(
                command,
                "exit"
            ) == 0
        ) {

            cleanup();

            printf(
                "\nLinux Process Manager terminated.\n"
            );

            break;

        } else if (
            strlen(command) == 0
        ) {

            continue;

        } else {

            printf(
                "\nUnknown command: %s\n",
                command
            );

            printf(
                "Type 'help' for available commands.\n"
            );
        }
    }

    return 0;
}
