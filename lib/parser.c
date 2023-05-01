#include "parser.h"
#include "lexer.h"
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "map.h"

#define SYM_IDENT 1
#define SYM_TYPE 2

struct scope_item {
	struct scope_item *next;
	map_int_t syms;
};

struct Parser_ {
	Lexer *lexer;
	struct scope_item *scopes;
	int counter;
	int next_count;

	int managed_count;
};

static int symlookup(Parser *p, const char *sym)
{
	int v = SYM_IDENT;
	for (struct scope_item *i = p->scopes; i; i = i->next) {
		int *pv = map_get(&(i->syms), sym);
		if (pv) {
			v = *pv;
			break;
		}
	}
	return v;
}

static bool symset(Parser *p, const char *sym, int sv)
{
	int *pv = map_get(&(p->scopes->syms), sym);
	if (pv && *pv != sv)
		return false;
	map_set(&(p->scopes->syms), sym, sv);
	return true;
}

static struct scope_item *get_scope(Parser *p)
{
	struct scope_item *i = p->scopes;
	p->scopes = i->next;
	return i;
}

static struct scope_item *dup_scope(Parser *p)
{
	struct scope_item *i = malloc(sizeof(struct scope_item));
	i->next = NULL;
	map_init(&(i->syms));

	const char *key;
	map_iter_t iter = map_iter(&(p->scopes->syms));
	while ((key = map_next(&(p->scopes->syms), &iter))) {
		map_set(&(i->syms), key, *map_get(&(p->scopes->syms), key));
	}
	return i;
}

static void restore_scope(Parser *p, struct scope_item *i)
{
	if (i == NULL) {
		i = malloc(sizeof(struct scope_item));
		i->next = NULL;
		map_init(&(i->syms));
	}
	i->next = p->scopes;
	p->scopes = i;
}

static void free_scope(struct scope_item *i)
{
	if (i) {
		map_deinit(&(i->syms));
		free(i);
	}
}

static void enter_scope(Parser *p)
{
	restore_scope(p, NULL);
}

static void leave_scope(Parser *p)
{
	struct scope_item *i = get_scope(p);
	free_scope(i);
}

static void parser_init(Parser *p, Lexer *l)
{
	p->lexer = l;
	p->scopes = NULL;
	enter_scope(p);
	const char *const gcc_builtin_types[] = {
		"__builtin_va_list",
		"_Float16", "_Float16x",
		"_Float32", "_Float32x",
		"_Float64", "_Float64x",
		"_Float128", "_Float128x",
		"__int128_t", "__uint128_t",
		NULL
	};
	for (const char *const *t = gcc_builtin_types; *t; t++)
		symset(p, *t, SYM_TYPE);
	p->counter = 0;
	p->next_count = 0;

	p->managed_count = 0;
}

static void parser_free(Parser *p)
{
	leave_scope(p);
}

Parser *parser_new(Lexer *l)
{
	Parser *self = malloc(sizeof(Parser));
	parser_init(self, l);
	return self;
}

void parser_delete(Parser *p)
{
	parser_free(p);
	free(p);
}

BEGIN_MANAGED

static const char *__new_cstring(const char *s)
{
	int len = strlen(s);
	char *str = __new_(len + 1);
	memcpy(str, s, len + 1);
	return str;
}

#define P lexer_peek(p->lexer)
#define PS lexer_peek_string(p->lexer)
#define PSL lexer_peek_string_len(p->lexer)
#define PI lexer_peek_uint(p->lexer)
#define PF lexer_peek_float(p->lexer)
#define PC lexer_peek_char(p->lexer)
#define N (lexer_next(p->lexer), p->next_count++)
#define F_(cond, errval, ...) do { if (!(cond)) { __VA_ARGS__; return errval; } } while (0)
#define F(cond, ...) F_(cond, 0, ## __VA_ARGS__)

static int match(Parser *p, int token)
{
	if (P == token) {
		N; return 1;
	} else {
		return 0;
	}
}

static const char *get_and_next(Parser *p)
{
	const char *id = __new_cstring(PS);
	N; return id;
}

static Expr *parse_primary_expr(Parser *p);        // 1
static Expr *parse_postfix_expr(Parser *p);        // 2
static Expr *parse_unary_expr(Parser *p);          // 3
static Expr *parse_cast_expr(Parser *p);           // 4
static Expr *parse_multiplicative_expr(Parser *p); // 5
static Expr *parse_additive_expr(Parser *p);       // 6
static Expr *parse_shift_expr(Parser *p);          // 7
static Expr *parse_relational_expr(Parser *p);     // 8
static Expr *parse_equality_expr(Parser *p);       // 9
static Expr *parse_band_expr(Parser *p);           // 10
static Expr *parse_bxor_expr(Parser *p);           // 11
static Expr *parse_bor_expr(Parser *p);            // 12
static Expr *parse_and_expr(Parser *p);            // 13
static Expr *parse_or_expr(Parser *p);             // 14
static Expr *parse_conditional_expr(Parser *p);    // 15
static Expr *parse_assignment_expr(Parser *p);     // 16
static Expr *parse_initializer(Parser *p);

static StmtBLOCK *parse_block_stmt(Parser *p);
static Expr *parse_parentheses_post(Parser *p)
{
	if (P == '{') {
		StmtBLOCK *block;
		F(block = parse_block_stmt(p));
		F(match(p, ')'));
		return exprSTMT(block);
	} else {
		Expr *e;
		F(e = parse_expr(p));
		F(match(p, ')'));
		return exprEXPR(e);
	}
}

static Expr *parse_ident_or_builtin(Parser *p, const char *id)
{
	if (strcmp(id, "__builtin_va_start") == 0) {
		Expr *ap;
		const char *last;
		F(match(p, '('));
		F(ap = parse_assignment_expr(p));
		F(match(p, ','));
		F(P == TOK_IDENT);
		last = get_and_next(p);
		F(match(p, ')'));
		return exprVASTART(ap, last);
	} else if (strcmp(id, "__builtin_va_arg") == 0) {
		Expr *ap;
		Type *type;
		F(match(p, '('));
		F(ap = parse_assignment_expr(p));
		F(match(p, ','));
		F(type = parse_type(p));
		F(match(p, ')'));
		return exprVAARG(ap, type);
	} else if (strcmp(id, "__builtin_va_end") == 0) {
		Expr *ap;
		F(match(p, '('));
		F(ap = parse_assignment_expr(p));
		F(match(p, ')'));
		return exprVAEND(ap);
	} else if (strcmp(id, "__builtin_offsetof") == 0) {
		Type *type;
		Expr *mem;
		F(match(p, '('));
		F(type = parse_type(p));
		F(match(p, ','));
		F(mem = parse_assignment_expr(p));
		F(match(p, ')'));
		return exprOFFSETOF(type, mem);
	} else if (strcmp(id, "__builtin_types_compatible_p") == 0) {
		Type *type1, *type2;
		F(match(p, '('));
		F(type1 = parse_type(p));
		F(match(p, ','));
		F(type2 = parse_type(p));
		F(match(p, ')'));
		return exprTYPESCOMPATIBLE(type1, type2);
	}
	if (symlookup(p, id) != SYM_IDENT)
		return NULL;
	return exprIDENT(id);
}

