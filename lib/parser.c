#include "parser.h"
#include "lexer.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include "map.h"

#define SYM_UNKNOWN 0
#define SYM_IDENT 1
#define SYM_TYPE 2

struct Parser_ {
	Lexer *lexer;
	map_int_t symtabs[100];
	int symtop;
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
	p->symtop = 1;
	symset(p, "__builtin_va_list", SYM_TYPE);
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

#define P lexer_peek(p->lexer)
#define PS lexer_peek_string(p->lexer)
#define PSL lexer_peek_string_len(p->lexer)
#define PI lexer_peek_int(p->lexer)
#define PUI lexer_peek_uint(p->lexer)
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

static Expr *parse_parentheses_post(Parser *p)
{
	Expr *e;
	F(e = parse_expr(p));
	F(match(p, ')'), tree_free(e));
	return exprEXPR(e);
}

static Expr *parse_primary_expr(Parser *p)
{
	switch (P) {
	case TOK_IDENT: {
		const char *name = strdup(PS);
		N; return exprIDENT(name);
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
			if (P == ')') {
				N;
			} else {
				Expr *arg;
				F(arg = parse_assignment_expr(p), tree_free(&n->h));
				exprCALL_append(n, arg);
				while (P == ',') {
					N;
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
				N; e = exprMEM(e, strdup(PS));
			} else {
				return NULL;
			}
			break;
		}
		case TOK_PMEM: { // -
			N;
			if (P == TOK_IDENT) {
				N; e = exprPMEM(e, strdup(PS));
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
	case TOK_INC:
		N; return applyUOP(EXPR_OP_PREINC, parse_unary_expr(p));
	case TOK_DEC:
		N; return applyUOP(EXPR_OP_PREDEC, parse_unary_expr(p));
	case '!':
		N; return applyUOP(EXPR_OP_NOT, parse_unary_expr(p));
	case '~':
		N; return applyUOP(EXPR_OP_BNOT, parse_unary_expr(p));
	case '+':
		N; return applyUOP(EXPR_OP_POS, parse_unary_expr(p));
	case '-':
		N; return applyUOP(EXPR_OP_NEG, parse_unary_expr(p));
	case '&':
		N; return applyUOP(EXPR_OP_ADDROF, parse_unary_expr(p));
	case '*':
		N; return applyUOP(EXPR_OP_DEREF, parse_unary_expr(p));
	case TOK_SIZEOF:
		N;
		if (P == '(') {
			N;
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

static Expr *parse_cast_expr(Parser *p)
{
	if (P =='(') {
		N;
		Type *t = parse_type(p);
		if (t) {
			F(match(p, ')'), tree_free(t));
			if (P == '{') {
				// TODO: ( type-name ) { init list }
				abort();
			} else {
				Expr *e;
				F(e = parse_cast_expr(p), tree_free(t));
				return exprCAST(t, e);
			}
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
		if (P == '?') {
			N;
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
			N; e = applyBOP(EXPR_OP_OR, e, parse_equality_expr(p)); break;
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

static Declarator parse_type1(Parser *p);
static int parse_declarator0(Parser *p, Declarator *d)
{
	if (P == '*') {
		N;
		F(parse_declarator0(p, d));
		d->type = typePTR(d->type);
		return 1;
	}
	if (P == TOK_CONST || P == TOK_VOLATILE || P == TOK_RESTRICT) { // TODO
		N;
		F(parse_declarator0(p, d));
		return 1;
	}
	if (P == TOK_IDENT) {
		F(d->ident == NULL);
		d->ident = strdup(PS);
		N;
	} else if (P == '(') {
		N;
		F(parse_declarator0(p, d));
		F(match(p, ')'));
	}
	while (1) {
		if (P == '[') {
			N;
			Expr *e;
			e = parse_assignment_expr(p);
			F(match(p, ']'), tree_free(e));
			d->type = typeARRAY(d->type, e);
		} else if (P == '(') {
			N;
			TypeFUN *n = typeFUN(d->type);
			if (P == ')') {
				N;
				d->type = &n->h;
			} else {
				d->funargs = stmtBLOCK();
				Declarator d1 = parse_type1(p);
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
				stmtBLOCK_append(d->funargs, stmtVARDECL(d1.flags, d1.ident, d1.type, NULL));
				while (P == ',') {
					N;
					if (P == TOK_DOT3) {
						N;
						n->va_arg = true;
						break;
					}
					d1 = parse_type1(p);
					if (d1.type == NULL) {
						tree_free(&n->h);
					}
					typeFUN_append(n, d1.type);
					stmtBLOCK_append(d->funargs, stmtVARDECL(d1.flags, d1.ident, d1.type, NULL));
				}
				F(match(p, ')'), tree_free(&n->h));
				d->type = &n->h;
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
			ret.id = strdup(PS);
			N;
			if (P == '=') {
				N;
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
static Declarator parse_type1(Parser *p)
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
		// type-qualifier TODO
		case TOK_CONST:
		case TOK_RESTRICT:
		case TOK_VOLATILE:
			N; break;
		// function-specifier TODO
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
					d.type = typeTYPEDEF(strdup(PS));
					N;
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
				tag = strdup(PS);
				N;
			}
			if (P == '{') {
				N;
				decls = parse_stmts(p);
				F_(match(p, '}'), err, tree_free(&decls->h));
			}
			d.type = typeSTRUCT(is_union, tag, decls);
			break;
		}
		case TOK_ENUM:
		{
			N;
			F_(d.type == NULL, err);
			char *tag = NULL;
			StmtBLOCK *decls = NULL;
			if (P == TOK_IDENT) {
				tag = strdup(PS);
				N;
			}

			EnumList *list = malloc(sizeof(EnumList));
			vec_init(&(list->items));
			if (P == '{') {
				N;
				struct EnumPair_ pair = parse_enum_pair(p);
				if (pair.id) {
					vec_push(&(list->items), pair);
					while (P == ',') {
						N;
						pair = parse_enum_pair(p);
						if (pair.id == NULL)
							break;
						vec_push(&(list->items), pair);
					}
				}
				if (P == '}') {
					N;
					d.type = typeENUM(tag, list);
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
			d.type = typeENUM(tag, list);
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
					d.type = is_unsigned ? typeUSHORT() : typeSHORT();
				}
			} else if (long_count == 0) {
				if (tcount)
					d.type = is_unsigned ? typeUINT() : typeINT();
			} else if (long_count == 1) {
				d.type = is_unsigned ? typeULONG() : typeLONG();
			} else if (long_count == 2) {
				d.type = is_unsigned ? typeULLONG() : typeLLONG();
			}
		} else {
			if (is_short + long_count == 0) {
				if (is_char) {
					if (is_char)
						d.type = is_unsigned ? typeUCHAR() : typeCHAR();
				} else if (scount == 0) {
					if (is_bool)
						d.type = typeBOOL();
					else if (is_float)
						d.type = typeFLOAT();
					else if (is_double)
						d.type = typeDOUBLE();
					else if (is_void)
						d.type = typeVOID();
				}
			} else {
				if (is_double && long_count == 1 && !is_short) {
					d.type = typeLDOUBLE();
				}
			}
		}
	}
	F_(d.type, err);
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

static Expr *parse_initializer0(Parser *p)
{
	if (P == '{') {
		N;
		ExprINIT *init = exprINIT();
		Expr *e;
		if (P == '}') {
			N; return (Expr *) init;
		}

		F(e = parse_initializer0(p),
		  tree_free((Expr *) init));
		exprINIT_append(init, e);
		while (P == ',') {
			N;
			e = parse_initializer0(p);
			if (!e) {
				break;
			}
			exprINIT_append(init, e);
		}

		if (P == '}') {
			N; return (Expr *) init;
		}

		tree_free((Expr *) init);
		return NULL;
	}
	return parse_assignment_expr(p);
}

static Expr *parse_initializer(Parser *p)
{
	if (P == '=') {
		N;
		return parse_initializer0(p);
	}
	return NULL;
}

Stmt *parse_decl(Parser *p)
{
	Declarator d = parse_type1(p);
	tree_free(&d.funargs->h);
	F(d.type);

	if (d.is_typedef) {
		if (P == ';') {
			N;
			if (d.ident)
				symset(p, d.ident, SYM_TYPE);
			return stmtTYPEDEF(d.ident, d.type);
		} else {
			tree_free(d.type);
			free(d.ident);
			return NULL;
		}
	}

	if (d.ident)
		symset(p, d.ident, SYM_IDENT);
	if (d.type->type == TYPE_FUN && P == '{') {
		StmtBLOCK *b;
		F(b = parse_block_stmt(p), tree_free(d.type), free(d.ident));
		return stmtFUNDECL(d.flags, d.ident, (TypeFUN *) d.type, d.funargs, b);
	}

	Stmt *decl1;
	if (d.type->type == TYPE_FUN) {
		decl1 = stmtFUNDECL(d.flags, d.ident, (TypeFUN *) d.type, d.funargs, NULL);
	} else {
		decl1 = stmtVARDECL(d.flags, d.ident, d.type, parse_initializer(p));
	}

	if (P == ';') {
		N; return decl1;
	} else {
		StmtDECLS *decls = stmtDECLS();
		stmtDECLS_append(decls, decl1);
		while (P == ',') {
			N;
			Declarator dd = d;
			//dd.type = type_copy(dd.type);
			dd.ident = NULL;
			dd.funargs = NULL;
			parse_declarator(p, &dd);
			if (!dd.ident) {
				//tree_free(dd.type);
				tree_free((Stmt *) decls);
				return NULL;
			}
			symset(p, dd.ident, SYM_IDENT);
			if (dd.type->type == TYPE_FUN) {
				decl1 = stmtFUNDECL(dd.flags, dd.ident, (TypeFUN *) dd.type, dd.funargs, NULL);
			} else {
				decl1 = stmtVARDECL(dd.flags, dd.ident, dd.type, parse_initializer(p));
			}
			stmtDECLS_append(decls, decl1);
		}
		if (P == ';') {
			N; return (Stmt *) decls;
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
		if (P == TOK_ELSE) {
			N;
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
		char *id = strdup(PS);
		N;
		F(match(p, ';'), free(id));
		return stmtGOTO(id);
	}
	case '{': {
		return (Stmt *) parse_block_stmt(p);
	}
	default: {
		Stmt *decl = parse_decl(p);
		if (decl)
			return decl;

		Expr *e;
		if (P == TOK_IDENT) {
			char *id = strdup(PS);
			N;
			if (match(p, ':')) {
				// a label
				Stmt *s;
				F(s = parse_stmt(p), free(id));
				return stmtLABEL(id, s);
			} else {
				// maybe an expression
				e = parse_postfix_expr_post(p, exprIDENT(id), 17);
			}
		} else {
			e = parse_expr(p);
		}
		if (e) {
			F(match(p, ';'), tree_free(e));
			return stmtEXPR(e);
		} else {
			F(match(p, ';'));
			return stmtSKIP();
		}
	}
	}
}

Type *parse_type(Parser *p)
{
	Declarator d = parse_type1(p);
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
