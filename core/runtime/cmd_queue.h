#ifndef _PTX_CMD_QUEUE_H
#define _PTX_CMD_QUEUE_H

#define PTX_CMD_QUEUE_SIZE 16
#define PTX_CMD_LINE_LEN   512

void ptx_cmd_queue_init(void);
int  ptx_cmd_queue_push(const char* line);
int  ptx_cmd_queue_pop(char* out);
void ptx_cmd_queue_process(void);

#endif