static Expr *parse_primary_expr(Parser *p)
{
	switch (P) {
	case TOK_IDENT: {
		const char *id = get_and_next(p);
		return parse_ident_or_builtin(p, id);
	}
	case TOK_INT_CST: {
		int i = PI;
		N; return exprINT_CST(i);
	}
	case TOK_UINT_CST: {
		unsigned int i = PI;
		N; return exprUINT_CST(i);
	}
	case TOK_LONG_CST: {
		long i = PI;
		N; return exprLONG_CST(i);
	}
	case TOK_ULONG_CST: {
		unsigned long i = PI;
		N; return exprULONG_CST(i);
	}
	case TOK_LLONG_CST: {
		long long i = PI;
		N; return exprLLONG_CST(i);
	}
	case TOK_ULLONG_CST: {
		unsigned long long i = PI;
		N; return exprULLONG_CST(i);
	}
	case TOK_WCHAR_CST:
	case TOK_CHAR_CST: {
		bool wide = P == TOK_WCHAR_CST;
		char c = PC;
		N; return exprCHAR_CST(c, wide);
	}
	case TOK_WSTRING_CST:
	case TOK_STRING_CST: {
		bool wide = P == TOK_WSTRING_CST;
		int len = PSL;
		char *str = __new_(len);
		memcpy(str, PS, len);
		N; return exprSTRING_CST(str, len, wide);
	}
	case TOK_FLOAT_CST: {
		float i = PF;
		N; return exprFLOAT_CST(i);
	}
	case TOK_DOUBLE_CST: {
		double i = PF;
		N; return exprDOUBLE_CST(i);
	}
	case '(':
		N; return parse_parentheses_post(p);
	case TOK_GENERIC: {
		N; Expr *expr;
		F(match(p, '('));
		F(expr = parse_assignment_expr(p));
		ExprGENERIC *g = exprGENERIC(expr);
		while(match(p, ',')) {
			GENERICPair item = (GENERICPair) {};
			F(match(p, TOK_DEFAULT) || (item.type = parse_type(p)));
			F(match(p, ':'));
			F(item.expr = parse_assignment_expr(p));
			exprGENERIC_append(g, item);
		}
		F(match(p, ')'));
		return (Expr *) g;
	}
	default:
		return NULL;
	}
}

static Expr *parse_assignment_expr_post(Parser *p, Expr *e, int prec);
static Expr *parse_multiplicative_expr_post(Parser *p, Expr *e, int prec);
static Expr *parse_postfix_expr_post(Parser *p, Expr *e, int prec) // 2
{
	if (prec < 2)
		return e;

	while (1) {
		switch (P) {
		case '[': {
			N; Expr *i;
			F(i = parse_expr(p));
			F(match(p, ']'));
			e = exprBOP(EXPR_OP_IDX, e, i);
			break;
		}
		case '(': {
			N; ExprCALL *n = exprCALL(e);
			if (!match(p, ')')) {
				Expr *arg;
				F(arg = parse_assignment_expr(p));
				exprCALL_append(n, arg);
				while (match(p, ',')) {
					F(arg = parse_assignment_expr(p));
					exprCALL_append(n, arg);
				}
				F(match(p, ')'));
			}
			e = &n->h;
			break;
		}
		case '.': {
			N; if (P == TOK_IDENT) {
				e = exprMEM(e, get_and_next(p));
			} else {
				return NULL;
			}
			break;
		}
		case TOK_PMEM: { // -
			N; if (P == TOK_IDENT) {
				e = exprPMEM(e, get_and_next(p));
			} else {
				return NULL;
			}
			break;
		}
		case TOK_INC: {
			N; e = exprUOP(EXPR_OP_POSTINC, e);
			break;
		}
		case TOK_DEC: {
			N; e = exprUOP(EXPR_OP_POSTDEC, e);
			break;
		}
		default:
			if (P == '=') {
				return parse_assignment_expr_post(p, e, prec);
			} else {
				return parse_multiplicative_expr_post(p, e, prec);
			}
		}
	}
}

static Expr *applyUOP(ExprUnOp op, Expr *e)
{
	if (e)
		return exprUOP(op, e);
	return NULL;
}

static Expr *parse_unary_expr(Parser *p)
{
	Expr *e = parse_primary_expr(p);
	if (e)
		return parse_postfix_expr_post(p, e, 3);

	switch (P) {
	case TOK_EXTENSION:
		N; return parse_unary_expr(p);
	case TOK_INC:
		N; return applyUOP(EXPR_OP_PREINC, parse_unary_expr(p));
	case TOK_DEC:
		N; return applyUOP(EXPR_OP_PREDEC, parse_unary_expr(p));
	case '!':
		N; return applyUOP(EXPR_OP_NOT, parse_cast_expr(p));
	case '~':
		N; return applyUOP(EXPR_OP_BNOT, parse_cast_expr(p));
	case '+':
		N; return applyUOP(EXPR_OP_POS, parse_cast_expr(p));
	case '-':
		N; return applyUOP(EXPR_OP_NEG, parse_cast_expr(p));
	case '&':
		N; return applyUOP(EXPR_OP_ADDROF, parse_cast_expr(p));
	case '*':
		N; return applyUOP(EXPR_OP_DEREF, parse_cast_expr(p));
	case TOK_SIZEOF:
		N; if (match(p, '(')) {
			Type *t = parse_type(p);
			if (t) {
				F(match(p, ')'));
				// sizeof compound litera: sizeof (type) {...}
				if (P == '{') {
					Expr *ei = parse_initializer(p);
					if (ei) {
						e = parse_postfix_expr_post(p, exprCAST(t, ei), 4);
						if (e)
							return exprSIZEOF(e);
					}
					return NULL;
				}
				return exprSIZEOFT(t);
			}
			Expr *e;
			F(e = parse_postfix_expr_post(p, parse_parentheses_post(p), 4));
			return exprSIZEOF(e);
		} else {
			Expr *e;
			F(e = parse_unary_expr(p));
			return exprSIZEOF(e);
		}
	case TOK_ALIGNOF: {
		N; if (match(p, '(')) {
			Type *t = parse_type(p);
			if (t) {
				F(match(p, ')'));
				return exprALIGNOF(t);
			} else {
				Expr *e;
				F(e = parse_postfix_expr_post(p, parse_parentheses_post(p), 4));
				return exprALIGNOF(typeTYPEOF(e, 0));
			}
		} else {
			Expr *e;
			F(e = parse_unary_expr(p));
			return exprALIGNOF(typeTYPEOF(e, 0));
		}
	}
	case TOK_AND:
		N; if (P == TOK_IDENT) {
			return applyUOP(EXPR_OP_ADDROFLABEL, exprIDENT(get_and_next(p)));
		}
		return NULL;
	}
	return NULL;
}

static Expr *parse_cast_expr(Parser *p)
{
	match(p, TOK_EXTENSION);
	if (match(p, '(')) {
		Type *t = parse_type(p);
		if (t) {
			F(match(p, ')'));
			Expr *e;
			if (P == '{') {
				F(e = parse_initializer(p));
			} else {
				F(e = parse_cast_expr(p));
			}
			return parse_postfix_expr_post(p, exprCAST(t, e), 4);
		}
		Expr *e = parse_parentheses_post(p);
		return parse_postfix_expr_post(p, e, 4);
	}

	return parse_unary_expr(p);
}

static Expr *applyBOP(ExprBinOp op, Expr *a, Expr *b)
{
	if (b) {
		return exprBOP(op, a, b);
	} else {
		return NULL;
	}
}

static Expr *parse_expr_post(Parser *p, Expr *e, int prec)
{
	if (prec < 17)
		return e;

	while (e) {
		switch (P) {
		case ',':
			N; e = applyBOP(EXPR_OP_COMMA, e, parse_assignment_expr(p)); break;
		default:
			return e;
		}
	}
	return e;
}

