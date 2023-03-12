#include "tree.h"
#include <stdlib.h>

#define __new(T) ((T *) malloc(sizeof(T)))

TypeFUN *typeFUN(Type *rt)
{
	TypeFUN *t = __new(TypeFUN);
	t->h.type = TYPE_FUN;
	t->rt = rt;
	vec_init(&t->at);
	return t;
}

void typeFUN_append(TypeFUN *fun, Type *arg)
{
	(void) vec_push(&fun->at, arg);
}

ExprCALL *exprCALL(Expr *func)
{
	ExprCALL *t = __new(ExprCALL);
	t->h.type = EXPR_CALL;
	t->func = func;
	vec_init(&t->args);
	return t;
}

void exprCALL_append(ExprCALL *call, Expr *arg)
{
	(void) vec_push(&call->args, arg);
}

StmtBLOCK *stmtBLOCK()
{
	StmtBLOCK *t = __new(StmtBLOCK);
	t->h.type = STMT_BLOCK;
	vec_init(&t->items);
	return t;
}

void stmtBLOCK_append(StmtBLOCK *block, Stmt *i)
{
	(void) vec_push(&block->items, i);
}

void tree_free(Tree *t)
{
}

#define ARGCOUNT_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, ...) _11
#define ARGCOUNT(...) ARGCOUNT_IMPL(~, ## __VA_ARGS__,  10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define PASTE0(a, b) a ## b
#define PASTE(a, b) PASTE0(a, b)
#define GEN_0(TYPENAME, FUNCNAME, ENUMNAME, id)	\
TYPENAME *FUNCNAME##id() \
{ \
	TYPENAME##id *__t =  __new(TYPENAME##id); \
	__t->h.type = ENUMNAME##_##id; \
	return &__t->h; \
}

#define GEN_2(TYPENAME, FUNCNAME, ENUMNAME, id, T1, v1)	\
TYPENAME *FUNCNAME##id(T1 v1) \
{ \
	TYPENAME##id *__t =  __new(TYPENAME##id); \
	__t->h.type = ENUMNAME##_##id; \
	__t->v1 = v1; \
	return &__t->h; \
}

#define GEN_4(TYPENAME, FUNCNAME, ENUMNAME, id, T1, v1, T2, v2)	\
TYPENAME *FUNCNAME##id(T1 v1, T2 v2) \
{ \
	TYPENAME##id *__t =  __new(TYPENAME##id); \
	__t->h.type = ENUMNAME##_##id; \
	__t->v1 = v1; \
	__t->v2 = v2; \
	return &__t->h; \
}

#define GEN_6(TYPENAME, FUNCNAME, ENUMNAME, id, T1, v1, T2, v2, T3, v3)	\
TYPENAME *FUNCNAME##id(T1 v1, T2 v2, T3 v3) \
{ \
	TYPENAME##id *__t =  __new(TYPENAME##id); \
	__t->h.type = ENUMNAME##_##id; \
	__t->v1 = v1; \
	__t->v2 = v2; \
	__t->v3 = v3; \
	return &__t->h; \
}

#define GEN_8(TYPENAME, FUNCNAME, ENUMNAME, id, T1, v1, T2, v2, T3, v3, T4, v4) \
TYPENAME *FUNCNAME##id(T1 v1, T2 v2, T3 v3, T4 v4) \
{ \
	TYPENAME##id *__t =  __new(TYPENAME##id); \
	__t->h.type = ENUMNAME##_##id; \
	__t->v1 = v1; \
	__t->v2 = v2; \
	__t->v3 = v3; \
	__t->v4 = v4; \
	return &__t->h; \
}

#define GEN_10(TYPENAME, FUNCNAME, ENUMNAME, id, T1, v1, T2, v2, T3, v3, T4, v4, T5, v5) \
TYPENAME *FUNCNAME##id(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5) \
{ \
	TYPENAME##id *__t =  __new(TYPENAME##id); \
	__t->h.type = ENUMNAME##_##id; \
	__t->v1 = v1; \
	__t->v2 = v2; \
	__t->v3 = v3; \
	__t->v4 = v4; \
	__t->v5 = v5; \
	return &__t->h; \
}

#define STMT(id, ...) PASTE(GEN_, ARGCOUNT(__VA_ARGS__))(Stmt, stmt, STMT, id, ## __VA_ARGS__)
#define EXPR(id, ...) PASTE(GEN_, ARGCOUNT(__VA_ARGS__))(Expr, expr, EXPR, id, ## __VA_ARGS__)
#define TYPE(id, ...) PASTE(GEN_, ARGCOUNT(__VA_ARGS__))(Type, type, TYPE, id, ## __VA_ARGS__)
#include "tree_nodes.def"
#undef STMT
#undef EXPR
#undef TYPE
#undef ARGCOUNT_IMPL
#undef ARGCOUNT
#undef PASTE0
#undef PASTE
#undef GEN_0
#undef GEN_2
#undef GEN_4
#undef GEN_6
#undef GEN_8
