#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#define NUM_CHILDREN 5
#define LOG_FILE "/tmp/daemon_zombie_log.txt"

void log_message(const char *message) {
    time_t now;
    time(&now);
    char *timestamp = ctime(&now);
    timestamp[strlen(timestamp)-1] = '\0'; // Remove newline
    
    FILE *log_file = fopen(LOG_FILE, "a");
    if (log_file) {
        fprintf(log_file, "[%s] %s\n", timestamp, message);
        fclose(log_file);
    }
    
    // Also print to terminal if running in foreground
    printf("[%s] %s\n", timestamp, message);
}

void daemonize() {
    pid_t pid = fork();
    
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    
    if (pid > 0) {
        exit(EXIT_SUCCESS); // Exit parent process
    }
    
    // Child becomes session leader
    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }
    
    // Change working directory
    chdir("/");
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    // Ignore signals
    signal(SIGCHLD, SIG_DFL);
    signal(SIGHUP, SIG_IGN);
}

void create_child_processes() {
    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();
        
        if (pid < 0) {
            log_message("Failed to fork child process");
            continue;
        }
        
        if (pid == 0) {
            // Child process
            char sleep_time = 2 + (i % 4); // Sleep between 2-5 seconds
            char msg[100];
            snprintf(msg, sizeof(msg), "Child %d (PID: %d) sleeping for %d seconds", 
                     i+1, getpid(), sleep_time);
            log_message(msg);
            
            sleep(sleep_time);
            
            int exit_code = 10 + i;
            snprintf(msg, sizeof(msg), "Child %d (PID: %d) exiting with code %d", 
                     i+1, getpid(), exit_code);
            log_message(msg);
            
            exit(exit_code);
        } else {
            // Parent (daemon) process
            char msg[100];
            snprintf(msg, sizeof(msg), "Created child %d with PID: %d", i+1, pid);
            log_message(msg);
        }
    }
}

void check_zombies() {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        char msg[200];
        
        if (WIFEXITED(status)) {
            snprintf(msg, sizeof(msg), "Cleaned up zombie process PID: %d, Exit status: %d", 
                     pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            snprintf(msg, sizeof(msg), "Cleaned up zombie process PID: %d, Terminated by signal: %d", 
                     pid, WTERMSIG(status));
        } else {
            snprintf(msg, sizeof(msg), "Cleaned up zombie process PID: %d, Unknown status", pid);
        }
        
        log_message(msg);
    }
}

int main() {
    log_message("Daemon starting...");
    
    // Daemonize the process
    daemonize();
    
    log_message("Daemon initialized");
    
    // Create child processes
    create_child_processes();
    
    // Main daemon loop
    while (1) {
        check_zombies();
        sleep(1); // Check every 1 second
    }
    
    return EXIT_SUCCESS;
}
