#ifndef _CYCLEBUF_H_
#define _CYCLEBUF_H_

typedef struct _cy_buf cy_buf;

cy_buf* cyb_create(char* name);

void cyb_putc(cy_buf* b, unsigned char c);

unsigned char cyb_getc(cy_buf* b);

int cyb_isempty(cy_buf* b);

int cyb_isfull(cy_buf* b);

#endif