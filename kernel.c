#include "stm32f10x.h"
#include "RTOSConfig.h"

#include "syscall.h"
#include "command.h"
#include "string.h"
#include "task.h"
#include "fio.h"

#include <stddef.h>

/*
 * pathserver assumes that all files are FIFOs that were registered
 * with mkfifo.  It also assumes a global tables of FDs shared by all
 * processes.  It would have to get much smarter to be generally useful.
 *
 * The first TASK_LIMIT FDs are reserved for use by their respective tasks.
 * 0-2 are reserved FDs and are skipped.
 * The server registers itself at /sys/pathserver
*/
#define PATH_SERVER_NAME "/sys/pathserver"
void pathserver()
{
	char paths[PIPE_LIMIT - TASK_LIMIT - 3][PATH_MAX];
	int npaths = 0;
	int i = 0;
	unsigned int plen = 0;
	unsigned int replyfd = 0;
	char path[PATH_MAX];

	memcpy(paths[npaths++], PATH_SERVER_NAME, sizeof(PATH_SERVER_NAME));

	while (1) {
		read(PATHSERVER_FD, &replyfd, 4);
		read(PATHSERVER_FD, &plen, 4);
		read(PATHSERVER_FD, path, plen);

		if (!replyfd) { /* mkfifo */
			int dev;
			read(PATHSERVER_FD, &dev, 4);
			memcpy(paths[npaths], path, plen);
			mknod(npaths + 3 + TASK_LIMIT, 0, dev);
			npaths++;
		}
		else { /* open */
			/* Search for path */
			for (i = 0; i < npaths; i++) {
				if (*paths[i] && strcmp(path, paths[i]) == 0) {
					i += 3; /* 0-2 are reserved */
					i += TASK_LIMIT; /* FDs reserved for tasks */
					write(replyfd, &i, 4);
					i = 0;
					break;
				}
			}

			if (i >= npaths) {
				i = -1; /* Error: not found */
				write(replyfd, &i, 4);
			}
		}
	}
}

void serialout(USART_TypeDef* uart, unsigned int intr)
{
	int fd;
	char c;
	int doread = 1;
	mkfifo("/dev/tty0/out", 0);
	fd = open("/dev/tty0/out", 0);

	while (1) {
		if (doread)
			read(fd, &c, 1);
		doread = 0;
		if (USART_GetFlagStatus(uart, USART_FLAG_TXE) == SET) {
			USART_SendData(uart, c);
			USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
			doread = 1;
		}
		interrupt_wait(intr);
		USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
	}
}

void serialin(USART_TypeDef* uart, unsigned int intr)
{
	int fd;
	char c;
	mkfifo("/dev/tty0/in", 0);
	fd = open("/dev/tty0/in", 0);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

	while (1) {
		interrupt_wait(intr);
		if (USART_GetFlagStatus(uart, USART_FLAG_RXNE) == SET) {
			c = USART_ReceiveData(uart);
			write(fd, &c, 1);
		}
	}
}
void rs232_xmit_msg_task()
{
	int fdout, fdin;
	char str[100];
	int curr_char;
	fdout = open("/dev/tty0/out", 0);
	fdin = mq_open("/tmp/mqueue/out", O_CREAT);
	setpriority(0, PRIORITY_DEFAULT - 2);

	while (1) {
		/* Read from the queue.  Keep trying until a message is
		 * received.  This will block for a period of time (specified
		 * by portMAX_DELAY). */
		read(fdin, str, 100);

		/* Write each character of the message to the RS232 port. */
		curr_char = 0;
		while (str[curr_char] != '\0') {
			write(fdout, &str[curr_char], 1);
			curr_char++;
		}
	}
}

//Handle input string
void HandleInput(char* input)
{
    int i, j;
    char splitStr[50][20];/*splitStr[0]:command
                            splitStr[1]~splitStr[k]:options
                            splitStr[k+1]~splitStr[n]:arguments
                          */
    int splitNum = 0;
    int cmdNO;
    int fdout = mq_open("/tmp/mqueue/out", 0);
    for(i=0, j=0; input[i]!='\0' && j < 20; i++)
    {
        if(input[i] == ' ')
        {
            if(j != 0)
            {
                splitStr[splitNum++][j] = '\0';
                j = 0;
            }
            continue;
        }
        splitStr[splitNum][j] = input[i];
        j++;
    }
    splitStr[splitNum++][j] = '\0';

    int cmd_num = sizeof(cmd)/sizeof(cmd_type);
    for(i = 0; i < cmd_num; i++)
    {
        if(strcmp(splitStr[0], cmd[i].name) == 0)
        {
            cmd[i].handler(splitStr, splitNum);
            return;
        }
    }
    write(fdout, splitStr[0], strlen(splitStr[0])+1);
    write(fdout, ": command not found\n", 21);
}