static Expr *parse_assignment_expr_post(Parser *p, Expr *e, int prec)
{
	if (prec < 16)
		return e;

	if (!e)
		return NULL;

	switch (P) {
	case '=':
		N; e = applyBOP(EXPR_OP_ASSIGN, e, parse_assignment_expr(p)); break;
	case TOK_ASSIGNMUL:
		N; e = applyBOP(EXPR_OP_ASSIGNMUL, e, parse_assignment_expr(p)); break;
	case TOK_ASSIGNDIV:
		N; e = applyBOP(EXPR_OP_ASSIGNDIV, e, parse_assignment_expr(p)); break;
	case TOK_ASSIGNMOD:
		N; e = applyBOP(EXPR_OP_ASSIGNMOD, e, parse_assignment_expr(p)); break;
	case TOK_ASSIGNADD:
		N; e = applyBOP(EXPR_OP_ASSIGNADD, e, parse_assignment_expr(p)); break;
	case TOK_ASSIGNSUB:
		N; e = applyBOP(EXPR_OP_ASSIGNSUB, e, parse_assignment_expr(p)); break;
	case TOK_ASSIGNBSHL:
		N; e = applyBOP(EXPR_OP_ASSIGNBSHL, e, parse_assignment_expr(p)); break;
	case TOK_ASSIGNBSHR:
		N; e = applyBOP(EXPR_OP_ASSIGNBSHR, e, parse_assignment_expr(p)); break;
	case TOK_ASSIGNBAND:
		N; e = applyBOP(EXPR_OP_ASSIGNBAND, e, parse_assignment_expr(p)); break;
	case TOK_ASSIGNBXOR:
		N; e = applyBOP(EXPR_OP_ASSIGNBXOR, e, parse_assignment_expr(p)); break;
	case TOK_ASSIGNBOR:
		N; e = applyBOP(EXPR_OP_ASSIGNBOR, e, parse_assignment_expr(p)); break;
	default:;
	}
	return parse_expr_post(p, e, prec);
}

static Expr *parse_conditional_expr_post(Parser *p, Expr *cond, int prec)
{
	if (prec < 15)
		return cond;

	if (cond) {
		if (match(p, '?')) {
			Expr *e1, *e2;
			if (match(p, ':')) {
				// gcc extension: cond ?: expr
				// CAN NOT CONVERT TO: cond ? cond : expr
				e1 = NULL;
			} else {
				F(e1 = parse_expr(p));
				F(match(p, ':'));
			}
			F(e2 = parse_conditional_expr(p));
			return parse_assignment_expr_post(p, exprCOND(cond, e1, e2), prec);
		} else {
			return parse_assignment_expr_post(p, cond, prec);
		}
	}
	return NULL;
}

static Expr *parse_or_expr_post(Parser *p, Expr *e, int prec)
{
	if (prec < 14)
		return e;

	while (e) {
		switch (P) {
		case TOK_OR:
			N; e = applyBOP(EXPR_OP_OR, e, parse_and_expr(p)); break;
		default:
			return parse_conditional_expr_post(p, e, prec);
		}
	}
	return e;
}

static Expr *parse_and_expr_post(Parser *p, Expr *e, int prec)
{
	if (prec < 13)
		return e;

	while (e) {
		switch (P) {
		case TOK_AND:
			N; e = applyBOP(EXPR_OP_AND, e, parse_bor_expr(p)); break;
		default:
			return parse_or_expr_post(p, e, prec);
		}
	}
	return e;
}

static Expr *parse_bor_expr_post(Parser *p, Expr *e, int prec)
{
	if (prec < 12)
		return e;

	while (e) {
		switch (P) {
		case '|':
			N; e = applyBOP(EXPR_OP_BOR, e, parse_bxor_expr(p)); break;
		default:
			return parse_and_expr_post(p, e, prec);
		}
	}
	return e;
}

static Expr *parse_bxor_expr_post(Parser *p, Expr *e, int prec)
{
	if (prec < 11)
		return e;

	while (e) {
		switch (P) {
		case '^':
			N; e = applyBOP(EXPR_OP_BXOR, e, parse_band_expr(p)); break;
		default:
			return parse_bor_expr_post(p, e, prec);
		}
	}
	return e;
}

static Expr *parse_band_expr_post(Parser *p, Expr *e, int prec)
{
	if (prec < 10)
		return e;

	while (e) {
		switch (P) {
		case '&':
			N; e = applyBOP(EXPR_OP_BAND, e, parse_equality_expr(p)); break;
		default:
			return parse_bxor_expr_post(p, e, prec);
		}
	}
	return e;
}

static Expr *parse_equality_expr_post(Parser *p, Expr *e, int prec)
{
	if (prec < 9)
		return e;

	while (e) {
		switch (P) {
		case TOK_EQ:
			N; e = applyBOP(EXPR_OP_EQ, e, parse_relational_expr(p)); break;
		case TOK_NEQ:
			N; e = applyBOP(EXPR_OP_NEQ, e, parse_relational_expr(p)); break;
		default:
			return parse_band_expr_post(p, e, prec);
		}
	}
	return e;
}

static Expr *parse_relational_expr_post(Parser *p, Expr *e, int prec)
{
	if (prec < 8)
		return e;

	while (e) {
		switch (P) {
		case '<':
			N; e = applyBOP(EXPR_OP_LT, e, parse_shift_expr(p)); break;
		case '>':
			N; e = applyBOP(EXPR_OP_GT, e, parse_shift_expr(p)); break;
		case TOK_LE:
			N; e = applyBOP(EXPR_OP_LE, e, parse_shift_expr(p)); break;
		case TOK_GE:
			N; e = applyBOP(EXPR_OP_GE, e, parse_shift_expr(p)); break;
		default:
			return parse_equality_expr_post(p, e, prec);
		}
	}
	return e;
}

static Expr *parse_shift_expr_post(Parser *p, Expr *e, int prec)
{
	if (prec < 7)
		return e;

	while (e) {
		switch (P) {
		case TOK_BSHL:
			N; e = applyBOP(EXPR_OP_BSHL, e, parse_additive_expr(p)); break;
		case TOK_BSHR:
			N; e = applyBOP(EXPR_OP_BSHR, e, parse_additive_expr(p)); break;
		default:
			return parse_relational_expr_post(p, e, prec);
		}
	}
	return e;
}

static Expr *parse_additive_expr_post(Parser *p, Expr *e, int prec)
{
	if (prec < 6)
		return e;

	while (e) {
		switch (P) {
		case '+':
			N; e = applyBOP(EXPR_OP_ADD, e, parse_multiplicative_expr(p)); break;
		case '-':
			N; e = applyBOP(EXPR_OP_SUB, e, parse_multiplicative_expr(p)); break;
		default:
			return parse_shift_expr_post(p, e, prec);
		}
	}
	return e;
}

static Expr *parse_multiplicative_expr_post(Parser *p, Expr *e, int prec)
{
	if (prec < 5)
		return e;

	while (e) {
		switch (P) {
		case '*':
			N; e = applyBOP(EXPR_OP_MUL, e, parse_cast_expr(p)); break;
		case '/':
			N; e = applyBOP(EXPR_OP_DIV, e, parse_cast_expr(p)); break;
		case '%':
			N; e = applyBOP(EXPR_OP_MOD, e, parse_cast_expr(p)); break;
		default:
			return parse_additive_expr_post(p, e, prec);
		}
	}
	return e;
}

static Expr *parse_multiplicative_expr(Parser *p)
{
	return parse_multiplicative_expr_post(p, parse_cast_expr(p), 5);
}

static Expr *parse_additive_expr(Parser *p)
{
	return parse_additive_expr_post(p, parse_multiplicative_expr(p), 6);
}

static Expr *parse_shift_expr(Parser *p)
{
	return parse_shift_expr_post(p, parse_additive_expr(p), 7);
}

static Expr *parse_relational_expr(Parser *p)
{
	return parse_relational_expr_post(p, parse_shift_expr(p), 8);
}

static Expr *parse_equality_expr(Parser *p)
{
	return parse_equality_expr_post(p, parse_relational_expr(p), 9);
}

static Expr *parse_band_expr(Parser *p)
{
	return parse_band_expr_post(p, parse_equality_expr(p), 10);
}

static Expr *parse_bxor_expr(Parser *p)
{
	return parse_bxor_expr_post(p, parse_band_expr(p), 11);
}

static Expr *parse_bor_expr(Parser *p)
{
	return parse_bor_expr_post(p, parse_bxor_expr(p), 12);
}

static Expr *parse_and_expr(Parser *p)
{
	return parse_and_expr_post(p, parse_bor_expr(p), 13);
}

