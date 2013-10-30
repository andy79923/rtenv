#include "command.h"
#include "task.h"

void ps(char splitInput[][20], int splitNum)
{
    int fdout = mq_open("/tmp/mqueue/out", 0);
    const struct task_control_block tasks[TASK_LIMIT];
    int taskCount,i;
    getProcessInfo(tasks,&taskCount);
    char str[10];
    write(fdout, "Pid\tStatus\t\tPriority\n\r", 24);
    for(i = 0; i < taskCount; i++)
    {
        itoa(tasks[i].pid, str);
        write(fdout, str, strlen(str)+1);
        write(fdout, "\t", 2);
        switch(tasks[i].status)
        {
            case TASK_READY:
                write(fdout, "TASK READY\t", 12);
                break;
            case TASK_WAIT_READ:
                write(fdout, "TASK WAIT READ\t", 16);
                break;
            case TASK_WAIT_WRITE:
                write(fdout, "TASK WAIT WRITE\t", 17);
                break;
            case TASK_WAIT_INTR:
                write(fdout, "TASK WAIT INTR\t", 16);
                break;
            case TASK_WAIT_TIME:
                write(fdout, "TASK WAIT TIME\t", 16);
                break;
            default:
                break;
        }
        itoa(tasks[i].priority, str);
        write(fdout, str, strlen(str)+1);
        write(fdout, "\n\r",3);
    }
}

void echo(char splitInput[][20], int splitNum)
{
    int fdout = mq_open("/tmp/mqueue/out", 0);
    int i=1;
    while(splitInput[i][0] == '-' && i < splitNum)i++;
    for(; i < splitNum; i++)
    {
        write(fdout, splitInput[i], strlen(splitInput[i])+1);
        write(fdout, " ", 2);
    }
    write(fdout, "\n", 2);
}

void hello(char splitInput[][20], int splitNum)
{
    int fdout = mq_open("/tmp/mqueue/out", 0);
    char *str = "Hello, World!\n";
    write(fdout, str, strlen(str)+1);
}
