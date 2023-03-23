#include "parser.h"
#include "lexer.h"
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "map.h"

#define SYM_UNKNOWN 0
#define SYM_IDENT 1
#define SYM_TYPE 2

struct Parser_ {
	Lexer *lexer;
	map_int_t symtabs[100];
	int symtop;
	int counter;
};

static int symlookup(Parser *p, const char *sym)
{
	int top = p->symtop - 1;
	int v = SYM_UNKNOWN;
	for (; top >= 0; top--) {
		int *pv = map_get(&(p->symtabs[top]), sym);
		if (pv) {
			v = *pv;
			break;
		}
	}
	return v;
}

static void symset(Parser *p, const char *sym, int sv)
{
	map_set(&(p->symtabs[p->symtop - 1]), sym, sv);
}

static void parser_init(Parser *p, Lexer *l)
{
	p->lexer = l;
	map_init(&(p->symtabs[0]));
	p->symtop = 1;
	symset(p, "__builtin_va_list", SYM_TYPE);
	p->counter = 0;
}

static void parser_free(Parser *p)
{
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

static Type *type_copy(Type *t)
{
	// TODO
	return t;
}

#define P lexer_peek(p->lexer)
#define PS lexer_peek_string(p->lexer)
#define PSL lexer_peek_string_len(p->lexer)
#define PI lexer_peek_int(p->lexer)
#define PUI lexer_peek_uint(p->lexer)
#define PF lexer_peek_float(p->lexer)
#define PC lexer_peek_char(p->lexer)
#define N lexer_next(p->lexer)
#define F_(cond, errval, ...) do { if (!(cond)) { __VA_ARGS__; return errval; } } while (0)
#define F(cond, ...) F_(cond, 0, ## __VA_ARGS__)

static int match(Parser *p, int token)
{
	if (P == token) {
		N;
		return 1;
	} else {
		return 0;
	}
}

static char *get_and_next(Parser *p)
{
	char *id = strdup(PS);
	N;
	return id;
}

static Expr *parse_parentheses_post(Parser *p)
{
	Expr *e;
	F(e = parse_expr(p));
	F(match(p, ')'), tree_free(e));
	return exprEXPR(e);
}

static Expr *parse_assignment_expr(Parser *p);
Type *parse_type(Parser *p);
static Expr *parse_ident_or_builtin(Parser *p, char *id)
{
	if (strcmp(id, "__builtin_va_start") == 0) {
		Expr *ap;
		char *last;
		F(match(p, '('), free(id));
		F(ap = parse_assignment_expr(p), free(id));
		F(match(p, ','), free(id));
		F(P == TOK_IDENT, tree_free(ap), free(id));
		last = get_and_next(p);
		F(match(p, ')'), free(p), tree_free(ap), free(id));
		return exprVASTART(ap, last);
	} else if (strcmp(id, "__builtin_va_arg") == 0) {
		Expr *ap;
		Type *type;
		F(match(p, '('), free(id));
		F(ap = parse_assignment_expr(p), free(id));
		F(match(p, ','), free(id));
		F(type = parse_type(p), tree_free(ap), free(id));
		F(match(p, ')'), tree_free(type), tree_free(ap), free(id));
		return exprVAARG(ap, type);
	} else if (strcmp(id, "__builtin_va_end") == 0) {
		Expr *ap;
		F(match(p, '('), free(id));
		F(ap = parse_assignment_expr(p), free(id));
		F(match(p, ')'), tree_free(ap), free(id));
		return exprVAEND(ap);
	} else if (strcmp(id, "__builtin_offsetof") == 0) {
		Type *type;
		char *mem;
		F(match(p, '('), free(id));
		F(type = parse_type(p), free(id));
		F(match(p, ','), free(id));
		F(P == TOK_IDENT, tree_free(type), free(id));
		mem = get_and_next(p);
		F(match(p, ')'), free(mem), tree_free(type), free(id));
		return exprOFFSETOF(type, mem);
	}
	return exprIDENT(id);
}

static Expr *parse_primary_expr(Parser *p)
{
	switch (P) {
	case TOK_IDENT: {
		char *id = get_and_next(p);
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
	case TOK_CHAR_CST: {
		char c = PC;
		N; return exprCHAR_CST(c);
	}
	case TOK_STRING_CST: {
		int len = PSL;
		char *str = malloc(len);
		memcpy(str, PS, len);
		N; return exprSTRING_CST(str, len);
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
	default:
		return NULL;
	}
}

static Expr *parse_assignment_expr(Parser *p);
static Expr *parse_assignment_expr_post(Parser *p, Expr *e, int prec);
static Expr *parse_multiplicative_expr_post(Parser *p, Expr *e, int prec);
static Expr *parse_postfix_expr_post(Parser *p, Expr *e, int prec) // 2
{
	if (prec < 2)
		return e;

	while (1) {
		switch (P) {
		case '[': {
			N;
			Expr *i;
			F(i = parse_expr(p));
			F(match(p, ']'), tree_free(i));
			e = exprBOP(EXPR_OP_IDX, e, i);
			break;
		}
		case '(': {
			N;
			ExprCALL *n = exprCALL(e);
			if (!match(p, ')')) {
				Expr *arg;
				F(arg = parse_assignment_expr(p), tree_free(&n->h));
				exprCALL_append(n, arg);
				while (match(p, ',')) {
					F(arg = parse_assignment_expr(p), tree_free(&n->h));
					exprCALL_append(n, arg);
				}
				F(match(p, ')'), tree_free(&n->h));
			}
			e = &n->h;
			break;
		}
		case '.': {
			N;
			if (P == TOK_IDENT) {
				e = exprMEM(e, get_and_next(p));
			} else {
				return NULL;
			}
			break;
		}
		case TOK_PMEM: { // -
			N;
			if (P == TOK_IDENT) {
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

static Expr *parse_cast_expr(Parser *p);
static Expr *parse_unary_expr(Parser *p)
{
	Expr *e = parse_primary_expr(p);
	if (e)
		return parse_postfix_expr_post(p, e, 3);

	switch (P) {
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
		N;
		if (match(p, '(')) {
			Type *t = parse_type(p);
			if (t) {
				F(match(p, ')'), tree_free(t));
				return exprSIZEOFT(t);
			}
			Expr *e;
			F(e = parse_postfix_expr_post(p, parse_parentheses_post(p), 3));
			return exprSIZEOF(e);
		} else {
			Expr *e;
			F(e = parse_unary_expr(p));
			return exprSIZEOF(e);
		}
	}
	return NULL;
}

static Expr *parse_initializer(Parser *p);
static Expr *parse_cast_expr(Parser *p)
{
	if (match(p, '(')) {
		Type *t = parse_type(p);
		if (t) {
			F(match(p, ')'), tree_free(t));
			Expr *e;
			if (P == '{') {
				F(e = parse_initializer(p), tree_free(t));
			} else {
				F(e = parse_cast_expr(p), tree_free(t));
			}
			return exprCAST(t, e);
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
		tree_free(a);
		return NULL;
	}
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
			F(e1 = parse_expr(p), tree_free(cond));
			F(match(p, ':'), tree_free(cond), tree_free(e1));
			F(e2 = parse_conditional_expr(p), tree_free(cond), tree_free(e1));
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
	char *ident;
	StmtBLOCK *funargs;
} Declarator;

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
	}
	return flags;
}

static Declarator parse_type1(Parser *p, Type **pbtype);
static int parse_declarator0(Parser *p, Declarator *d)
{
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
		F(parse_declarator0(p, d));
		F(match(p, ')'));
	}
	while (1) {
		if (match(p, '[')) {
			unsigned int flags = 0;
			Expr *e = NULL;
			bool is_static = false;
			if (match(p, '*')) {
				if(!match(p, ']')) {
					F(e = parse_multiplicative_expr_post(
						  p, applyUOP(EXPR_OP_DEREF, parse_cast_expr(p)), 16));
					F(match(p, ']'), tree_free(e));
				}
			} else {
				is_static = match(p, TOK_STATIC);
				flags = parse_type_qualifier(p);
				if (!is_static)
					match(p, TOK_STATIC);
				if (!match(p, ']')) {
					F(e = parse_assignment_expr(p));
					F(match(p, ']'), tree_free(e));
				}
			}
			flags |= is_static ? TFLAG_ARRAY_STATIC : 0;
			d->type = typeARRAY(d->type, e, flags);
		} else if (match(p, '(')) {
			TypeFUN *n = typeFUN(d->type);
			if (match(p, ')')) {
				d->type = &n->h;
			} else {
				StmtBLOCK *funargs = d->funargs ? NULL : stmtBLOCK();
				Declarator d1 = parse_type1(p, NULL);
				if (d1.type == NULL) {
					tree_free(&n->h);
				} else if (d1.type->type == TYPE_VOID &&
					   d1.ident == NULL && P == ')') {
					N;
					tree_free(d1.type);
					d1.type = NULL;
					d->type = &n->h;
					continue;
				}
				typeFUN_append(n, d1.type);
				if (funargs)
					stmtBLOCK_append(funargs, stmtVARDECL(d1.flags, d1.ident, d1.type, NULL, -1));
				while (match(p, ',')) {
					if (match(p, TOK_DOT3)) {
						n->va_arg = true;
						break;
					}
					d1 = parse_type1(p, NULL);
					if (d1.type == NULL) {
						tree_free(&n->h);
					}
					typeFUN_append(n, d1.type);
					if (funargs)
						stmtBLOCK_append(funargs, stmtVARDECL(d1.flags, d1.ident, d1.type, NULL, -1));
				}
				F(match(p, ')'), tree_free(&n->h));
				d->type = &n->h;
				if (funargs)
					d->funargs = funargs;
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
		int sv = symlookup(p, PS);
		if (sv == SYM_UNKNOWN) {
			symset(p, PS, SYM_IDENT);
			ret.id = get_and_next(p);
			if (match(p, '=')) {
				ret.val = parse_assignment_expr(p);
				if (!ret.val) {
					free((char *) ret.id);
					ret.id = NULL;
				}
			}
		}
	}
	return ret;
}

static StmtBLOCK *parse_stmts(Parser *p);
static Declarator parse_type1(Parser *p, Type **pbtype)
{
	Declarator d, err;
	d.is_typedef = false;
	d.flags = 0;
	d.type = NULL;
	d.ident = NULL;
	d.funargs = NULL;
	err.is_typedef = false;
	err.flags = 0;
	err.type = NULL;
	err.ident = NULL;
	err.funargs = NULL;
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

	bool flag = true;
	while (flag) {
		switch (P) {
		// storage-class-specifier
		case TOK_TYPEDEF:
			d.is_typedef = true;
			N; break;
		case TOK_EXTERN:
			d.flags |= DFLAG_EXTERN;
			N; break;
		case TOK_STATIC:
			d.flags |= DFLAG_STATIC;
			N; break;
		case TOK_AUTO: // ignore
		case TOK_REGISTER: // ignore
			N; break;
		// type-qualifier
		case TOK_CONST:
		case TOK_VOLATILE:
		case TOK_RESTRICT:
			tflags |= parse_type_qualifier(p);
			break;
		// function-specifier
		case TOK_INLINE:
			d.flags |= DFLAG_INLINE;
			N; break;
		// type-specifier
		case TOK_SIGNED:
			N; is_signed = 1;
			break;
		case TOK_UNSIGNED:
			N; is_unsigned = 1;
			break;
		case TOK_VOID:
			N; is_void = 1;
			break;
		case TOK_INT:
			N; is_int = 1;
			break;
		case TOK_LONG:
			N; long_count++;
			break;
		case TOK_SHORT:
			N; is_short = 1;
			break;
		case TOK_CHAR:
			N; is_char = 1;
			break;
		case TOK_BOOL:
			N; is_bool = 1;
			break;
		case TOK_FLOAT:
			N; is_float = 1;
			break;
		case TOK_DOUBLE:
			N; is_double = 1;
			break;
		case TOK_IDENT: {
			int sv = symlookup(p, PS);
			if (sv == SYM_TYPE) {
				int tcount = is_int + is_bool + is_char + is_float + is_double + is_void;
				if (d.type == NULL && tcount == 0) {
					char *name = get_and_next(p);
					tflags |= parse_type_qualifier(p);
					d.type = typeTYPEDEF(name, tflags);
					break;
				}
			}
			flag = false;
			break;
		}
		case TOK_STRUCT:
		case TOK_UNION:
		{
			bool is_union = P == TOK_UNION;
			N;
			F_(d.type == NULL, err);
			char *tag = NULL;
			StmtBLOCK *decls = NULL;
			if (P == TOK_IDENT) {
				tag = get_and_next(p);
			}
			tflags |= parse_type_qualifier(p);
			if (match(p, '{')) {
				decls = parse_stmts(p);
				F_(match(p, '}'), err, tree_free(&decls->h));
			}
			d.type = typeSTRUCT(is_union, tag, decls, tflags);
			break;
		}
		case TOK_ENUM:
		{
			N;
			F_(d.type == NULL, err);
			char *tag = NULL;
			StmtBLOCK *decls = NULL;
			if (P == TOK_IDENT) {
				tag = get_and_next(p);
			}

			EnumList *list = NULL;
			if (match(p, '{')) {
				list = malloc(sizeof(EnumList));
				vec_init(&(list->items));
				struct EnumPair_ pair = parse_enum_pair(p);
				if (pair.id) {
					vec_push(&(list->items), pair);
					while (match(p, ',')) {
						pair = parse_enum_pair(p);
						if (pair.id == NULL)
							break;
						vec_push(&(list->items), pair);
					}
				}
				if (match(p, '}')) {
					d.type = typeENUM(tag, list, tflags);
					break;
				}
				struct EnumPair_ *p;
				int i;
				vec_foreach_ptr(&list->items, p, i) {
					free((char *) (p->id));
					free(p->val);
				}
				vec_deinit(&(list->items));
				free(list);
				return err;
			}
			d.type = typeENUM(tag, list, tflags);
			break;
		}
		default:
			flag = false;
			break;
		}
	}

	int tcount = is_int + is_bool + is_char + is_float + is_double + is_void;
	int scount = is_signed + is_unsigned;
	if (d.type) {
		if (tcount + scount + is_short + long_count) {
			tree_free(d.type);
			d.type = NULL;
		}
	} else {
		if (tcount == 0 || is_int) {
			if (is_short) {
				if (long_count == 0) {
					d.type = is_unsigned ? typeUSHORT(tflags) : typeSHORT(tflags);
				}
			} else if (long_count == 0) {
				if (tcount || scount) {
					d.type = is_unsigned ? typeUINT(tflags) : typeINT(tflags);
				}
			} else if (long_count == 1) {
				d.type = is_unsigned ? typeULONG(tflags) : typeLONG(tflags);
			} else if (long_count == 2) {
				d.type = is_unsigned ? typeULLONG(tflags) : typeLLONG(tflags);
			}
		} else {
			if (is_short + long_count == 0) {
				if (is_char) {
					if (is_char)
						d.type = is_unsigned ? typeUCHAR(tflags) : typeCHAR(tflags);
				} else if (scount == 0) {
					if (is_bool)
						d.type = typeBOOL(tflags);
					else if (is_float)
						d.type = typeFLOAT(tflags);
					else if (is_double)
						d.type = typeDOUBLE(tflags);
					else if (is_void)
						d.type = typeVOID(tflags);
				}
			} else {
				if (is_double && long_count == 1 && !is_short) {
					d.type = typeLDOUBLE(tflags);
				}
			}
		}
	}
	F_(d.type, err);
	if (pbtype)
		*pbtype = d.type;
	F_(parse_declarator(p, &d), err);
	return d;
}

static StmtBLOCK *parse_stmts(Parser *p)
{
	StmtBLOCK *block = stmtBLOCK();
	assert(p->symtop < 100);
	map_init(&(p->symtabs[p->symtop]));
	p->symtop++;
	while (P != '}' && P != TOK_END) {
		Stmt *s;
		F(s = parse_stmt(p), tree_free(&block->h),
		  p->symtop--, map_deinit(&(p->symtabs[p->symtop])));
		stmtBLOCK_append(block, s);
	}
	p->symtop--;
	map_deinit(&(p->symtabs[p->symtop]));
	return block;
}

static StmtBLOCK *parse_block_stmt(Parser *p)
{
	StmtBLOCK *block;
	F(match(p, '{'));
	F(block = parse_stmts(p));
	F(match(p, '}'), tree_free(&block->h));
	return block;
}

static bool match_initialzer_item(Parser *p, ExprINIT *init)
{
	Designator *d1 = NULL;
	while (true) {
		Designator *n;
		if (match(p, '[')) {
			n = malloc(sizeof(Designator));
			n->type = DES_INDEX;
			// free...
			F(n->index = parse_expr(p));
			F(match(p, ']'));
		} else if (match(p, '.')) {
			n = malloc(sizeof(Designator));
			n->type = DES_FIELD;
			// free...
			F(P == TOK_IDENT);
			n->field = strdup(PS);
			N;
		} else {
			break;
		}
		n->next = d1;
		d1 = n;
	}
	if (d1) {
		// free...
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
	// free...
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

		F(match_initialzer_item(p, init),
		  tree_free((Expr *) init));
		while (match(p, ',')) {
			if (!match_initialzer_item(p, init))
				break;
		}

		if (match(p, '}')) {
			return (Expr *) init;
		}

		tree_free((Expr *) init);
		return NULL;
	}
	return parse_assignment_expr(p);
}

static Expr *parse_initializer1(Parser *p)
{
	F(match(p, '='));
	return parse_initializer(p);
}

Stmt *make_decl(Parser *p, Declarator d)
{
	Stmt *decl1;
	if (d.type->type == TYPE_FUN) {
		decl1 = stmtFUNDECL(d.flags, d.ident, (TypeFUN *) d.type, d.funargs, NULL);
	} else {
		int bitfield = -1;
		if (match(p, ':')) {
			if (P == TOK_INT_CST) {
				bitfield = PI; N;
			} else {
				return NULL;
			}
		}
		decl1 = stmtVARDECL(d.flags, d.ident, d.type, parse_initializer1(p), bitfield);
	}
	return decl1;
}

Stmt *parse_decl(Parser *p)
{
	Type *btype;
	Declarator d = parse_type1(p, &btype);
	tree_free(&d.funargs->h);
	F(d.type);

	bool is_typedef = d.is_typedef;
	unsigned int symtype = is_typedef ? SYM_TYPE : SYM_IDENT;
	if (d.ident)
		symset(p, d.ident, symtype);
	if (d.type->type == TYPE_FUN && P == '{') {
		F(!is_typedef);
		StmtBLOCK *b;
		F(b = parse_block_stmt(p), tree_free(d.type), free(d.ident));
		return stmtFUNDECL(d.flags, d.ident, (TypeFUN *) d.type, d.funargs, b);
	}

	Stmt *decl1;
	if (is_typedef) {
		decl1 = stmtTYPEDEF(d.ident, d.type);
	} else {
		F(decl1 = make_decl(p, d), tree_free(d.type), free(d.ident));
	}

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
					b->tag = strdup(buf);
				}
				Type *ntype = typeSTRUCT(b->is_union, b->tag, b->decls, 0);
				b->decls = NULL;
				stmtDECLS_append(decls, stmtVARDECL(0, NULL, ntype, NULL, -1));
			}
		}
		stmtDECLS_append(decls, decl1);
		while (match(p, ',')) {
			Declarator dd = d;
			dd.type = type_copy(btype);
			dd.ident = NULL;
			dd.funargs = NULL;
			parse_declarator(p, &dd);
			if (!dd.ident) {
				//tree_free(dd.type);
				tree_free((Stmt *) decls);
				return NULL;
			}
			symset(p, dd.ident, symtype);
			if (is_typedef) {
				F(decl1 = stmtTYPEDEF(dd.ident, dd.type), tree_free((Stmt *) decls));
			} else {
				F(decl1 = make_decl(p, dd), tree_free((Stmt *) decls));
			}
			stmtDECLS_append(decls, decl1);
		}
		if (match(p, ';')) {
			return (Stmt *) decls;
		} else {
			tree_free((Stmt *) decls);
		}
	}
	return NULL;
}

Stmt *parse_stmt(Parser *p)
{
	switch (P) {
	case TOK_IF: {
		N;
		Expr *e;
		Stmt *s1, *s2;
		F(match(p, '('));
		F(e = parse_expr(p));
		F(match(p, ')'), tree_free(e));
		F(s1 = parse_stmt(p), tree_free(e));
		if (match(p, TOK_ELSE)) {
			F(s2 = parse_stmt(p), tree_free(e), tree_free(s1));
			return stmtIF(e, s1, s2);
		}
		return stmtIF(e, s1, NULL);
	}
	case TOK_WHILE: {
		N;
		Expr *e;
		Stmt *s;
		F(match(p, '('));
		F(e = parse_expr(p));
		F(match(p, ')'), tree_free(e));
		F(s = parse_stmt(p), tree_free(e));
		return stmtWHILE(e, s);
	}
	case TOK_DO: {
		N;
		Expr *e;
		Stmt *s;
		F(s = parse_stmt(p));
		F(match(p, TOK_WHILE), tree_free(s));
		F(match(p, '('), tree_free(s));
		F(e = parse_expr(p), tree_free(s));
		F(match(p, ')'), tree_free(s), tree_free(e));
		F(match(p, ';'), tree_free(s), tree_free(e));
		return stmtDO(e, s);
	}
	case TOK_FOR: {
		N;
		Expr *init, *cond, *step;
		Stmt *init99, *body;
		F(match(p, '('));
		init99 = parse_decl(p);
		if (init99) {
			F((init99->type == STMT_VARDECL), tree_free(init99));
		} else {
			init = parse_expr(p);
			F(match(p, ';'), tree_free(init));
		}
		cond = parse_expr(p);
		F(match(p, ';'), tree_free(init), tree_free(cond));
		step = parse_expr(p);
		F(match(p, ')'), tree_free(init), tree_free(cond), tree_free(step));
		F(body = parse_stmt(p), tree_free(init), tree_free(cond), tree_free(step));
		if (init99)
			return stmtFOR99((StmtVARDECL *) init99, cond, step, body);
		else
			return stmtFOR(init, cond, step, body);
	}
	case TOK_BREAK: {
		N;
		F(match(p, ';'));
		return stmtBREAK();
	}
	case TOK_CONTINUE: {
		N;
		F(match(p, ';'));
		return stmtCONTINUE();
	}
	case TOK_SWITCH: {
		N;
		Expr *e;
		StmtBLOCK *b;
		F(match(p, '('));
		F(e = parse_expr(p));
		F(match(p, ')'), tree_free(e));;
		F(b = parse_block_stmt(p), tree_free(e));
		return stmtSWITCH(e, b);
	}
	case TOK_CASE: {
		N;
		Expr *e;
		F(e = parse_expr(p));
		Stmt *s;
		F(match(p, ':'), tree_free(e));
		F(s = parse_stmt(p), tree_free(e));
		return stmtCASE(e, s);
	}
	case TOK_DEFAULT: {
		N;
		Stmt *s;
		F(match(p, ':'));
		F(s = parse_stmt(p));
		return stmtDEFAULT(s);
	}
	case TOK_RETURN: {
		N;
		if (P == ';') {
			N; return stmtRETURN(NULL);
		}
		Expr *e;
		F(e = parse_expr(p));
		F(match(p, ';'), tree_free(e));
		return stmtRETURN(e);
	}
	case TOK_GOTO: {
		N;
		F(P == TOK_IDENT);
		char *id = get_and_next(p);
		F(match(p, ';'), free(id));
		return stmtGOTO(id);
	}
	case '{': {
		return (Stmt *) parse_block_stmt(p);
	}
	case ';': {
		N; return stmtSKIP();
	}
	default: {
		Stmt *decl = parse_decl(p);
		if (decl)
			return decl;

		Expr *e;
		if (P == TOK_IDENT) {
			char *id = get_and_next(p);
			if (match(p, ':')) {
				// a label
				Stmt *s;
				F(s = parse_stmt(p), free(id));
				return stmtLABEL(id, s);
			} else {
				// maybe an expression
				Expr *e0 = parse_ident_or_builtin(p, id);
				e = parse_postfix_expr_post(p, e0, 17);
			}
		} else {
			e = parse_expr(p);
		}
		if (e) {
			F(match(p, ';'), tree_free(e));
			return stmtEXPR(e);
		} else {
			return NULL;
		}
	}
	}
}

Type *parse_type(Parser *p)
{
	Declarator d = parse_type1(p, NULL);
	free(d.ident);
	tree_free(&d.funargs->h);
	return d.type;
}

Expr *parse_expr(Parser *p)
{
	Expr *e = parse_assignment_expr(p);
	return parse_expr_post(p, e, 17);
}

StmtBLOCK *parse_translation_unit(Parser *p)
{
	StmtBLOCK *s = parse_stmts(p);
	if (lexer_peek(p->lexer) != TOK_END) {
		tree_free(&s->h);
		return NULL;
	}
	return s;
}
