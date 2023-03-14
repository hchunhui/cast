#ifndef TREE_H
#define TREE_H

#include <stdbool.h>
#define DFLAG_EXTERN 1
#define DFLAG_STATIC 2
#define DFLAG_INLINE 4

typedef struct Tree_ {
	enum {
#define STMT(id, ...)
#define EXPR(id, ...)
#define TYPE(id, ...)

#undef  STMT
#define STMT(id, ...) STMT_##id,
#include "tree_nodes.def"
#undef STMT
#define STMT(id, ...)
		STMT_BLOCK,
		STMT_DECLS,

#undef  EXPR
#define EXPR(id, ...) EXPR_##id,
#include "tree_nodes.def"
#undef EXPR
#define EXPR(id, ...)
		EXPR_CALL,

#undef  TYPE
#define TYPE(id, ...) TYPE_##id,
#include "tree_nodes.def"
#undef TYPE
#define TYPE(id, ...)
		TYPE_FUN,

#undef STMT
#undef EXPR
#undef TYPE
	} type;
} Tree;

typedef Tree Stmt;
typedef Tree Expr;
typedef Tree Type;

typedef enum {
	EXPR_OP_NEG,
	EXPR_OP_POS,
	EXPR_OP_NOT,
	EXPR_OP_BNOT,

	EXPR_OP_ADDROF,
	EXPR_OP_DEREF,

	EXPR_OP_PREINC,
	EXPR_OP_POSTINC,
	EXPR_OP_PREDEC,
	EXPR_OP_POSTDEC,
} ExprUnOp;

typedef enum {
	EXPR_OP_ADD,
	EXPR_OP_SUB,
	EXPR_OP_MUL,
	EXPR_OP_DIV,
	EXPR_OP_MOD,

	EXPR_OP_OR,
	EXPR_OP_AND,

	EXPR_OP_BOR,
	EXPR_OP_BAND,
	EXPR_OP_BXOR,
	EXPR_OP_BSHL,
	EXPR_OP_BSHR,

	EXPR_OP_COMMA,
	EXPR_OP_IDX,

	EXPR_OP_EQ,
	EXPR_OP_NEQ,
	EXPR_OP_LT,
	EXPR_OP_LE,
	EXPR_OP_GT,
	EXPR_OP_GE,

	EXPR_OP_ASSIGN,
	EXPR_OP_ASSIGNADD,
	EXPR_OP_ASSIGNSUB,
	EXPR_OP_ASSIGNMUL,
	EXPR_OP_ASSIGNDIV,
	EXPR_OP_ASSIGNMOD,
	EXPR_OP_ASSIGNBOR,
	EXPR_OP_ASSIGNBAND,
	EXPR_OP_ASSIGNBXOR,
	EXPR_OP_ASSIGNBSHL,
	EXPR_OP_ASSIGNBSHR,
} ExprBinOp;

#include "vec.h"
typedef vec_t(Type*) vec_type_t;

typedef struct TypeFUN_ {
	Type h;
	Type *rt;
	vec_type_t at;
} TypeFUN;

TypeFUN *typeFUN(Type *rt);
void typeFUN_append(TypeFUN *fun, Type *arg);

typedef vec_t(Expr*) vec_expr_t;

typedef struct ExprCALL_ {
	Expr h;
	Expr *func;
	vec_expr_t args;
} ExprCALL;

ExprCALL *exprCALL(Expr *func);
void exprCALL_append(ExprCALL *call, Expr *arg);

typedef vec_t(Stmt*) vec_stmt_t;

typedef struct StmtBLOCK_ {
	Stmt h;
	vec_stmt_t items;
} StmtBLOCK;

StmtBLOCK *stmtBLOCK();
void stmtBLOCK_append(StmtBLOCK *block, Stmt *i);

typedef struct StmtDECLS_ {
	Stmt h;
	vec_stmt_t items;
} StmtDECLS;
StmtDECLS *stmtDECLS();
void stmtDECLS_append(StmtDECLS *decls, Stmt *i);

#define STMT(id, ...) typedef struct Stmt##id##_ Stmt##id;
#define EXPR(id, ...) typedef struct Expr##id##_ Expr##id;
#define TYPE(id, ...) typedef struct Type##id##_ Type##id;
#include "tree_nodes.def"
#undef STMT
#undef EXPR
#undef TYPE

#define ARGCOUNT_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, ...) _11
#define ARGCOUNT(...) ARGCOUNT_IMPL(~, ## __VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define PASTE0(a, b) a ## b
#define PASTE(a, b) PASTE0(a, b)
#define GEN_0(TYPENAME, FUNCNAME, id)	\
	struct TYPENAME##id##_ { \
		Tree h; \
	}; \
	TYPENAME *FUNCNAME##id();

#define GEN_2(TYPENAME, FUNCNAME, id, T1, v1)	\
	struct TYPENAME##id##_ { \
		Tree h; \
		T1 v1;  \
	}; \
	TYPENAME *FUNCNAME##id(T1 v1);

#define GEN_4(TYPENAME, FUNCNAME, id, T1, v1, T2, v2)	\
	struct TYPENAME##id##_ { \
		Tree h; \
		T1 v1;  \
		T2 v2;  \
	}; \
	TYPENAME *FUNCNAME##id(T1 v1, T2 v2);

#define GEN_6(TYPENAME, FUNCNAME, id, T1, v1, T2, v2, T3, v3)	\
	struct TYPENAME##id##_ { \
		Tree h; \
		T1 v1;  \
		T2 v2;  \
		T3 v3;  \
	}; \
	TYPENAME *FUNCNAME##id(T1 v1, T2 v2, T3 v3);

#define GEN_8(TYPENAME, FUNCNAME, id, T1, v1, T2, v2, T3, v3, T4, v4)	\
	struct TYPENAME##id##_ { \
		Tree h; \
		T1 v1;  \
		T2 v2;  \
		T3 v3;  \
		T4 v4;  \
	}; \
	TYPENAME *FUNCNAME##id(T1 v1, T2 v2, T3 v3, T4 v4);

#define GEN_10(TYPENAME, FUNCNAME, id, T1, v1, T2, v2, T3, v3, T4, v4, T5, v5) \
	struct TYPENAME##id##_ { \
		Tree h; \
		T1 v1;  \
		T2 v2;  \
		T3 v3;  \
		T4 v4;  \
		T5 v5;  \
	}; \
	TYPENAME *FUNCNAME##id(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5);

#define STMT(id, ...) PASTE(GEN_, ARGCOUNT(__VA_ARGS__))(Stmt, stmt, id, ## __VA_ARGS__)
#define EXPR(id, ...) PASTE(GEN_, ARGCOUNT(__VA_ARGS__))(Expr, expr, id, ## __VA_ARGS__)
#define TYPE(id, ...) PASTE(GEN_, ARGCOUNT(__VA_ARGS__))(Type, type, id, ## __VA_ARGS__)
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

void tree_free(Tree *);

#endif /* TREE_H */
