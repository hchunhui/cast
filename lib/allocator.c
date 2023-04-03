#include "allocator.h"
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

typedef struct Allocator_ Allocator;

#define ALLOCSIZE 16300
struct allocpage {
	struct allocpage *next;
	unsigned char content[];
};

struct Allocator_ {
	struct allocpage *pages;
	int cur;
};

static void *allocator_init(Allocator *a)
{
	a->pages = malloc(sizeof(struct allocpage) + ALLOCSIZE);
	a->pages->next = NULL;
	a->cur = 0;
}

static void *allocator_free(Allocator *a)
{
	for (struct allocpage *i = a->pages; i; ) {
		struct allocpage *next = i->next;
		free(i);
		i = next;
	}
}

void *allocator_new()
{
	Allocator *self = malloc(sizeof(Allocator));
	allocator_init(self);
	return self;
}

void allocator_delete(Allocator *a)
{
	allocator_free(a);
	free(a);
}

static int max(int a, int b)
{
	return a > b ? a : b;
}

void *allocator_memalloc(Allocator *a, int size)
{
	size = (size + 15) / 16 * 16;
	if (a->cur + size < ALLOCSIZE) {
		void *ret = &(a->pages->content[a->cur]);
		a->cur += size;
		return ret;
	} else {
		struct allocpage *n = malloc(sizeof(struct allocpage) +
					     max(size, ALLOCSIZE));
		n->next = a->pages;
		a->pages = n;
		a->cur = size;
		return &(a->pages->content[0]);
	}
}

char *allocator_strdup(Allocator *a, const char *s)
{
	int size = strlen(s);
	char *d = allocator_memalloc(a, size + 1);
	memcpy(d, s, size + 1);
	return d;
}
