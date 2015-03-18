#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>

#include  <sys/types.h>
#include  <sys/syscall.h>
#include  <signal.h>
#include  <unistd.h>

#include <pthread.h>
#include <omp.h>

#include <algorithm>
#include <vector>

std::vector<int> tidlist;

void ompt_init()
{
    // Add current thread to threadlist
    int mpid = omp_get_thread_num();
    int mtid = syscall(SYS_gettid);
    tidlist[mpid] = mtid;
}

void ompt_destroy()
{
    // Remove current thread from threadlist
    int mpid = omp_get_thread_num();
    tidlist[mpid] = -1;
}

void sig_handle_broadcast(int sig)
{
    int mtid = syscall(SYS_gettid);

    printf("Thread %d received BROADCAST signal\n", mtid);

    // Master thread broadcasts to all other threads
    for(int i=0; i<tidlist.size(); i++)
    {
        int tid = tidlist.at(i);

        // No thread here
        if(tid == -1)
            continue;

        // Send signal to thread using tgkill
        syscall(SYS_tgkill, getpid(), tid, SIGUSR2);
    }
}

void sig_handle_thread(int sig)
{
    int mtid = syscall(SYS_gettid);

    printf("Thread %d received THREAD signal\n", mtid);
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


int main()
{
    int n=4;

    tidlist.resize(omp_get_max_threads(),-1);

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

    // Create sampler thread
    pthread_t sampler_thread;
    pthread_create(&sampler_thread, NULL, &sampler, NULL);

    int i,j,x;
#pragma omp parallel for private(i,j,x) shared(tidlist)
    for(i=0; i<n; i++)
    {
        ompt_init();

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

        ompt_destroy();
    }

    return 0;
}
