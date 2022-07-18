#ifndef COSMOPOLITAN_LIBC_INTRIN_PROMISES_H_
#define COSMOPOLITAN_LIBC_INTRIN_PROMISES_H_

#define PROMISE_STDIO      0
#define PROMISE_RPATH      1
#define PROMISE_WPATH      2
#define PROMISE_CPATH      3
#define PROMISE_DPATH      4
#define PROMISE_FLOCK      5
#define PROMISE_FATTR      6
#define PROMISE_INET       7
#define PROMISE_UNIX       8
#define PROMISE_DNS        9
#define PROMISE_TTY        10
#define PROMISE_RECVFD     11
#define PROMISE_PROC       12
#define PROMISE_THREAD     13
#define PROMISE_EXEC       14
#define PROMISE_EXECNATIVE 15
#define PROMISE_ID         16
#define PROMISE_UNVEIL     17
#define PROMISE_SENDFD     18

#define PLEDGED(x) ((~__promises >> PROMISE_##x) & 1)

#if !(__ASSEMBLER__ + __LINKER__ + 0)
COSMOPOLITAN_C_START_

hidden extern unsigned long __promises;
hidden extern unsigned long __execpromises;

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */
#endif /* COSMOPOLITAN_LIBC_INTRIN_PROMISES_H_ */