void Shell()
{
	int fdout, fdin;
	char str[100];
	char* ch[]={'0','\0'};

	int curr_char;
	int done;
	char pos[] = "\rrtenv:~$ ";
	char newLine[] = "\n\r";

    fdout = mq_open("/tmp/mqueue/out", 0);
	fdin = open("/dev/tty0/in", 0);

	while (1)
    {
        write(fdout, pos, strlen(pos)+1);
		curr_char = 0;
		done = 0;
		str[curr_char] = '\0';
		do
        {
			/* Receive a byte from the RS232 port (this call will
			 * block). */
			read(fdin, ch, 1);

			/* If the byte is an end-of-line type character, then
			 * finish the string and inidcate we are done.
			 */
			if (curr_char >= 98 || (ch[0] == '\r') || (ch[0] == '\n'))
            {
				str[curr_char] = '\0';
				done = -1;
				/* Otherwise, add the character to the
				 * response string. */
			}
			else if(ch[0] == 127)//press the backspace key
            {
                if(curr_char!=0)
                {
                    curr_char--;
                    write(fdout, "\b \b", 4);
                }
            }
            else if(ch[0] == 27)//press up, down, left, right, home, page up, delete, end, page down
            {
                read(fdin, ch, 1);
                if(ch[0] != '[')
                {
                    str[curr_char++] = ch;
                    write(fdout, ch, 2);
                }
                else
                {
                    read(fdin, ch[0], 1);
                    if(ch[0] >= '1' && ch[0] <= '6')
                    {
                        read(fdin, ch, 1);
                    }
                }
            }
			else
            {
				str[curr_char++] = ch[0];
				write(fdout, ch, 2);
			}
		} while (!done);
        write(fdout, newLine, strlen(newLine)+1);
        if(strlen(str)>0)
        {
            HandleInput(str);
        }
	}
}

void first()
{
	setpriority(0, 0);

	if (!fork()) setpriority(0, 0), pathserver();
	if (!fork()) setpriority(0, 0), serialout(USART2, USART2_IRQn);
	if (!fork()) setpriority(0, 0), serialin(USART2, USART2_IRQn);
	if (!fork()) rs232_xmit_msg_task();
	if (!fork()) setpriority(0, PRIORITY_DEFAULT - 10), Shell();

	setpriority(0, PRIORITY_LIMIT);

	while(1);
}