static Expr *parse_or_expr(Parser *p)
{
	return parse_or_expr_post(p, parse_and_expr(p), 14);
}

static Expr *parse_conditional_expr(Parser *p)
{
	return parse_conditional_expr_post(p, parse_or_expr(p), 15);
}

static Expr *parse_assignment_expr(Parser *p)
{
	return parse_assignment_expr_post(p, parse_conditional_expr(p), 16);
}

typedef struct {
	bool is_typedef;
	unsigned int flags;
	Type *type;
	const char *ident;
	StmtBLOCK *funargs;
	struct scope_item *funscope;
	Attribute *attrs;
	Expr *c11_alignas;
	bool old_fundecl;
} Declarator;

static void init_declarator(Declarator *pd)
{
	pd->is_typedef = false;
	pd->flags = 0;;
	pd->type = NULL;
	pd->ident = NULL;
	pd->funargs = NULL;
	pd->funscope = NULL;
	pd->attrs = NULL;
	pd->c11_alignas = NULL;
	pd->old_fundecl = false;
}

static void free_funscope(Declarator *pd)
{
	if (pd->funscope) {
		free_scope(pd->funscope);
		pd->funscope = NULL;
	}
}

static void fix_type(Declarator *d, Type *btype)
{
	Type *tnew = btype;
	Type *told = d->type;
	while (told) {
		switch (told->type) {
		case TYPE_PTR: {
			TypePTR *n = (TypePTR *) told;
			told = n->t;
			n->t = tnew;
			tnew = &n->h;
			break;
		}
		case TYPE_ARRAY: {
			TypeARRAY *n = (TypeARRAY *) told;
			told = n->t;
			n->t = tnew;
			tnew = &n->h;
			break;
		}
		case TYPE_FUN: {
			TypeFUN *n = (TypeFUN *) told;
			told = n->rt;
			n->rt = tnew;
			tnew = &n->h;
			break;
		}
		default:
			assert(false);
		}
	}
	d->type = tnew;
}

#include "parser-legacy.inc"

static unsigned int parse_type_qualifier(Parser *p)
{
	unsigned int flags = 0;
	bool progress = true;
	while (progress) {
		progress = false;
		if (match(p, TOK_CONST)) {
			flags |= TFLAG_CONST;
			progress = true;
		}
		if (match(p, TOK_VOLATILE)) {
			flags |= TFLAG_VOLATILE;
			progress = true;
		}
		if (match(p, TOK_RESTRICT)) {
			flags |= TFLAG_RESTRICT;
			progress = true;
		}
		if (match(p, TOK_ATOMIC)) {
			flags |= TFLAG_ATOMIC;
			progress = true;
		}
	}
	return flags;
}

static bool parse_attribute(Parser *p, Attribute **pa);
static Declarator parse_type1(Parser *p, Type **pbtype, Attribute *attrs, bool implicit_int);
static int parse_declarator0(Parser *p, Declarator *d)
{
	F(parse_attribute(p, &d->attrs));
	bool flag = false;
	if (match(p, '*')) {
		unsigned int flags = parse_type_qualifier(p);
		F(parse_declarator0(p, d));
		d->type = typePTR(d->type, flags);
		return 1;
	}
	if (P == TOK_IDENT) {
		F(d->ident == NULL);
		d->ident = get_and_next(p);
	} else if (match(p, '(')) {
		if (P == '*' || P == '(' || P == '[' || P == TOK_ATTRIBUTE ||
		    (P == TOK_IDENT &&
		     (d->is_typedef || symlookup(p, PS) != SYM_TYPE))) {
			F(parse_declarator0(p, d));
			F(match(p, ')'));
		} else {
			flag = true;
		}
	}

	while (1) {
		if (!flag && match(p, '[')) {
			unsigned int flags = 0;
			Expr *e = NULL;
			bool is_static = false;
			if (match(p, '*')) {
				if(!match(p, ']')) {
					F(e = parse_multiplicative_expr_post(
						  p, applyUOP(EXPR_OP_DEREF, parse_cast_expr(p)), 16));
					F(match(p, ']'));
				}
			} else {
				is_static = match(p, TOK_STATIC);
				flags = parse_type_qualifier(p);
				if (!is_static)
					match(p, TOK_STATIC);
				if (!match(p, ']')) {
					F(e = parse_assignment_expr(p));
					F(match(p, ']'));
				}
			}
			flags |= is_static ? TFLAG_ARRAY_STATIC : 0;
			d->type = typeARRAY(d->type, e, flags);
		} else if (flag || match(p, '(')) {
			flag = false;
			TypeFUN *n = typeFUN(d->type);
			if (match(p, ')')) {
				d->type = &n->h;
				d->old_fundecl = true;
			} else {
				int ret = parse_oldfunargs1(p, d, n);
				F(ret);
				if (ret > 0)
					return 1;
				enter_scope(p);
				StmtBLOCK *funargs = d->funargs ? NULL : stmtBLOCK();
				Declarator d1 = parse_type1(p, NULL, NULL, false);
				F(parse_attribute(p, &d1.attrs));
				free_funscope(&d1);
				if (d1.type && d1.type->type == TYPE_VOID &&
				    d1.ident == NULL && P == ')') {
					N;
					d1.type = NULL;
					d->type = &n->h;
					leave_scope(p);
					continue;
				}
				typeFUN_append(n, d1.type);
				if (funargs) {
					stmtBLOCK_append(funargs, stmtVARDECL(d1.flags, d1.ident, d1.type, NULL, NULL,
									      (Extension) {
										      .gcc_attribute = d1.attrs,
									      }));
					if (d1.ident) {
						F(symset(p, d1.ident, SYM_IDENT), leave_scope(p));
					}
				}
				while (match(p, ',')) {
					if (match(p, TOK_DOT3)) {
						n->va_arg = true;
						break;
					}
					d1 = parse_type1(p, NULL, NULL, false);
					F(parse_attribute(p, &d1.attrs));
					free_funscope(&d1);
					typeFUN_append(n, d1.type);
					if (funargs) {
						stmtBLOCK_append(funargs, stmtVARDECL(d1.flags, d1.ident, d1.type, NULL, NULL,
										      (Extension) {
											      .gcc_attribute = d1.attrs,
										      }));
						if (d1.ident) {
							F(symset(p, d1.ident, SYM_IDENT), leave_scope(p));
						}
					}
				}
				F(match(p, ')'), leave_scope(p));
				d->type = &n->h;
				if (funargs) {
					d->funargs = funargs;
					assert (d->funscope == NULL);
					d->funscope = get_scope(p);
				} else {
					leave_scope(p);
				}
			}
		} else {
			break;
		}
	}
	return 1;
}

static int parse_declarator(Parser *p, Declarator *d)
{
	Type *btype = d->type;
	d->type = NULL;
	F(parse_declarator0(p, d));
	fix_type(d, btype);
	return 1;
}

static struct EnumPair_ parse_enum_pair(Parser *p)
{
	struct EnumPair_ ret = {NULL, NULL};
	if (P == TOK_IDENT) {
		if (!symset(p, PS, SYM_IDENT)) {
			return ret;
		}
		ret.id = get_and_next(p);
		if (match(p, '=')) {
			ret.val = parse_assignment_expr(p);
			if (!ret.val) {
				ret.id = NULL;
			}
		}
	}
	return ret;
}

static StmtBLOCK *parse_decls(Parser *p, bool in_struct, bool implicit_int);
static void type_set_tflags(Type *t, unsigned int tflags)
{
	switch (t->type) {
	case TYPE_VOID: (((TypeVOID *) t)->flags) |= tflags; break;
	case TYPE_PRIM: (((TypePRIM *) t)->flags) |= tflags; break;
	case TYPE_PTR: (((TypePTR *) t)->flags) |= tflags; break;
	case TYPE_ARRAY: (((TypeARRAY *) t)->flags) |= tflags; break;
	case TYPE_FUN: break;
	case TYPE_TYPEDEF: (((TypeTYPEDEF *) t)->flags) |= tflags; break;
	case TYPE_STRUCT:  (((TypeSTRUCT *) t)->flags) |= tflags; break;
	case TYPE_ENUM: (((TypeENUM *) t)->flags) |= tflags; break;
	case TYPE_TYPEOF: (((TypeTYPEOF *) t)->flags) |= tflags; break;
	case TYPE_AUTO: (((TypeAUTO *) t)->flags) |= tflags; break;
	default:
		assert(false);
		break;
	}
}

