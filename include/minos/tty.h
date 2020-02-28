#ifndef __MINOS__TTY_H__
#define __MINOS_TTY_H__

struct tty;

struct tty_ops {
	int (*put_char)(struct tty *tty, char ch);
	int (*put_chars)(struct tty *tty, char *str, int count);
	int (*open)(struct tty *tty);
	void (*close)(struct tty *tty);
};

struct tty {
	uint32_t id;
	int open;
	unsigned long flags;
	struct tty_ops *ops;
	void *pdata;
	struct list_head list;
};

struct tty *alloc_tty(uint32_t id, unsigned long flags);
int register_tty(struct tty *tty);
int release_tty(struct tty *tty);
void close_tty(struct tty *tty);
struct tty *open_tty(uint32_t id);

#endif
