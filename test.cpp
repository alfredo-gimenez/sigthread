#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <pthread.h>
#include <omp.h>

#include <vector>

DIR *proc_pid_task;
struct dirent *ent;

std::vector<int> get_thread_list()
{
    std::vector<int> thread_list;

    rewinddir(proc_pid_task);
    while((ent = readdir(proc_pid_task)) != NULL)
    {
        if(ent->d_name[0] != '.')
        {
            thread_list.push_back(atoi(ent->d_name));
        }
    }
    return thread_list;
}

void* sampler(void *args)
{
    // Send a SIGUSR1 to the current process
    // once every second 
    while(1)
    {
        sleep(1);
        kill(getpid(),SIGUSR1);
    }
}

void sig_handle_thread(int sig)
{
    int mtid = syscall(SYS_gettid);
    printf("Thread %d received THREAD signal\n", mtid);
}

void sig_handle_broadcast(int sig)
{
    int mtid = syscall(SYS_gettid);

    printf("Thread %d received BROADCAST signal\n", mtid);

    // Master thread broadcasts to all other threads
    std::vector<int> thread_list = get_thread_list();
    for(int i=0; i<thread_list.size(); i++)
    {
        int tid = thread_list[i];

        // Send signal to thread using tgkill
        syscall(SYS_tgkill, getpid(), tid, SIGUSR2);
    }
}

void init_signal_handlers()
{
    // Register broadcast signal handler
    struct sigaction sact1;
    memset(&sact1, 0, sizeof(sact1));
    sact1.sa_handler = &sig_handle_broadcast;
    sigaction(SIGUSR1, &sact1, NULL);

    // Register thread signal handler
    struct sigaction sact2;
    memset(&sact2, 0, sizeof(sact2));
    sact2.sa_handler = &sig_handle_thread;
    sigaction(SIGUSR2, &sact2, NULL);
}

int main()
{
    // Open task directory (for threads)
    proc_pid_task = opendir("/proc/self/task");
    if(proc_pid_task == NULL)
    {
        printf("Unable to open pid tasks!\n");
        return 1;
    }
    get_thread_list();

    // Initialize signal handlers
    init_signal_handlers();

    // Create sampler thread
    pthread_t sampler_thread;
    pthread_create(&sampler_thread, NULL, &sampler, NULL);

    int n=4;
    int i,j,x;
#pragma omp parallel for private(i,j,x) 
    for(i=0; i<n; i++)
    {
        // Do meaningless work
        x = rand();
        for(j=0; j<10; j++)
        {
            x = x * i * j;
            j--;
        }

        // Validate meaninglessness
        // (so work doesn't get compiled out)
        printf("%d\n",x);
    }

    return 0;
}
