#ifndef TREE_H
#define TREE_H

#include <stdbool.h>
enum {
	DFLAG_EXTERN      = 1,
	DFLAG_STATIC      = 2,
	DFLAG_INLINE      = 4,
	DFLAG_THREADLOCAL = 8,
	DFLAG_NORETURN    = 16,
	DFLAG_REGISTER    = 32,
	DFLAG_MANAGED     = (1<<31),
};

enum {
	TFLAG_CONST        = 1,
	TFLAG_VOLATILE     = 2,
	TFLAG_RESTRICT     = 4,
	TFLAG_ARRAY_STATIC = 8,
	TFLAG_ATOMIC       = 16,
	TFLAG_COMPLEX      = 32,
	TFLAG_IMAGINARY    = 64,
};

typedef struct EnumList_ EnumList;

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
		// gcc extensions
		STMT_ASM,

#undef  EXPR
#define EXPR(id, ...) EXPR_##id,
#include "tree_nodes.def"
#undef EXPR
#define EXPR(id, ...)
		EXPR_CALL,
		EXPR_INIT,
		// C11
		EXPR_GENERIC,

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
	PT_INT,
	PT_LONG,
	PT_SHORT,
	PT_CHAR,
	PT_SCHAR,
	PT_LLONG,
	PT_UINT,
	PT_ULONG,
	PT_USHORT,
	PT_UCHAR,
	PT_ULLONG,
	PT_FLOAT,
	PT_DOUBLE,
	PT_LDOUBLE,
	PT_BOOL,

	// gcc extensions
	PT_INT128,
	PT_UINT128,
} PTKind;

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

	// gcc extensions
	EXPR_OP_ADDROFLABEL,
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

typedef enum {
	WCK_NONE = 0,
	WCK_L,
	WCK_u,
	WCK_U,
	WCK_u8,
} WCKind;

#include "allocator.h"
#include "avec.h"
typedef avec_t(Type*) avec_type_t;

typedef struct TypeFUN_ {
	Type h;
	Type *rt;
	avec_type_t at;
	bool va_arg;
} TypeFUN;

BEGIN_MANAGED
TypeFUN *typeFUN(Type *rt);
void typeFUN_append(TypeFUN *fun, Type *arg);
void typeFUN_prepend(TypeFUN *fun, Type *arg);
END_MANAGED

typedef avec_t(Expr*) avec_expr_t;

typedef struct ExprCALL_ {
	Expr h;
	Expr *func;
	avec_expr_t args;
} ExprCALL;

typedef struct Attribute_ {
	const char *name;
	avec_expr_t args;
	struct Attribute_ *next;
} Attribute;

typedef struct {
	Attribute *gcc_attribute;
	const char *gcc_asm_name;
	Expr *c11_alignas;
} Extension;

BEGIN_MANAGED
ExprCALL *exprCALL(Expr *func);
void exprCALL_append(ExprCALL *call, Expr *arg);
void exprCALL_prepend(ExprCALL *call, Expr *arg);
END_MANAGED

typedef struct Designator_ {
	enum {
		DES_FIELD,
		DES_INDEX,
		DES_INDEXRANGE,
	} type;
	Expr *index, *indexhigh;
	const char *field;
	struct Designator_ *next;
} Designator;
typedef struct ExprINITItem_ {
	Designator *designator;
	Expr *value;
} ExprINITItem;
typedef avec_t(ExprINITItem) avec_inititem_t;

typedef struct ExprINIT_ {
	Expr h;
	avec_inititem_t items;
} ExprINIT;

BEGIN_MANAGED
ExprINIT *exprINIT();
void exprINIT_append(ExprINIT *init, Designator *d, Expr *e);
END_MANAGED

typedef struct GENERICPair_ {
	Type *type; // NULL => default
	Expr *expr;
} GENERICPair;
typedef avec_t(GENERICPair) avec_gpair_t;
typedef struct ExprGENERIC_ {
	Expr h;
	Expr *expr;
	avec_gpair_t items;
} ExprGENERIC;

BEGIN_MANAGED
ExprGENERIC *exprGENERIC(Expr *expr);
void exprGENERIC_append(ExprGENERIC *e, GENERICPair pair);
END_MANAGED

typedef avec_t(Stmt*) avec_stmt_t;

typedef struct StmtBLOCK_ {
	Stmt h;
	avec_stmt_t items;
} StmtBLOCK;

BEGIN_MANAGED
StmtBLOCK *stmtBLOCK();
void stmtBLOCK_append(StmtBLOCK *block, Stmt *i);
void stmtBLOCK_prepend(StmtBLOCK *block, Stmt *i);
END_MANAGED

typedef struct StmtDECLS_ {
	Stmt h;
	avec_stmt_t items;
} StmtDECLS;

BEGIN_MANAGED
StmtDECLS *stmtDECLS();
void stmtDECLS_append(StmtDECLS *decls, Stmt *i);
END_MANAGED

