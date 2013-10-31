#ifndef FIO_H
#define FIO_H

#include <stddef.h>
#include "task.h"

#define PIPE_BUF   64 /* Size of largest atomic pipe message */
#define PATH_MAX   32 /* Longest absolute path */
#define PIPE_LIMIT (TASK_LIMIT * 2)
#define S_IFIFO 1
#define S_IMSGQ 2
#define O_CREAT 4

#define PATHSERVER_FD (TASK_LIMIT + 3)
	/* File descriptor of pipe to pathserver */

#define RB_PUSH(rb, size, v) do { \
		(rb).data[(rb).end] = (v); \
		(rb).end++; \
		if ((rb).end >= size) (rb).end = 0; \
	} while (0)

#define RB_POP(rb, size, v) do { \
		(v) = (rb).data[(rb).start]; \
		(rb).start++; \
		if ((rb).start >= size) (rb).start = 0; \
	} while (0)

#define RB_PEEK(rb, size, v, i) do { \
		int _counter = (i); \
		int _src_index = (rb).start; \
		int _dst_index = 0; \
		while (_counter--) { \
			((char*)&(v))[_dst_index++] = (rb).data[_src_index++]; \
			if (_src_index >= size) _src_index = 0; \
		} \
	} while (0)

#define RB_LEN(rb, size) (((rb).end - (rb).start) + \
	(((rb).end < (rb).start) ? size : 0))

#define PIPE_PUSH(pipe, v) RB_PUSH((pipe), PIPE_BUF, (v))
#define PIPE_POP(pipe, v)  RB_POP((pipe), PIPE_BUF, (v))
#define PIPE_PEEK(pipe, v, i)  RB_PEEK((pipe), PIPE_BUF, (v), (i))
#define PIPE_LEN(pipe)     (RB_LEN((pipe), PIPE_BUF))

struct pipe_ringbuffer {
	int start;
	int end;
	char data[PIPE_BUF];

	int (*readable) (struct pipe_ringbuffer*, struct task_control_block*);
	int (*writable) (struct pipe_ringbuffer*, struct task_control_block*);
	int (*read) (struct pipe_ringbuffer*, struct task_control_block*);
	int (*write) (struct pipe_ringbuffer*, struct task_control_block*);
};

void puts(char *s);
int mkfile(const char *pathname, int mode, int dev);
int mkfifo(const char *pathname, int mode);
int open(const char *pathname, int flags);
int mq_open(const char *name, int oflag);
void _read(struct task_control_block *task, struct task_control_block *tasks, size_t task_count, struct pipe_ringbuffer *pipes);
void _write(struct task_control_block *task, struct task_control_block *tasks, size_t task_count, struct pipe_ringbuffer *pipes);
int fifo_readable (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int mq_readable (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int fifo_read (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int mq_read (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int fifo_writable (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int mq_writable (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int fifo_write (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int mq_write (struct pipe_ringbuffer *pipe, struct task_control_block *task);
int _mknod(struct pipe_ringbuffer *pipe, int dev);

#endif // FIO_H
