#ifndef __ITM_OUTPUT__
#define __ITM_OUTPUT__

//This file is only used when building for ULINK output

#define PRINTF(format, ...)     { char xyz[80]; sprintf (xyz, format, ## __VA_ARGS__); ITM_puts(xyz);}
#define PUTS(st)                                                        ITM_puts((char*)st);


int ITM_putc (int ch);
int ITM_getc (void);
int ITM_puts (char *);

#endif