static bool parse_type1_(Parser *p, Type **pbtype, Declarator *pd, bool implicit_int)
{
	unsigned int tflags = 0;

	int is_signed = 0;
	int is_unsigned = 0;
	int is_short = 0;
	int long_count = 0;

	int is_int = 0;
	int is_bool = 0;
	int is_char = 0;
	int is_float = 0;
	int is_double = 0;
	int is_void = 0;
	int is_int128 = 0;

	bool flag = true;
	int old_count = p->next_count;
	while (flag) {
		F_(parse_attribute(p, &pd->attrs), false);
		switch (P) {
		// storage-class-specifier
		case TOK_TYPEDEF:
			N; pd->is_typedef = true; break;
		case TOK_EXTERN:
			N; pd->flags |= DFLAG_EXTERN; break;
		case TOK_STATIC:
			N; pd->flags |= DFLAG_STATIC; break;
		case TOK_AUTO: // ignore
			N; break;
		case TOK_REGISTER:
			N; pd->flags |= DFLAG_REGISTER; break;
		case TOK_THREADLOCAL:
			N; pd->flags |= DFLAG_THREADLOCAL; break;
		case TOK_ALIGNAS: {
			N;
			F_(match(p, '('), false);
			Type *t = parse_type(p);
			if (t) {
				pd->c11_alignas = exprALIGNOF(t);
			} else {
				Expr *e;
				F_(pd->c11_alignas = parse_conditional_expr(p), false);
			}
			F_(match(p, ')'), false);
			break;
		}
		// type-qualifier
		case TOK_CONST:
			N; tflags |= TFLAG_CONST; break;
		case TOK_VOLATILE:
			N; tflags |= TFLAG_VOLATILE; break;
		case TOK_RESTRICT:
			N; tflags |= TFLAG_RESTRICT; break;
		case TOK_ATOMIC: {
			N; int xcount = is_int + is_bool + is_char +
				is_float + is_double + is_void +
				is_int128 +
				is_signed + is_unsigned + is_short + long_count;
			if (pd->type == NULL && xcount == 0 && match(p, '(')) {
				Type *atype;
				F_(atype = parse_type(p), false);
				F_(match(p, ')'), false);
				tflags |= TFLAG_ATOMIC;
				tflags |= parse_type_qualifier(p);
				type_set_tflags(atype, tflags);
				pd->type = atype;
			} else {
				tflags |= TFLAG_ATOMIC;
			}
			break;
		}
		// function-specifier
		case TOK_INLINE:
			N; pd->flags |= DFLAG_INLINE; break;
		case TOK_NORETURN:
			N; pd->flags |= DFLAG_NORETURN; break;
		// type-specifier
		case TOK_SIGNED:
			N; is_signed = 1; break;
		case TOK_UNSIGNED:
			N; is_unsigned = 1; break;
		case TOK_VOID:
			N; is_void = 1; break;
		case TOK_INT:
			N; is_int = 1; break;
		case TOK_LONG:
			N; long_count++; break;
		case TOK_SHORT:
			N; is_short = 1; break;
		case TOK_CHAR:
			N; is_char = 1; break;
		case TOK_BOOL:
			N; is_bool = 1; break;
		case TOK_FLOAT:
			N; is_float = 1; break;
		case TOK_DOUBLE:
			N; is_double = 1; break;
		case TOK_COMPLEX:
			N; tflags |= TFLAG_COMPLEX; break;
		case TOK_IMAGINARY:
			N; tflags |= TFLAG_IMAGINARY; break;
		case TOK_INT128:
			N; is_int128 = 1; break;
		case TOK_IDENT: {
			int sv = symlookup(p, PS);
			if (sv == SYM_TYPE) {
				int xcount = is_int + is_bool + is_char + is_float + is_double + is_void +
					is_int128 + (tflags & (TFLAG_COMPLEX | TFLAG_IMAGINARY)) +
					is_signed + is_unsigned + is_short + long_count;
				if (pd->type == NULL && xcount == 0) {
					const char *name = get_and_next(p);
					tflags |= parse_type_qualifier(p);
					pd->type = typeTYPEDEF(name, tflags);
					break;
				}
			}
			flag = false;
			break;
		}
		case TOK_STRUCT:
		case TOK_UNION:
		{
			Attribute *attrs = NULL;
			bool is_union = P == TOK_UNION;
			N; F_(pd->type == NULL, false);
			const char *tag = NULL;
			StmtBLOCK *decls = NULL;
			F_(parse_attribute(p, &attrs), false);
			if (P == TOK_IDENT) {
				tag = get_and_next(p);
			}
			if (match(p, '{')) {
				enter_scope(p);
				int old_count = p->next_count;
				decls = parse_decls(p, true, false);
				int new_count = p->next_count;
				leave_scope(p);
				if (old_count != new_count && decls == NULL)
					return false;
				F_(match(p, '}'), false);
				F_(parse_attribute(p, &attrs), false); // ambiguous
			}
			tflags |= parse_type_qualifier(p);
			pd->type = typeSTRUCT(is_union, tag, decls, tflags, attrs);
			break;
		}
		case TOK_ENUM:
		{
			Attribute *attrs = NULL;
			N; F_(pd->type == NULL, false);
			const char *tag = NULL;
			StmtBLOCK *decls = NULL;
			F_(parse_attribute(p, &attrs), false);
			if (P == TOK_IDENT) {
				tag = get_and_next(p);
			}

			EnumList *list = NULL;
			if (match(p, '{')) {
				list = __new(EnumList);
				avec_init(&(list->items));
				struct EnumPair_ pair = parse_enum_pair(p);
				if (pair.id) {
					avec_push(&(list->items), pair);
					while (match(p, ',')) {
						pair = parse_enum_pair(p);
						if (pair.id == NULL)
							break;
						avec_push(&(list->items), pair);
					}
				}
				if (match(p, '}')) {
					F_(parse_attribute(p, &attrs), false); // ambiguous
					tflags |= parse_type_qualifier(p);
					pd->type = typeENUM(tag, list, tflags, attrs);
					break;
				}
				return false;
			}
			tflags |= parse_type_qualifier(p);
			pd->type = typeENUM(tag, list, tflags, attrs);
			break;
		}
		case TOK_TYPEOF: {
			N; Expr *e;
			F_(match(p, '('), false);
			Type *t = parse_type(p);
			if (t) {
				F_(match(p, ')'), false);
				pd->type = t;
				break;
			}
			F_(e = parse_expr(p), false);
			F_(match(p, ')'), false);
			tflags |= parse_type_qualifier(p);
			pd->type = typeTYPEOF(e, tflags);
			break;
		}
		case TOK_AUTOTYPE: {
			N; tflags |= parse_type_qualifier(p);
			pd->type = typeAUTO(tflags);
			break;
		}
		default:
			flag = false;
			break;
		}
	}
	F_(!((tflags & TFLAG_COMPLEX) && (tflags & TFLAG_IMAGINARY)), false);

	int tcount = is_int + is_bool + is_char + is_float + is_double + is_void + is_int128;
	int scount = is_signed + is_unsigned;
	if (pd->type) {
		if (tcount + scount + is_short + long_count || (tflags & (TFLAG_COMPLEX | TFLAG_IMAGINARY))) {
			pd->type = NULL;
		}
	} else {
		if (tcount == 0 || tcount == 1 && is_int) {
			if (is_short) {
				if (long_count == 0)
					pd->type = is_unsigned ? typePRIM(PT_USHORT, tflags) : typePRIM(PT_SHORT, tflags);
			} else if (long_count == 0) {
				if (tcount || scount)
					pd->type = is_unsigned ? typePRIM(PT_UINT, tflags) : typePRIM(PT_INT, tflags);
				else if (tflags & (TFLAG_COMPLEX | TFLAG_IMAGINARY))
					pd->type = typePRIM(PT_DOUBLE, tflags);
				else if (implicit_int || p->next_count != old_count)
					pd->type = typePRIM(PT_INT, tflags);
			} else if (long_count == 1) {
				pd->type = is_unsigned ? typePRIM(PT_ULONG, tflags) : typePRIM(PT_LONG, tflags);
			} else if (long_count == 2) {
				pd->type = is_unsigned ? typePRIM(PT_ULLONG, tflags) : typePRIM(PT_LLONG, tflags);
			}
		} else if (tcount == 1) {
			if (is_short + long_count == 0) {
				if (is_char && !(tflags & (TFLAG_COMPLEX | TFLAG_IMAGINARY))) {
					if (is_signed)
						pd->type = typePRIM(PT_SCHAR, tflags);
					else if (is_unsigned)
						pd->type = typePRIM(PT_UCHAR, tflags);
					else
						pd->type = typePRIM(PT_CHAR, tflags);
				} else if (is_int128) {
					pd->type = is_unsigned ? typePRIM(PT_UINT128, tflags) : typePRIM(PT_INT128, tflags);
				} else if (scount == 0) {
					if (is_bool && !(tflags & (TFLAG_COMPLEX | TFLAG_IMAGINARY)))
						pd->type = typePRIM(PT_BOOL, tflags);
					else if (is_float)
						pd->type = typePRIM(PT_FLOAT, tflags);
					else if (is_double)
						pd->type = typePRIM(PT_DOUBLE, tflags);
					else if (is_void)
						pd->type = typeVOID(tflags);
				}
			} else {
				if (is_double && long_count == 1 && !is_short)
					pd->type = typePRIM(PT_LDOUBLE, tflags);
			}
		}
	}
	F_(pd->type, false);
	if (pbtype)
		*pbtype = pd->type;
	F_(parse_declarator(p, pd), false);
	return true;
}