int main()
{
	unsigned int stacks[TASK_LIMIT][STACK_SIZE];
	struct task_control_block tasks[TASK_LIMIT];
	struct pipe_ringbuffer pipes[PIPE_LIMIT];
	struct task_control_block *ready_list[PRIORITY_LIMIT + 1];  /* [0 ... 39] */
	struct task_control_block *wait_list = NULL;
	size_t task_count = 0;
	size_t current_task = 0;
	size_t i;
	struct task_control_block *task;
	int timeup;
	unsigned int tick_count = 0;

	SysTick_Config(configCPU_CLOCK_HZ / configTICK_RATE_HZ);

	init_rs232();
	__enable_irq();

	tasks[task_count].stack = (void*)init_task(stacks[task_count], &first);
	tasks[task_count].pid = 0;
	tasks[task_count].priority = PRIORITY_DEFAULT;
	task_count++;

	/* Initialize all pipes */
	for (i = 0; i < PIPE_LIMIT; i++)
		pipes[i].start = pipes[i].end = 0;

	/* Initialize fifos */
	for (i = 0; i <= PATHSERVER_FD; i++)
		_mknod(&pipes[i], S_IFIFO);

	/* Initialize ready lists */
	for (i = 0; i <= PRIORITY_LIMIT; i++)
		ready_list[i] = NULL;

	while (1) {
		tasks[current_task].stack = activate(tasks[current_task].stack);
		tasks[current_task].status = TASK_READY;
		timeup = 0;

		switch (tasks[current_task].stack->r7) {
		case 0x1: /* fork */
			if (task_count == TASK_LIMIT) {
				/* Cannot create a new task, return error */
				tasks[current_task].stack->r0 = -1;
			}
			else {
				/* Compute how much of the stack is used */
				size_t used = stacks[current_task] + STACK_SIZE
					      - (unsigned int*)tasks[current_task].stack;
				/* New stack is END - used */
				tasks[task_count].stack = (void*)(stacks[task_count] + STACK_SIZE - used);
				/* Copy only the used part of the stack */
				memcpy(tasks[task_count].stack, tasks[current_task].stack,
				       used * sizeof(unsigned int));
				/* Set PID */
				tasks[task_count].pid = task_count;
				/* Set priority, inherited from forked task */
				tasks[task_count].priority = tasks[current_task].priority;
				/* Set return values in each process */
				tasks[current_task].stack->r0 = task_count;
				tasks[task_count].stack->r0 = 0;
				tasks[task_count].prev = NULL;
				tasks[task_count].next = NULL;
				task_push(&ready_list[tasks[task_count].priority], &tasks[task_count]);
				/* There is now one more task */
				task_count++;
			}
			break;
		case 0x2: /* getpid */
			tasks[current_task].stack->r0 = current_task;
			break;
		case 0x3: /* write */
			_write(&tasks[current_task], tasks, task_count, pipes);
			break;
		case 0x4: /* read */
			_read(&tasks[current_task], tasks, task_count, pipes);
			break;
		case 0x5: /* interrupt_wait */
			/* Enable interrupt */
			NVIC_EnableIRQ(tasks[current_task].stack->r0);
			/* Block task waiting for interrupt to happen */
			tasks[current_task].status = TASK_WAIT_INTR;
			break;
		case 0x6: /* getpriority */
			{
				int who = tasks[current_task].stack->r0;
				if (who > 0 && who < (int)task_count)
					tasks[current_task].stack->r0 = tasks[who].priority;
				else if (who == 0)
					tasks[current_task].stack->r0 = tasks[current_task].priority;
				else
					tasks[current_task].stack->r0 = -1;
			} break;
		case 0x7: /* setpriority */
			{
				int who = tasks[current_task].stack->r0;
				int value = tasks[current_task].stack->r1;
				value = (value < 0) ? 0 : ((value > PRIORITY_LIMIT) ? PRIORITY_LIMIT : value);
				if (who > 0 && who < (int)task_count)
					tasks[who].priority = value;
				else if (who == 0)
					tasks[current_task].priority = value;
				else {
					tasks[current_task].stack->r0 = -1;
					break;
				}
				tasks[current_task].stack->r0 = 0;
			} break;
		case 0x8: /* mknod */
			if (tasks[current_task].stack->r0 < PIPE_LIMIT)
				tasks[current_task].stack->r0 =
					_mknod(&pipes[tasks[current_task].stack->r0],
						   tasks[current_task].stack->r2);
			else
				tasks[current_task].stack->r0 = -1;
			break;
		case 0x9: /* sleep */
			if (tasks[current_task].stack->r0 != 0) {
				tasks[current_task].stack->r0 += tick_count;
				tasks[current_task].status = TASK_WAIT_TIME;
			}
			break;
        case 0xa:
            {
                struct task_control_block* tempTasks=tasks[current_task].stack->r0;
                for(i = 0; i < task_count; i++)
                {
                    tempTasks[i]=tasks[i];
                }
                int* taskCount=tasks[current_task].stack->r1;
                *taskCount=task_count;
            }
            break;
		default: /* Catch all interrupts */
			if ((int)tasks[current_task].stack->r7 < 0) {
				unsigned int intr = -tasks[current_task].stack->r7 - 16;

				if (intr == SysTick_IRQn) {
					/* Never disable timer. We need it for pre-emption */
					timeup = 1;
					tick_count++;
				}
				else {
					/* Disable interrupt, interrupt_wait re-enables */
					NVIC_DisableIRQ(intr);
				}
				/* Unblock any waiting tasks */
				for (i = 0; i < task_count; i++)
					if ((tasks[i].status == TASK_WAIT_INTR && tasks[i].stack->r0 == intr) ||
					    (tasks[i].status == TASK_WAIT_TIME && tasks[i].stack->r0 == tick_count))
						tasks[i].status = TASK_READY;
			}
		}

		/* Put waken tasks in ready list */
		for (task = wait_list; task != NULL;) {
			struct task_control_block *next = task->next;
			if (task->status == TASK_READY)
				task_push(&ready_list[task->priority], task);
			task = next;
		}
		/* Select next TASK_READY task */
		for (i = 0; i < (size_t)tasks[current_task].priority && ready_list[i] == NULL; i++);
		if (tasks[current_task].status == TASK_READY) {
			if (!timeup && i == (size_t)tasks[current_task].priority)
				/* Current task has highest priority and remains execution time */
				continue;
			else
				task_push(&ready_list[tasks[current_task].priority], &tasks[current_task]);
		}
		else {
			task_push(&wait_list, &tasks[current_task]);
		}
		while (ready_list[i] == NULL)
			i++;
		current_task = task_pop(&ready_list[i])->pid;
	}

	return 0;
}
