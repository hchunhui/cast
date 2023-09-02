#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#ifdef __CAST_MANAGED__
#define BEGIN_MANAGED __managed {
#define END_MANAGED }
#define CALL_MANAGED(f, ...) __managed_##f(__VA_ARGS__)
#else
#define BEGIN_MANAGED
#define END_MANAGED
#define CALL_MANAGED(f, a, ...) f(__VA_ARGS__)
#endif

typedef struct Allocator_ Allocator;
void *allocator_new();
void allocator_delete(Allocator *a);
void *allocator_memalloc(Allocator *a, int size);
char *allocator_strdup(Allocator *a, const char *s);

typedef struct {
	Allocator *allocator;
} Context;

#include <stdlib.h>
static Context *context_new(Allocator *a)
{
	Context *p = malloc(sizeof(Context));
	p->allocator = a;
	return p;
}

static void context_delete(Context *p)
{
	free(p);
}

#define __new(T) ((T *) __new_(sizeof(T)))
static void *__new_(int size)
{
	return malloc(size);
}

#endif /* ALLOCATOR_H */