static Declarator parse_type1(Parser *p, Type **pbtype, Attribute *attrs, bool implicit_int)
{
	Declarator d, err;
	init_declarator(&d);
	init_declarator(&err);
	d.attrs = attrs;
	if (parse_type1_(p, pbtype, &d, implicit_int))
		return d;

	free_funscope(&d);
	return err;
}

static bool parse_attribute1(Parser *p, Attribute *a)
{
	if (match(p, '(')) {
		if (!match(p, ')')) {
			Expr *arg;
			F_(arg = parse_assignment_expr(p), false);
			avec_push(&a->args, arg);
			while (match(p, ',')) {
				F_(arg = parse_assignment_expr(p), false);
				avec_push(&a->args, arg);
			}
			F_(match(p, ')'), false);
		}
	}
	return true;
}

static bool parse_attribute(Parser *p, Attribute **pa)
{
	while (match(p, TOK_ATTRIBUTE)) {
		F_(match(p, '('), false); F_(match(p, '('), false);
		bool flag = true;
		while (flag) {
			int tok_type = P;
			if (tok_type == TOK_IDENT ||
			    (tok_type > TOK_KEYWORD_START &&
			     tok_type < TOK_KEYWORD_END)) {
				Attribute *a = __new(Attribute);
				a->name = get_and_next(p);
				avec_init(&a->args);
				a->next = *pa;
				*pa = a;
				F_(parse_attribute1(p, a), false);
			} else if (tok_type == ',') {
				N;
			} else if (tok_type == ')') {
				flag = false;
			} else {
				return false;
			}
		}
		F_(match(p, ')'), false); F_(match(p, ')'), false);
	}
	return true;
}

static StmtBLOCK *parse_stmts(Parser *p)
{
	StmtBLOCK *block = stmtBLOCK();
	while (P != '}' && P != TOK_END) {
		Stmt *s;
		F(s = parse_stmt(p));
		stmtBLOCK_append(block, s);
	}
	return block;
}

static StmtBLOCK *parse_block_stmt(Parser *p)
{
	StmtBLOCK *block;
	F(match(p, '{'));
	enter_scope(p);
	F(block = parse_stmts(p), leave_scope(p));
	leave_scope(p);
	F(match(p, '}'));
	return block;
}

static bool parse_decl_(Parser *p, Stmt **pstmt, bool in_struct, bool in_for99, bool implicit_int);
static StmtBLOCK *parse_decls(Parser *p, bool in_struct, bool implicit_int)
{
	StmtBLOCK *block = stmtBLOCK();
	while (P != '}' && P != TOK_END) {
		Stmt *s;
		F(parse_decl_(p, &s, in_struct, false, implicit_int) && s);
		stmtBLOCK_append(block, s);
	}
	return block;
}

static bool match_initialzer_item(Parser *p, ExprINIT *init)
{
	Designator *d1 = NULL;
	while (true) {
		Designator *n;
		if (match(p, '[')) {
			n = __new(Designator);
			n->type = DES_INDEX;
			F(n->index = parse_expr(p));
			if (match(p, TOK_DOT3)) {
				F(n->indexhigh = parse_expr(p));
				n->type = DES_INDEXRANGE;
			}
			F(match(p, ']'));
		} else if (match(p, '.')) {
			n = __new(Designator);
			n->type = DES_FIELD;
			F(P == TOK_IDENT);
			n->field = get_and_next(p);
		} else {
			break;
		}
		n->next = d1;
		d1 = n;
	}
	if (d1) {
		F(match(p, '='));
	}
	Designator *d = NULL;
	while (d1) {
		Designator *d1next = d1->next;
		d1->next = d;
		d = d1;
		d1 = d1next;
	}
	Expr *e;
	F(e = parse_initializer(p));
	exprINIT_append(init, d, e);
	return true;
}

static Expr *parse_initializer(Parser *p)
{
	if (match(p, '{')) {
		ExprINIT *init = exprINIT();
		if (match(p, '}')) {
			return (Expr *) init;
		}

		F(match_initialzer_item(p, init));
		while (match(p, ',')) {
			if (!match_initialzer_item(p, init))
				break;
		}

		if (match(p, '}')) {
			return (Expr *) init;
		}

		return NULL;
	}
	return parse_assignment_expr(p);
}

static bool parse_asm_name(Parser *p, const char **name)
{
	if (match(p, TOK_ASM)) {
		F_(match(p, '('), false);
		F_(P == TOK_STRING_CST, false);
		int len = PSL;
		char *str = __new_(len);
		memcpy(str, PS, len);
		*name = str;
		N;
		F_(match(p, ')'), false);
	}
	return true;
}

static Stmt *make_decl(Parser *p, Declarator d, bool check)
{
	Stmt *decl1;
	Extension ext = (Extension) {};
	if (d.type->type == TYPE_FUN) {
		if (p->managed_count)
			d.flags |= DFLAG_MANAGED;
		F(parse_asm_name(p, &ext.gcc_asm_name));
		ext.gcc_attribute = d.attrs;
		F(parse_attribute(p, &ext.gcc_attribute));
		decl1 = stmtFUNDECL(d.flags, d.ident, (TypeFUN *) d.type, d.funargs, NULL, ext);
	} else {
		Expr *bitfield = NULL;
		if (match(p, ':')) {
			F(bitfield = parse_conditional_expr(p));
		} else {
			F(parse_asm_name(p, &ext.gcc_asm_name));
		}
		ext.gcc_attribute = d.attrs;
		F(parse_attribute(p, &ext.gcc_attribute));
		ext.c11_alignas = d.c11_alignas;
		Expr *init = NULL;
		if (match(p, '=')) {
			F(init = parse_initializer(p));
		}
		if (check)
			F(bitfield || d.ident);
		decl1 = stmtVARDECL(d.flags, d.ident, d.type, init, bitfield, ext);
	}
	return decl1;
}

