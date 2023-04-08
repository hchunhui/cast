#include "tree.h"
#include <stdlib.h>

BEGIN_MANAGED

TypeFUN *typeFUN(Type *rt)
{
	TypeFUN *t = __new(TypeFUN);
	t->h.type = TYPE_FUN;
	t->rt = rt;
	avec_init(&t->at);
	t->va_arg = false;
	return t;
}

void typeFUN_append(TypeFUN *fun, Type *arg)
{
	(void) avec_push(&fun->at, arg);
}

void typeFUN_prepend(TypeFUN *fun, Type *arg)
{
	(void) avec_insert(&fun->at, 0, arg);
}

ExprCALL *exprCALL(Expr *func)
{
	ExprCALL *t = __new(ExprCALL);
	t->h.type = EXPR_CALL;
	t->func = func;
	avec_init(&t->args);
	return t;
}

void exprCALL_append(ExprCALL *call, Expr *arg)
{
	(void) avec_push(&call->args, arg);
}

void exprCALL_prepend(ExprCALL *call, Expr *arg)
{
	(void) avec_insert(&call->args, 0, arg);
}

ExprINIT *exprINIT()
{
	ExprINIT *t = __new(ExprINIT);
	t->h.type = EXPR_INIT;
	avec_init(&t->items);
	return t;
}

void exprINIT_append(ExprINIT *init, Designator *d, Expr *e)
{
	ExprINITItem item = {d, e};
	(void) avec_push(&init->items, item);
}

ExprGENERIC *exprGENERIC(Expr *expr)
{
	ExprGENERIC *t = __new(ExprGENERIC);
	t->h.type = EXPR_GENERIC;
	t->expr = expr;
	avec_init(&t->items);
	return t;
}

void exprGENERIC_append(ExprGENERIC *e, GENERICPair item)
{
	(void) avec_push(&e->items, item);
}

StmtBLOCK *stmtBLOCK()
{
	StmtBLOCK *t = __new(StmtBLOCK);
	t->h.type = STMT_BLOCK;
	avec_init(&t->items);
	return t;
}

void stmtBLOCK_append(StmtBLOCK *block, Stmt *i)
{
	(void) avec_push(&block->items, i);
}

void stmtBLOCK_prepend(StmtBLOCK *block, Stmt *i)
{
	(void) avec_insert(&block->items, 0, i);
}

StmtDECLS *stmtDECLS()
{
	StmtDECLS *t = __new(StmtDECLS);
	t->h.type = STMT_DECLS;
	avec_init(&t->items);
	return t;
}

void stmtDECLS_append(StmtDECLS *block, Stmt *i)
{
	(void) avec_push(&block->items, i);
}

StmtASM *stmtASM(unsigned int flags, const char *content)
{
	StmtASM *t = __new(StmtASM);
	t->h.type = STMT_ASM;
	t->flags = flags;
	t->content = content;
	avec_init(&t->outputs);
	avec_init(&t->inputs);
	avec_init(&t->clobbers);
	avec_init(&t->gotolabels);
	return t;
}

void stmtASM_append_output(StmtASM *s, ASMOper oper)
{
	(void) avec_push(&s->outputs, oper);
}

void stmtASM_append_input(StmtASM *s, ASMOper oper)
{
	(void) avec_push(&s->inputs, oper);
}

void stmtASM_append_clobber(StmtASM *s, const char *name)
{
	(void) avec_push(&s->clobbers, name);
}

void stmtASM_append_gotolabels(StmtASM *s, const char *name)
{
	(void) avec_push(&s->gotolabels, name);
}

END_MANAGED

#define ARGCOUNT_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, ...) _13
#define ARGCOUNT(...) ARGCOUNT_IMPL(~, ## __VA_ARGS__,  12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define PASTE0(a, b) a ## b
#define PASTE(a, b) PASTE0(a, b)
#define GEN_0(TYPENAME, FUNCNAME, ENUMNAME, id)	\
BEGIN_MANAGED \
TYPENAME *FUNCNAME##id() \
{ \
	TYPENAME##id *__t =  __new(TYPENAME##id); \
	__t->h.type = ENUMNAME##_##id; \
	return &__t->h; \
} \
END_MANAGED

#define GEN_2(TYPENAME, FUNCNAME, ENUMNAME, id, T1, v1)	\
BEGIN_MANAGED \
TYPENAME *FUNCNAME##id(T1 v1) \
{ \
	TYPENAME##id *__t =  __new(TYPENAME##id); \
	__t->h.type = ENUMNAME##_##id; \
	__t->v1 = v1; \
	return &__t->h; \
} \
END_MANAGED

#define GEN_4(TYPENAME, FUNCNAME, ENUMNAME, id, T1, v1, T2, v2)	\
BEGIN_MANAGED \
TYPENAME *FUNCNAME##id(T1 v1, T2 v2) \
{ \
	TYPENAME##id *__t =  __new(TYPENAME##id); \
	__t->h.type = ENUMNAME##_##id; \
	__t->v1 = v1; \
	__t->v2 = v2; \
	return &__t->h; \
} \
END_MANAGED

#define GEN_6(TYPENAME, FUNCNAME, ENUMNAME, id, T1, v1, T2, v2, T3, v3)	\
BEGIN_MANAGED \
TYPENAME *FUNCNAME##id(T1 v1, T2 v2, T3 v3) \
{ \
	TYPENAME##id *__t =  __new(TYPENAME##id); \
	__t->h.type = ENUMNAME##_##id; \
	__t->v1 = v1; \
	__t->v2 = v2; \
	__t->v3 = v3; \
	return &__t->h; \
} \
END_MANAGED

#define GEN_8(TYPENAME, FUNCNAME, ENUMNAME, id, T1, v1, T2, v2, T3, v3, T4, v4) \
BEGIN_MANAGED \
TYPENAME *FUNCNAME##id(T1 v1, T2 v2, T3 v3, T4 v4) \
{ \
	TYPENAME##id *__t =  __new(TYPENAME##id); \
	__t->h.type = ENUMNAME##_##id; \
	__t->v1 = v1; \
	__t->v2 = v2; \
	__t->v3 = v3; \
	__t->v4 = v4; \
	return &__t->h; \
} \
END_MANAGED

#define GEN_10(TYPENAME, FUNCNAME, ENUMNAME, id, T1, v1, T2, v2, T3, v3, T4, v4, T5, v5) \
BEGIN_MANAGED \
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
} \
END_MANAGED

#define GEN_12(TYPENAME, FUNCNAME, ENUMNAME, id, T1, v1, T2, v2, T3, v3, T4, v4, T5, v5, T6, v6) \
BEGIN_MANAGED \
TYPENAME *FUNCNAME##id(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6) \
{ \
	TYPENAME##id *__t =  __new(TYPENAME##id); \
	__t->h.type = ENUMNAME##_##id; \
	__t->v1 = v1; \
	__t->v2 = v2; \
	__t->v3 = v3; \
	__t->v4 = v4; \
	__t->v5 = v5; \
	__t->v6 = v6; \
	return &__t->h; \
} \
END_MANAGED

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
#undef GEN_10
#undef GEN_12