enum {
	ASM_FLAG_VOLATILE = 1,
	ASM_FLAG_INLINE   = 2,
	ASM_FLAG_GOTO     = 4,
};

typedef struct ASMOper_ {
	const char *symbol;
	const char *constraint;
	Expr *variable;
} ASMOper;
typedef avec_t(ASMOper) avec_asmoper_t;
typedef avec_t(const char *) avec_str_t;
typedef struct StmtASM_ {
	Stmt h;
	unsigned int flags;
	const char *content;
	avec_asmoper_t outputs;
	avec_asmoper_t inputs;
	avec_str_t clobbers;
	avec_str_t gotolabels;
} StmtASM;

BEGIN_MANAGED
StmtASM *stmtASM(unsigned int flags, const char *content);
void stmtASM_append_output(StmtASM *s, ASMOper oper);
void stmtASM_append_input(StmtASM *s, ASMOper oper);
void stmtASM_append_clobber(StmtASM *s, const char *name);
void stmtASM_append_gotolabels(StmtASM *s, const char *name);
END_MANAGED

struct EnumPair_ {
	const char *id;
	Attribute *attr;
	Expr *val;
};
typedef avec_t(struct EnumPair_) avec_epair_t;

struct EnumList_ {
	avec_epair_t items;
};

#define STMT(id, ...) typedef struct Stmt##id##_ Stmt##id;
#define EXPR(id, ...) typedef struct Expr##id##_ Expr##id;
#define TYPE(id, ...) typedef struct Type##id##_ Type##id;
#include "tree_nodes.def"
#undef STMT
#undef EXPR
#undef TYPE

#define ARGCOUNT_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, ...) _13
#define ARGCOUNT(...) ARGCOUNT_IMPL(~, ## __VA_ARGS__, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define PASTE0(a, b) a ## b
#define PASTE(a, b) PASTE0(a, b)
#define GEN_0(TYPENAME, FUNCNAME, id)	\
	struct TYPENAME##id##_ { \
		Tree h; \
	}; \
	BEGIN_MANAGED \
	TYPENAME *FUNCNAME##id(); \
	END_MANAGED

#define GEN_2(TYPENAME, FUNCNAME, id, T1, v1)	\
	struct TYPENAME##id##_ { \
		Tree h; \
		T1 v1;  \
	}; \
	BEGIN_MANAGED \
	TYPENAME *FUNCNAME##id(T1 v1); \
	END_MANAGED

#define GEN_4(TYPENAME, FUNCNAME, id, T1, v1, T2, v2)	\
	struct TYPENAME##id##_ { \
		Tree h; \
		T1 v1;  \
		T2 v2;  \
	}; \
	BEGIN_MANAGED \
	TYPENAME *FUNCNAME##id(T1 v1, T2 v2); \
	END_MANAGED

#define GEN_6(TYPENAME, FUNCNAME, id, T1, v1, T2, v2, T3, v3)	\
	struct TYPENAME##id##_ { \
		Tree h; \
		T1 v1;  \
		T2 v2;  \
		T3 v3;  \
	}; \
	BEGIN_MANAGED \
	TYPENAME *FUNCNAME##id(T1 v1, T2 v2, T3 v3); \
	END_MANAGED

#define GEN_8(TYPENAME, FUNCNAME, id, T1, v1, T2, v2, T3, v3, T4, v4)	\
	struct TYPENAME##id##_ { \
		Tree h; \
		T1 v1;  \
		T2 v2;  \
		T3 v3;  \
		T4 v4;  \
	}; \
	BEGIN_MANAGED \
	TYPENAME *FUNCNAME##id(T1 v1, T2 v2, T3 v3, T4 v4); \
	END_MANAGED

#define GEN_10(TYPENAME, FUNCNAME, id, T1, v1, T2, v2, T3, v3, T4, v4, T5, v5) \
	struct TYPENAME##id##_ { \
		Tree h; \
		T1 v1;  \
		T2 v2;  \
		T3 v3;  \
		T4 v4;  \
		T5 v5;  \
	}; \
	BEGIN_MANAGED \
	TYPENAME *FUNCNAME##id(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5); \
	END_MANAGED

#define GEN_12(TYPENAME, FUNCNAME, id, T1, v1, T2, v2, T3, v3, T4, v4, T5, v5, T6, v6) \
	struct TYPENAME##id##_ { \
		Tree h; \
		T1 v1;  \
		T2 v2;  \
		T3 v3;  \
		T4 v4;  \
		T5 v5;  \
		T6 v6;  \
	}; \
	BEGIN_MANAGED \
	TYPENAME *FUNCNAME##id(T1 v1, T2 v2, T3 v3, T4 v4, T5 v5, T6 v6); \
	END_MANAGED

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
#undef GEN_12


#endif /* TREE_H */