Stmt *parse_decl0(Parser *p, Declarator d, Type *btype, bool in_struct)
{
	F(d.type);

	bool is_typedef = d.is_typedef;
	unsigned int symtype = is_typedef ? SYM_TYPE : SYM_IDENT;
	if (d.ident) {
		if (!in_struct || symtype == SYM_TYPE) {
			if (!symset(p, d.ident, symtype)) {
				return NULL;
			}
		}
	}

	if (d.type->type == TYPE_FUN && d.old_fundecl)
		F(parse_oldfunargs2(p, &d));

	if (d.type->type == TYPE_FUN && P == '{') {
		F(!is_typedef);
		Attribute *attrs = d.attrs;
		F(parse_attribute(p, &attrs));
		StmtBLOCK *b;
		restore_scope(p, d.funscope);
		b = parse_block_stmt(p);
		leave_scope(p);
		F(b);
		if (p->managed_count)
			d.flags |= DFLAG_MANAGED;
		return stmtFUNDECL(d.flags, d.ident, (TypeFUN *) d.type, d.funargs, b,
				   (Extension) {.gcc_attribute = attrs});
	}

	Stmt *decl1;
	if (is_typedef) {
		Attribute *attrs = d.attrs;
		F(parse_attribute(p, &attrs));
		decl1 = stmtTYPEDEF(d.ident, d.type,
				    (Extension) {.gcc_attribute = attrs});
	} else {
		F(decl1 = make_decl(p, d, false), free_funscope(&d));
	}

	free_funscope(&d);

	if (match(p, ';')) {
		return decl1;
	} else {
		StmtDECLS *decls = stmtDECLS();
		if (btype->type == TYPE_STRUCT) {
			TypeSTRUCT *b = (TypeSTRUCT *) btype;
			if (b->decls) {
				if (!b->tag) {
					char buf[64];
					sprintf(buf, "__anon_struct%d", p->counter++);
					b->tag = __new_cstring(buf);
				}
				Type *ntype = typeSTRUCT(b->is_union, b->tag, b->decls, 0, b->attrs);
				b->decls = NULL;
				stmtDECLS_append(decls, stmtVARDECL(0, NULL, ntype, NULL, NULL,
								    (Extension) {}));
			}
		} else if (btype->type == TYPE_ENUM) {
			TypeENUM *b = (TypeENUM *) btype;
			if (b->list) {
				if (!b->tag) {
					char buf[64];
					sprintf(buf, "__anon_enum%d", p->counter++);
					b->tag = __new_cstring(buf);
				}
				Type *ntype = typeENUM(b->tag, b->list, 0, b->attrs);
				b->list = NULL;
				stmtDECLS_append(decls, stmtVARDECL(0, NULL, ntype, NULL, NULL,
								    (Extension) {}));
			}
		}
		stmtDECLS_append(decls, decl1);
		while (match(p, ',')) {
			Declarator dd = d;
			dd.type = btype;
			dd.ident = NULL;
			dd.funargs = NULL;
			parse_attribute(p, &dd.attrs);
			parse_declarator(p, &dd);
			if (dd.ident && !symset(p, dd.ident, symtype)) {
				return NULL;
			}

			if (is_typedef) {
				Attribute *attrs = dd.attrs;
				F(parse_attribute(p, &attrs));
				F(decl1 = stmtTYPEDEF(dd.ident, dd.type,
						      (Extension) {
							      .gcc_attribute = attrs,
						      }),
				  free_funscope(&dd));
			} else {
				F(decl1 = make_decl(p, dd, true), free_funscope(&dd));
			}
			free_funscope(&dd);
			stmtDECLS_append(decls, decl1);
		}
		if (match(p, ';')) {
			return (Stmt *) decls;
		}
	}
	return NULL;
}

static Declarator parse_type1_ident_post(Parser *p, const char *id, Type **pbtype)
{
	Declarator d, err;
	init_declarator(&d);
	init_declarator(&err);
	int sv = symlookup(p, id);
	unsigned int tflags = 0;
	if (sv == SYM_TYPE) {
		tflags |= parse_type_qualifier(p);
		d.type = typeTYPEDEF(id, tflags);
		if (parse_type1_(p, pbtype, &d, false))
			return d;
	}
	return err;
}

static bool parse_decl_(Parser *p, Stmt **pstmt, bool in_struct, bool in_for99, bool implicit_int)
{
	match(p, TOK_EXTENSION);
	if (match(p, TOK_MANAGED)) {
		F_(match(p, '{'), false);
		p->managed_count++;
		StmtDECLS *decls = stmtDECLS();
		while (P != '}' && P != TOK_END) {
			Stmt *stmt;
			F_(parse_decl_(p, &stmt, in_struct, false, implicit_int) && stmt, false);
			stmtDECLS_append(decls, stmt);
		}
		F_(match(p, '}'), false);
		p->managed_count--;
		if (pstmt)
			*pstmt = (Stmt *) decls;
		return true;
	}
	if (P == TOK_PP_PRAGMA_LINE) {
		const char *line = get_and_next(p);
		if (pstmt)
			*pstmt = (Stmt *) stmtPRAGMA(line);
		return true;
	}
	if (match(p, TOK_STATICASSERT)) {
		F_(match(p, '('), false);
		Expr *expr;
		const char *errmsg = NULL;
		F_(expr = parse_conditional_expr(p), false);
		if (match(p, ',')) {
			F_(P == TOK_STRING_CST, false);
			errmsg = get_and_next(p);
		}
		F_(match(p, ')'), false);
		F_(match(p, ';'), false);
		if (pstmt)
			*pstmt = stmtSTATICASSERT(expr, errmsg);
		return true;
	}
	if (!in_struct) {
		if (P == TOK_ASM) {
			Stmt *s = parse_stmt(p);
			if (s) {
				if (pstmt)
					*pstmt = s;
				return true;
			} else {
				return false;
			}
		}
	}
	Type *btype;
	Attribute *attrs = NULL;
	int old_count0 = p->next_count;
	F_(parse_attribute(p, &attrs), false);
	if (!in_for99 && (match(p, ';'))) {
		if (pstmt)
			*pstmt = stmtSKIP(attrs);
		return true;
	}
	int old_count = p->next_count;
	Declarator d = parse_type1(p, &btype, attrs, implicit_int || (old_count0 != old_count));
	Stmt *res = parse_decl0(p, d, btype, in_struct);
	int new_count = p->next_count;
	if (pstmt)
		*pstmt = res;
	return old_count != new_count;
}

bool parse_decl(Parser *p, Stmt **pstmt)
{
	return parse_decl_(p, pstmt, false, false, false);
}

