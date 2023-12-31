#include "headers.h"
#include <stdio.h>
#define msgq_key 65
// #define arrivals_shm_key 88

int msgq_id;

void clearResources(int);

int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);

    // Create and initialize the message queue

    key_t key_id;
    key_id = ftok("msgQueueFileKey", msgq_key);
    if (key_id == -1)
    {
        perror("Error in creating the key\n");
        exit(-1);
    }
    int send_val;
    msgq_id = msgget(key_id, 0666 | IPC_CREAT);
    if (msgq_id == -1)
    {
        perror("Error in creating the message queue\n");
        exit(-1);
    }

    // 1. Read the input files.

    // open the file that contains the processes data
    FILE *file = fopen("processes.txt", "r");

    if (file == NULL)
    {
        printf("Error! File not found.\n");
        exit(-1);
    }

    // get the number of p//rocesses from the document by counting the lines
    int processes_count = 0;
    char ch;
    while (!feof(file))
    {
        ch = fgetc(file);
        if (ch == '\n')
            processes_count++;
    }
    fclose(file);

    // to exclude the first line that contains the headers
    processes_count--;
    printf("Number of processes: %d\n", processes_count);

    // create an array of processes with the size of the number of processes
    Process processes[processes_count];

    // reopen the file to read the processes data from the document and store them in the array
    file = fopen("processes.txt", "r");

    // skip the first line that contains the headers
    fscanf(file, "%*[^\n]");

    // read the processes data from the document and store them in the array
    for (int i = 0; i < processes_count; i++)
    {
        // here t%d will make fscanf ignore the space and read the number only
        fscanf(file, "%d\t%d\t%d\t%d\t%d", &processes[i].id, &processes[i].arrival_time, &processes[i].running_time, &processes[i].priority, &processes[i].memsize);

        if (processes[i].arrival_time > 1000)
        {
            printf("Error! Arrival time of process %d is greater than 1000, arrival sahred mem is overloaded\n", processes[i].id);
            exit(-1);
        }
    }
    fclose(file);

    printf("processes data is read successfully\n");
    for (int i = 0; i < processes_count; i++)
        printf("Process %d: id : %d arrival: %d runtime : %d priority : %d memsize : %d\n", i + 1, processes[i].id, processes[i].arrival_time, processes[i].running_time, processes[i].priority, processes[i].memsize);

    // 2. Ask the user for the chosen sc1heduling algorithm and its parameters, if there are any.

    int algorithm_num;
    printf("-----------------PROCESS_GENERATOR-----------------\n");
    printf("Choose a scheduling algorithm from the following:\n");
    printf("1. Non-preemptive Highest Priority First (HPF).\n");
    printf("2. Shortest Remaining time Next (SRTN).\n");
    printf("3. Round Robin (RR).\n");
    scanf("%d", &algorithm_num);
    while (algorithm_num < 1 || algorithm_num > 3)
    {
        printf("Please enter a valid algorithm number: ");
        scanf("%d", &algorithm_num);
    }
    printf("---------------------------------------------------\n");

    int quantum;
    if (algorithm_num == 3)
    {
        printf("Enter the quantum for the Round Robin algorithm: ");
        scanf("%d", &quantum);
    }

    // 3. Initiate and create the scheduler and clock processes.

    // Create the scheduler process
    pid_t scheduler_pid = fork();
    if (scheduler_pid == -1)
    {
        perror("Error in creating scheduler process\n");
        exit(-1);
    }
    else if (scheduler_pid == 0) // This is the scheduler process
    {
        // compile the C program named scheduler.c using the gcc compiler
        // The compiled output is named scheduler.out.

        printf("Scheduling..\n");

        // sprintf is used to convert the integer arguments to strings to be passed to the execl function
        char process_count_str[10], algorithm_num_str[2], quantum_str[10];
        sprintf(process_count_str, "%d", processes_count);
        sprintf(algorithm_num_str, "%d", algorithm_num);

        // execl is used to execute the scheduler.out file with the arguments passed to it
        // here the NULL in the execl function serves as a sentinel value to indicate the end of the argument list.
        if (algorithm_num != 3)
        {
            int success = execl("./scheduler.out", process_count_str, algorithm_num_str, NULL);
            if (success == -1)
            {
                printf("Error in executing scheduler.out with !RR\n");
                exit(-1);
            }
        }
        else
        {
            sprintf(quantum_str, "%d", quantum);
            int succes = execl("./scheduler.out", process_count_str, algorithm_num_str, quantum_str, NULL);
            if (succes == -1)
            {
                printf("Error in executing scheduler.out with RR\n");
                exit(-1);
            }
        }
        exit(0);
    }

    // Create the clock process
    pid_t clock_pid = fork();
    if (clock_pid == -1)
    {
        perror("Error in creating scheduler process\n");
        exit(-1);
    }
    else if (clock_pid == 0) // This is the clock process
    {
        int success = execl("./clk.out", "clk", NULL);
        if (success == -1)
        {
            printf("Error in executing clk.out\n");
            exit(-1);
        }
        exit(0);
    }

    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    printf("generator clk start : %d \n", getClk());

    // 5. Create a data structure for processes and provide it with its parameters.
    // --> already done in the beginning of the main function

    // 6. Send the information to the scheduler at the appropriate time.

    // qsort(processes, processes_count, sizeof(Process), comp);

    int index = 0;

    // send the processes to the scheduler at their arrival time
    while (index < processes_count)
    {
        if (processes[index].arrival_time == getClk())
        {

            // Create a message and send it to the message queue
            Msgbuff message;
            message.mtype = 1;
            message.proc = processes[index];
            int send_val = msgsnd(msgq_id, &message, sizeof(message.proc), !IPC_NOWAIT);
            if (send_val == -1)
            {
                perror("Error in sending the message\n");
                exit(-1);
            }
            printf("Process %d sent to the scheduler at time %d  and indx %d \n", processes[index].id, getClk(), index);
            index++;
        }
    }

    // wait for the scheduler to finish
    waitpid(scheduler_pid, NULL, 0);

    // 7. Clear clock resources
    // destroyClk(true);
}

void clearResources(int signum)
{
    // Clears all resources in case of interruption
    printf("Clearing due to interruption\n");
    destroyClk(true);
    msgctl(msgq_id, IPC_RMID, (struct msqid_ds *)0);
    exit(0);
    raise(SIGKILL);
}