Stmt *parse_stmt(Parser *p)
{
	switch (P) {
	case TOK_IF: {
		N; Expr *e;
		Stmt *s1, *s2;
		F(match(p, '('));
		enter_scope(p);
		F(e = parse_expr(p), leave_scope(p));
		struct scope_item *ifc = dup_scope(p);
		F(match(p, ')'), leave_scope(p));
		F(s1 = parse_stmt(p), leave_scope(p));
		leave_scope(p);
		if (match(p, TOK_ELSE)) {
			restore_scope(p, ifc);
			F(s2 = parse_stmt(p), leave_scope(p));
			leave_scope(p);
			return stmtIF(e, s1, s2);
		}
		free_scope(ifc);
		return stmtIF(e, s1, NULL);
	}
	case TOK_WHILE: {
		N; Expr *e;
		Stmt *s;
		F(match(p, '('));
		enter_scope(p);
		F(e = parse_expr(p), leave_scope(p));
		F(match(p, ')'), leave_scope(p));
		F(s = parse_stmt(p), leave_scope(p));
		leave_scope(p);
		return stmtWHILE(e, s);
	}
	case TOK_DO: {
		N; Expr *e;
		Stmt *s;
		enter_scope(p);
		F(s = parse_stmt(p), leave_scope(p));
		leave_scope(p);
		F(match(p, TOK_WHILE));
		F(match(p, '('));
		enter_scope(p);
		F(e = parse_expr(p), leave_scope(p));
		leave_scope(p);
		F(match(p, ')'));
		F(match(p, ';'));
		return stmtDO(e, s);
	}
	case TOK_FOR: {
		N; Expr *init, *cond, *step;
		Stmt *init99, *body;
		F(match(p, '('));
		enter_scope(p);
		bool is_stmt = parse_decl_(p, &init99, false, true, false);
		if (is_stmt) {
			F((init99 && init99->type == STMT_VARDECL), leave_scope(p));
		} else {
			init = parse_expr(p);
			F(match(p, ';'), leave_scope(p));
		}
		cond = parse_expr(p);
		F(match(p, ';'), leave_scope(p));
		step = parse_expr(p);
		F(match(p, ')'), leave_scope(p));
		F(body = parse_stmt(p), leave_scope(p));
		leave_scope(p);
		if (init99)
			return stmtFOR99((StmtVARDECL *) init99, cond, step, body);
		else
			return stmtFOR(init, cond, step, body);
	}
	case TOK_BREAK: {
		N; F(match(p, ';'));
		return stmtBREAK();
	}
	case TOK_CONTINUE: {
		N; F(match(p, ';'));
		return stmtCONTINUE();
	}
	case TOK_SWITCH: {
		N; Expr *e;
		Stmt *b;
		F(match(p, '('));
		enter_scope(p);
		F(e = parse_expr(p), leave_scope(p));
		F(match(p, ')'), leave_scope(p));
		F(b = parse_stmt(p), leave_scope(p));
		leave_scope(p);
		return stmtSWITCH(e, b);
	}
	case TOK_CASE: {
		N; Expr *e; Stmt *s;
		F(e = parse_expr(p));
		if (match(p, TOK_DOT3)) {
			Expr *eh;
			F(eh = parse_expr(p));
			F(match(p, ':'));
			F(s = parse_stmt(p));
			return stmtCASERANGE(e, eh, s);
		}
		F(match(p, ':'));
		F(s = parse_stmt(p));
		return stmtCASE(e, s);
	}
	case TOK_DEFAULT: {
		N; Stmt *s;
		F(match(p, ':'));
		F(s = parse_stmt(p));
		return stmtDEFAULT(s);
	}
	case TOK_RETURN: {
		N; if (P == ';') {
			N; return stmtRETURN(NULL);
		}
		Expr *e;
		F(e = parse_expr(p));
		F(match(p, ';'));
		return stmtRETURN(e);
	}
	case TOK_GOTO: {
		N; if (P == TOK_IDENT) {
			const char *id = get_and_next(p);
			F(match(p, ';'));
			return stmtGOTO(id);
		} else if (P == '*') {
			Expr *e;
			F(e = parse_expr(p));
			F(match(p, ';'));
			return stmtGOTOADDR(e);
		}
		return NULL;
	}
	case '{': {
		return (Stmt *) parse_block_stmt(p);
	}
	case ';': {
		N; return stmtSKIP(NULL);
	}
	case TOK_LABEL: {
		N; F(P == TOK_IDENT);
		Stmt *l;
		F(l = stmtLABELDECL(get_and_next(p)));
		if (P == ',') {
			StmtDECLS *decls = stmtDECLS();
			stmtDECLS_append(decls, l);
			while(match(p, ',')) {
				F(P == TOK_IDENT);
				F(l = stmtLABELDECL(get_and_next(p)));
				stmtDECLS_append(decls, l);
			}
			F(match(p, ';'));
			l = (Stmt *) decls;
		}
		return l;
	}
	case TOK_ASM: {
		N; unsigned int flags = 0;
		while (true) {
			if (match(p, TOK_VOLATILE)) {
				flags |= ASM_FLAG_VOLATILE;
			} else if (match(p, TOK_INLINE)) {
				flags |= ASM_FLAG_INLINE;
			} else if (match(p, TOK_GOTO)) {
				flags |= ASM_FLAG_GOTO;
			} else {
				break;
			}
		}
		F(match(p, '('));
		F(P == TOK_STRING_CST);
		const char *content = get_and_next(p);
		StmtASM *s = stmtASM(flags, content);
		if (match(p, ':')) {
			if (P == '[' || P == TOK_STRING_CST) {
				do {
					ASMOper oper = (ASMOper) {};
					if (match(p, '[')) {
						F(P == TOK_IDENT);
						oper.symbol = get_and_next(p);
						F(match(p, ']'));
					}
					F(P == TOK_STRING_CST);
					oper.constraint = get_and_next(p);
					F(match(p, '('));
					F(oper.variable = parse_expr(p));
					F(match(p, ')'));
					stmtASM_append_output(s, oper);
				} while (match(p, ','));
			}
			if (match(p, ':')) {
				if (P == '[' || P == TOK_STRING_CST) {
					do {
						ASMOper oper = (ASMOper) {};
						if (match(p, '[')) {
							F(P == TOK_IDENT);
							oper.symbol = get_and_next(p);
							F(match(p, ']'));
						}
						F(P == TOK_STRING_CST);
						oper.constraint = get_and_next(p);
						F(match(p, '('));
						F(oper.variable = parse_expr(p));
						F(match(p, ')'));
						stmtASM_append_input(s, oper);
					} while (match(p, ','));
				}
				if (match(p, ':')) {
					const char *cl;
					if (P == TOK_STRING_CST) {
						do {
							F(P == TOK_STRING_CST);
							cl = get_and_next(p);
							stmtASM_append_clobber(s, cl);
						} while(match(p, ','));
					}
					if (match(p, ':')) {
						const char *cl;
						if (P == TOK_IDENT) {
							do {
								F(P == TOK_IDENT);
								cl = get_and_next(p);
								stmtASM_append_gotolabels(s, cl);
							} while(match(p, ','));
						}
					}
				}
			}
		}
		F(match(p, ')'));
		F(match(p, ';'));
		return (Stmt *) s;
	}
	default: {
		Expr *e;
		if (P == TOK_IDENT) {
			const char *id = get_and_next(p);
			if (match(p, ':')) {
				// a label
				Stmt *s;
				F(s = parse_stmt(p));
				return stmtLABEL(id, s);
			} else {
				Type *btype;
				int old_count = p->next_count;
				Declarator d = parse_type1_ident_post(p, id, &btype);
				Stmt *decl = parse_decl0(p, d, btype, false);
				int new_count = p->next_count;
				if (old_count != new_count)
					return decl;
				// maybe an expression
				Expr *e0 = parse_ident_or_builtin(p, id);
				e = parse_postfix_expr_post(p, e0, 17);
			}
		} else {
			Stmt *decl;
			// both decl and expr can start with __extension__
			// match() is safe because we always ignore the keyword
			match(p, TOK_EXTENSION);
			if (parse_decl(p, &decl))
				return decl;
			e = parse_expr(p);
		}
		if (e) {
			F(match(p, ';'));
			return stmtEXPR(e);
		} else {
			return NULL;
		}
	}
	}
}

Type *parse_type(Parser *p)
{
	Declarator d = parse_type1(p, NULL, NULL, false);
	free_funscope(&d);
	return d.type;
}

Expr *parse_expr(Parser *p)
{
	Expr *e = parse_assignment_expr(p);
	return parse_expr_post(p, e, 17);
}

StmtBLOCK *parse_translation_unit(Parser *p)
{
	StmtBLOCK *s = parse_decls(p, false, true);
	enter_scope(p);
	if (lexer_peek(p->lexer) != TOK_END) {
		leave_scope(p);
		return NULL;
	}
	leave_scope(p);
	return s;
}

END_MANAGED
