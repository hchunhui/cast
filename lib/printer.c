#include "printer.h"
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

struct Printer_
{
	bool print_type_annot;
};

static void decl_flags_print(unsigned int flags)
{
	if (flags & DFLAG_EXTERN)
		printf("extern ");
	if (flags & DFLAG_STATIC)
		printf("static ");
	if (flags & DFLAG_REGISTER)
		printf("register ");
	if (flags & DFLAG_INLINE)
		printf("inline ");
	if (flags & DFLAG_THREADLOCAL)
		printf("_Thread_local ");
	if (flags & DFLAG_NORETURN)
		printf("_Noreturn ");
	if (flags & DFLAG_MANAGED)
		printf("/* __managed */ ");
}

static void type_flags_print(Type *t)
{
	unsigned int flags = 0;
	switch(t->type) {
	case TYPE_VOID: flags = ((TypeVOID *) t)->flags; break;
	case TYPE_PRIM: flags = ((TypePRIM *) t)->flags; break;
	case TYPE_PTR: flags = ((TypePTR *) t)->flags; break;
	case TYPE_ARRAY: flags = ((TypeARRAY *) t)->flags; break;
	case TYPE_FUN: break;
	case TYPE_TYPEDEF: flags = ((TypeTYPEDEF *) t)->flags; break;
	case TYPE_STRUCT:  flags = ((TypeSTRUCT *) t)->flags; break;
	case TYPE_ENUM: flags = ((TypeENUM *) t)->flags; break;
	case TYPE_TYPEOF: flags = ((TypeTYPEOF *) t)->flags; break;
	case TYPE_AUTO: flags = ((TypeAUTO *) t)->flags; break;
	default:
		assert(false);
		break;
	}
	if (flags & TFLAG_CONST)
		printf("const ");
	if (flags & TFLAG_RESTRICT)
		printf("restrict ");
	if (flags & TFLAG_VOLATILE)
		printf("volatile ");
	if (flags & TFLAG_ATOMIC)
		printf("_Atomic ");
	if (flags & TFLAG_COMPLEX)
		printf("_Complex ");
	if (flags & TFLAG_IMAGINARY)
		printf("_Imaginary ");
}

static void attrs_print(Printer *self, Attribute *attrs);

static void lp()
{
	printf("(");
}

static void rp()
{
	printf(")");
}

static bool expr_isprim(Expr *h)
{
	switch (h->type) {
	case EXPR_INT_CST:
	case EXPR_UINT_CST:
	case EXPR_LONG_CST:
	case EXPR_ULONG_CST:
	case EXPR_LLONG_CST:
	case EXPR_ULLONG_CST:
	case EXPR_CHAR_CST:
	case EXPR_STRING_CST:
	case EXPR_BOOL_CST:
	case EXPR_FLOAT_CST:
	case EXPR_DOUBLE_CST:
	case EXPR_IDENT:
		return true;
	default: return false;
	}
}

static bool expr_ishigh(Expr *h)
{
	switch (h->type) {
	case EXPR_BOP: {
		int op = ((ExprBOP *) h)->op;
		if (op == EXPR_OP_IDX) {
			return true;
		}
		break;
	}
	case EXPR_MEM:
	case EXPR_PMEM:
	case EXPR_CALL:
		return true;
	}
	return false;
}

static void expr_print(Printer *self, Expr *h, bool simple);
static void expr_print1(Printer *self, Expr *h, bool simple)
{
	if (expr_isprim(h) || h->type == EXPR_INIT) {
		expr_print(self, h, simple);
	} else {
		lp(); expr_print(self, h, simple); rp();
	}
}

static void expr_print2(Printer *self, Expr *h, bool simple)
{
	if (h->type == EXPR_BOP &&
	    ((ExprBOP *) h)->op == EXPR_OP_COMMA) {
		lp(); expr_print(self, h, simple); rp();
	} else {
		expr_print(self, h, simple);
	}
}

static void expr_print_assign(Printer *self, Expr *h, bool simple)
{
	int op = ((ExprBOP *) h)->op;
	if (h->type == EXPR_BOP &&
	    (op == EXPR_OP_COMMA ||
	     op >= EXPR_OP_ASSIGN && op <= EXPR_OP_ASSIGNBSHR)) {
		lp(); expr_print(self, h, simple); rp();
	} else {
		expr_print(self, h, simple);
	}
}

static void expr_print_bop(Printer *self, Expr *h, bool simple)
{
	if (expr_isprim(h) || h->type == EXPR_INIT || expr_ishigh(h) ||
	    h->type == EXPR_UOP) {
		expr_print(self, h, simple);
	} else {
		lp(); expr_print(self, h, simple); rp();
	}
}

static void expr_print_cond(Printer *self, Expr *h, bool simple)
{
	if (h->type == EXPR_BOP &&
	    ((ExprBOP *) h)->op == EXPR_OP_ASSIGN) {
		lp(); expr_print(self, h, simple); rp();
	} else {
		expr_print(self, h, simple);
	}
}

static void print_memlist(Printer *self, Expr *h)
{
	if (h->type == EXPR_MEM) {
		ExprMEM *m = (ExprMEM *) h;
		print_memlist(self, m->a);
		printf(".%s", m->id);
	} else if (h->type == EXPR_BOP &&
		   ((ExprBOP *) h)->op == EXPR_OP_IDX) {
		ExprBOP *i = (ExprBOP *) h;
		print_memlist(self, i->a);
		printf("[");
		expr_print(self, i->b, false);
		printf("]");
	} else {
		expr_print(self, h, false);
	}
}

static void stmt_print(Printer *self, Stmt *h, int level);
static void type_print_annot(Printer *self, Type *type, bool simple)
{
	type_flags_print(type);
	switch(type->type) {
	case TYPE_VOID:
		printf("void");
		break;
	case TYPE_PRIM: {
		switch(((TypePRIM *) type)->kind) {
		case PT_INT:
			printf("int");
			break;
		case PT_SHORT:
			printf("short");
			break;
		case PT_LONG:
			printf("long");
			break;
		case PT_LLONG:
			printf("long long");
			break;
		case PT_UINT:
			printf("unsigned int");
			break;
		case PT_USHORT:
			printf("unsigned short");
			break;
		case PT_ULONG:
			printf("unsigned long");
			break;
		case PT_ULLONG:
			printf("unsigned long long");
			break;
		case PT_BOOL:
			printf("_Bool");
			break;
		case PT_FLOAT:
			printf("float");
			break;
		case PT_LDOUBLE:
			printf("long double");
			break;
		case PT_DOUBLE:
			printf("double");
			break;
		case PT_CHAR:
			printf("char");
			break;
		case PT_SCHAR:
			printf("signed char");
			break;
		case PT_UCHAR:
			printf("unsigned char");
			break;
		case PT_INT128:
			printf("__int128");
			break;
		case PT_UINT128:
			printf("unsigned __int128");
			break;
		default:
			abort();
		}
		break;
	}
	case TYPE_PTR:
		printf("pointer(");
		type_print_annot(self, ((TypePTR *) type)->t, simple);
		printf(")");
		break;
	case TYPE_ARRAY:
		printf("array(");
		type_print_annot(self, ((TypeARRAY *) type)->t, simple);
		printf(", ");
		if (((TypeARRAY *) type)->n) {
			if (((TypeARRAY *) type)->flags & TFLAG_ARRAY_STATIC)
				printf("static ");
			expr_print2(self, ((TypeARRAY *) type)->n, simple);
		}
		printf(")");
		break;
	case TYPE_FUN:
		printf("(");
		Type *p;
		int i;
		bool flag = false;
		avec_foreach(&((TypeFUN *) type)->at, p, i) {
			if (i) printf(", ");
			type_print_annot(self, p, simple);
			flag = true;
		}
		if (((TypeFUN *) type)->va_arg)
			printf("%s...", flag ? ", " : "");
		printf(") -> ");
		type_print_annot(self, ((TypeFUN *) type)->rt, simple);
		break;
	case TYPE_TYPEDEF:
		printf("%s", ((TypeTYPEDEF *) type)->name);
		break;
	case TYPE_STRUCT: {
		TypeSTRUCT *t = (TypeSTRUCT *) type;
		printf("%s", t->is_union ? "union" : "struct");
		if (t->attrs) {
			printf(" ");
			attrs_print(self, t->attrs);
		}
		if (t->tag) printf(" %s", t->tag);
		if (t->decls) {
			if (simple) {
				printf(" {/* ... */}");
			} else {
				printf(" ");
				stmt_print(self, (Stmt *) t->decls, 0);
			}
		}
		break;
	}
	case TYPE_ENUM: {
		TypeENUM *t = (TypeENUM *) type;
		printf("enum");
		if (t->tag) printf(" %s", t->tag);
		if (t->list) {
			if (simple) {
				printf(" {/* ... */}");
			} else {
				printf(" {\n");
				struct EnumPair_ *p;
				int i;
				avec_foreach_ptr(&(t->list->items), p, i) {
					printf("\t%s", p->id);
					if (p->attr) {
						printf(" ");
						attrs_print(self, p->attr);
					}
					if (p->val) {
						printf(" = ");
						expr_print1(self, p->val, simple);
						printf(",\n");
					} else {
						printf(",\n");
					}
				}
				printf("}");
			}
		}
		break;
	}
	case TYPE_TYPEOF: {
		TypeTYPEOF *t = (TypeTYPEOF *) type;
		printf("__typeof__(");
		expr_print(self, t->e, simple);
		printf(")");
		break;
	}
	case TYPE_AUTO:
		printf("__auto_type");
		break;
	default:
		assert(false);
		break;
	}
}

static Type* type_get_basic(Type *type)
{
	switch(type->type) {
	case TYPE_FUN:
		return type_get_basic(((TypeFUN *) type)->rt);
	case TYPE_PTR:
		return type_get_basic(((TypePTR *) type)->t);
	case TYPE_ARRAY:
		return type_get_basic(((TypeARRAY *) type)->t);
	default:
		return type;
	}
}

static void type_print_declarator1(Printer *self, Type *type)
{
	switch(type->type) {
	case TYPE_FUN:
		type_print_declarator1(self, ((TypeFUN *) type)->rt);
		return;
	case TYPE_PTR: {
		Type *nt = ((TypePTR *) type)->t;
		type_print_declarator1(self, nt);
		if (nt->type == TYPE_FUN || nt->type == TYPE_PTR || nt->type == TYPE_ARRAY)
			printf("(*");
		else
			printf("*");
		type_flags_print(type);
		return;
	}
	case TYPE_ARRAY: {
		Type *nt = ((TypeARRAY *) type)->t;
		type_print_declarator1(self, ((TypeARRAY *) type)->t);
		if (nt->type == TYPE_FUN || nt->type == TYPE_PTR || nt->type == TYPE_ARRAY)
			printf("(");
		return;
	}
	default:
		return;
	}
}

static void type_print_vardecl(Printer *self, unsigned int flags, Type *type, const char *name,
			       bool simple);
static void type_print_declarator2(Printer *self, Type *type)
{
	switch(type->type) {
	case TYPE_FUN:
		printf("(");
		Type *p;
		int i;
		avec_foreach(&((TypeFUN *) type)->at, p, i) {
			if (i) printf(", ");
			type_print_annot(self, type_get_basic(p), false);
			printf(" ");
			type_print_declarator1(self, p);
			type_print_declarator2(self, p);
		}
		if (((TypeFUN *) type)->at.length == 0)
			printf("void");
		if (((TypeFUN *) type)->va_arg)
			printf(", ...");
		printf(")");
		type_print_declarator2(self, ((TypeFUN *) type)->rt);
		return;
	case TYPE_PTR: {
		Type *nt = ((TypePTR *) type)->t;
		if (nt->type == TYPE_FUN || nt->type == TYPE_PTR || nt->type == TYPE_ARRAY)
			printf(")");
		type_print_declarator2(self, nt);
		return;
	}
	case TYPE_ARRAY: {
		Type *nt = ((TypeARRAY *) type)->t;
		printf("[");
		type_flags_print(type);
		if (((TypeARRAY *) type)->n) {
			if (((TypeARRAY *) type)->flags & TFLAG_ARRAY_STATIC)
				printf("static ");
			expr_print1(self, ((TypeARRAY *) type)->n, false);
		}
		if (nt->type == TYPE_FUN || nt->type == TYPE_PTR || nt->type == TYPE_ARRAY)
			printf("])");
		else
			printf("]");
		type_print_declarator2(self, nt);
		return;
	}
	default:
		return;
	}
}

static void type_print_vardecl(Printer *self, unsigned int flags, Type *type, const char *name, bool simple)
{
	if (simple) {
		type_print_annot(self, type, true);
		return;
	}
	decl_flags_print(flags);
	type_print_annot(self, type_get_basic(type), false);
	printf(" ");
	type_print_declarator1(self, type);
	if (name) {
		printf("%s", name);
	} else {
		printf("/* unnamed */");
	}
	type_print_declarator2(self, type);
}

static void type_print_fundecl(Printer *self, unsigned int flags, TypeFUN *type, StmtBLOCK *args, const char *name, Attribute *attrs)
{
	Type *rt = type->rt;
	/*
	  attribute printing is tricky...
	  consider:
	  (1)    struct foo {...} __attribute__((...)) a, b; // type attr
	  (2)    int __attribute__((...)) a, b; // b doesn't have attr
	  (3)    __attribute__((...)) int a, b; // a and b have attr
	*/

	decl_flags_print(flags);
	// safe, because we don't declare multiple variables in one line
	if (attrs) {
		attrs_print(self, attrs);
		printf(" ");
	}
	type_print_annot(self, type_get_basic(rt), false);
	printf(" ");
	type_print_declarator1(self, rt);
	printf("%s(", name);
	if (args) {
		Stmt *p;
		int i;
		avec_foreach(&args->items, p, i) {
			StmtVARDECL *p1 = (StmtVARDECL *) p;
			if (i)
				printf(", ");
			type_print_vardecl(self,
				p1->flags,
				p1->type,
				p1->name,
				false);
			if (p1->ext.gcc_attribute) {
				printf(" ");
				attrs_print(self, p1->ext.gcc_attribute);
			}
		}
		if (type->va_arg)
			printf(", ...");
	} else {
		if (!type->va_arg)
			printf("void");
	}
	printf(")");
	type_print_declarator2(self, rt);
}

static const char *bopname(ExprBinOp op)
{
	switch (op) {
	case EXPR_OP_ADD: return "+";
	case EXPR_OP_SUB: return "-";
	case EXPR_OP_MUL: return "*";
	case EXPR_OP_DIV: return "/";
	case EXPR_OP_MOD: return "%";

	case EXPR_OP_OR: return "||";
	case EXPR_OP_AND: return "&&";

	case EXPR_OP_BOR: return "|";
	case EXPR_OP_BAND: return "&";
	case EXPR_OP_BXOR: return "^";
	case EXPR_OP_BSHL: return "<<";
	case EXPR_OP_BSHR: return ">>";

	case EXPR_OP_COMMA: return ",";

	case EXPR_OP_EQ: return "==";
	case EXPR_OP_NEQ: return "!=";
	case EXPR_OP_LT: return "<";
	case EXPR_OP_LE: return "<=";
	case EXPR_OP_GT: return ">";
	case EXPR_OP_GE: return ">=";

	case EXPR_OP_ASSIGN: return "=";
	case EXPR_OP_ASSIGNADD: return "+=";
	case EXPR_OP_ASSIGNSUB: return "-=";
	case EXPR_OP_ASSIGNMUL: return "*=";
	case EXPR_OP_ASSIGNDIV: return "/=";
	case EXPR_OP_ASSIGNMOD: return "%=";
	case EXPR_OP_ASSIGNBOR: return "|=";
	case EXPR_OP_ASSIGNBAND: return "&=";
	case EXPR_OP_ASSIGNBXOR: return "^=";
	case EXPR_OP_ASSIGNBSHL: return "<<=";
	case EXPR_OP_ASSIGNBSHR: return ">>=";
	default: abort();
	}
}

static void print_quoted(const char *v, int len)
{
	for (int i = 0; i < len; i++) {
		switch(v[i]) {
		case '\\': printf("\\\\"); break;
		case '\"': printf("\\\""); break;
		case '\a': printf("\\a"); break;
		case '\b': printf("\\b"); break;
		case '\f': printf("\\f"); break;
		case '\n': printf("\\n"); break;
		case '\r': printf("\\r"); break;
		case '\t': printf("\\t"); break;
		case '?': printf("\\\?"); break;
		default:
			if (v[i] >= 0 && v[i] < 32) {
				printf("\"\"\\x%x\"\"", v[i]); break;
			} else {
				putchar(v[i]); break;
			}
		}
	}
}

static void expr_print(Printer *self, Expr *h, bool simple)
{
	switch (h->type) {
	case EXPR_INT_CST: {
		ExprINT_CST *e = (ExprINT_CST *) h;
		printf("%d", e->v);
		break;
	}
	case EXPR_UINT_CST: {
		ExprUINT_CST *e = (ExprUINT_CST *) h;
		printf("%uu", e->v);
		break;
	}
	case EXPR_LONG_CST: {
		ExprLONG_CST *e = (ExprLONG_CST *) h;
		printf("%ldl", e->v);
		break;
	}
	case EXPR_ULONG_CST: {
		ExprULONG_CST *e = (ExprULONG_CST *) h;
		printf("%luul", e->v);
		break;
	}
	case EXPR_LLONG_CST: {
		ExprLLONG_CST *e = (ExprLLONG_CST *) h;
		printf("%lldll", e->v);
		break;
	}
	case EXPR_ULLONG_CST: {
		ExprULLONG_CST *e = (ExprULLONG_CST *) h;
		printf("%lluull", e->v);
		break;
	}
	case EXPR_CHAR_CST: {
		ExprCHAR_CST *e = (ExprCHAR_CST *) h;
		switch(e->kind) {
		case WCK_NONE: break;
		case WCK_L: putchar('L'); break;
		case WCK_u: putchar('u'); break;
		case WCK_U: putchar('U'); break;
		case WCK_u8: printf("u8"); break;
		}
		putchar('\'');
		switch(e->v) {
		case '\\': printf("\\\\"); break;
		case '\'': printf("\\\'"); break;
		case '\a': printf("\\a"); break;
		case '\b': printf("\\b"); break;
		case '\f': printf("\\f"); break;
		case '\n': printf("\\n"); break;
		case '\r': printf("\\r"); break;
		case '\t': printf("\\t"); break;
		case '\0': printf("\\0"); break;
		default:
			if (e->v >= 0 && e->v < 32) {
				printf("\\x%x", e->v); break;
			} else {
				putchar(e->v); break;
			}
		}
		putchar('\'');
		break;
	}
	case EXPR_STRING_CST: {
		ExprSTRING_CST *e = (ExprSTRING_CST *) h;
		switch(e->kind) {
		case WCK_NONE: break;
		case WCK_L: putchar('L'); break;
		case WCK_u: putchar('u'); break;
		case WCK_U: putchar('U'); break;
		case WCK_u8: printf("u8"); break;
		}
		putchar('"');
		print_quoted(e->v, e->len - 1);
		putchar('"');
		break;
	}
	case EXPR_BOOL_CST: {
		ExprBOOL_CST *e = (ExprBOOL_CST *) h;
		printf("%s", e->v ? "true" : "false");
		break;
	}
	case EXPR_FLOAT_CST: {
		ExprFLOAT_CST *e = (ExprFLOAT_CST *) h;
		if (isinf(e->v)) {
			printf("(__builtin_inff())");
		} else {
			printf("%.20ef", e->v);
		}
		break;
	}
	case EXPR_DOUBLE_CST: {
		ExprDOUBLE_CST *e = (ExprDOUBLE_CST *) h;
		if (isinf(e->v)) {
			printf("(__builtin_inf())");
		} else {
			printf("%.20e", e->v);
		}
		break;
	}
	case EXPR_IDENT: {
		ExprIDENT *e = (ExprIDENT *) h;
		printf("%s", e->id);
		break;
	}
	case EXPR_MEM: {
		ExprMEM *e = (ExprMEM *) h;
		expr_print1(self, e->a, simple);
		printf(".%s", e->id);
		break;
	}
	case EXPR_PMEM: {
		ExprPMEM *e = (ExprPMEM *) h;
		expr_print1(self, e->a, simple);
		printf("->%s", e->id);
		break;
	}
	case EXPR_CALL: {
		ExprCALL *e = (ExprCALL *) h;
		expr_print1(self, e->func, simple);
		printf("(");
		Expr *p;
		int i;
		avec_foreach(&e->args, p, i) {
			if (i) printf(", ");
			expr_print2(self, p, simple);
		}
		printf(")");
		break;
	}
	case EXPR_BOP: {
		ExprBOP *e = (ExprBOP *) h;
		if (e->op == EXPR_OP_IDX) {
			expr_print1(self, e->a, simple);
			printf("["); expr_print(self, e->b, simple); printf("]");
		} else if (e->op >= EXPR_OP_ASSIGN && e->op <= EXPR_OP_ASSIGNBSHR) {
			expr_print_assign(self, e->a, simple);
			printf(" %s ", bopname(e->op));
			expr_print_assign(self, e->b, simple);
		} else {
			expr_print_bop(self, e->a, simple);
			if (e->op == EXPR_OP_COMMA)
				printf(", ");
			else
				printf(" %s ", bopname(e->op));
			expr_print_bop(self, e->b, simple);
		}
		break;
	}
	case EXPR_UOP: {
		ExprUOP *e = (ExprUOP *) h;
		switch (e->op) {
		case EXPR_OP_NEG: printf("-"); expr_print1(self, e->e, simple); break;
		case EXPR_OP_POS: printf("+"); expr_print1(self, e->e, simple); break;
		case EXPR_OP_NOT: printf("!"); expr_print1(self, e->e, simple); break;
		case EXPR_OP_BNOT: printf("~"); expr_print1(self, e->e, simple); break;

		case EXPR_OP_ADDROF: printf("&"); expr_print1(self, e->e, simple); break;
		case EXPR_OP_DEREF: printf("*"); expr_print1(self, e->e, simple); break;

		case EXPR_OP_PREINC: printf("++"); expr_print1(self, e->e, simple); break;
		case EXPR_OP_POSTINC: expr_print1(self, e->e, simple); printf("++"); break;
		case EXPR_OP_PREDEC: printf("--"); expr_print1(self, e->e, simple); break;
		case EXPR_OP_POSTDEC: expr_print1(self, e->e, simple); printf("--"); break;

		case EXPR_OP_ADDROFLABEL:
			printf("&&%s", ((ExprIDENT * )e->e)->id); break;
		default: abort();
		}
		break;
	}
	case EXPR_COND: {
		ExprCOND *e = (ExprCOND *) h;
		expr_print1(self, e->c, simple);
		printf(" ? ");
		if (e->a)
			expr_print1(self, e->a, simple);
		printf(" : ");
		expr_print1(self, e->b, simple);
		break;
	}
	case EXPR_CAST: {
		ExprCAST *e = (ExprCAST *) h;
		printf("(");
		type_print_vardecl(self, 0, e->t, "", simple);
		if (e->e->type == EXPR_INIT) {
			printf(") ");
			expr_print(self, e->e, simple);
		} else {
			printf(") ");
			expr_print1(self, e->e, simple);
		}
		break;
	}
	case EXPR_SIZEOF: {
		ExprSIZEOF *e = (ExprSIZEOF *) h;
		printf("sizeof ");
		expr_print1(self, e->e, simple);
		break;
	}
	case EXPR_SIZEOFT: {
		ExprSIZEOFT *e = (ExprSIZEOFT *) h;
		printf("sizeof (");
		type_print_vardecl(self, 0, e->t, "", simple);
		printf(")");
		break;
	}
	case EXPR_ALIGNOF: {
		ExprALIGNOF *e = (ExprALIGNOF *) h;
		if (e->t->type == TYPE_TYPEOF) {
			TypeTYPEOF *t = (TypeTYPEOF *) e->t;
			printf("__alignof__(");
			expr_print(self, t->e, simple);
			printf(")");
		} else {
			printf("_Alignof (");
			type_print_vardecl(self, 0, e->t, "", simple);
			printf(")");
		}
		break;
	}
	case EXPR_INIT: {
		ExprINIT *e = (ExprINIT *) h;
		printf("{\n");
		ExprINITItem p;
		int i;
		avec_foreach(&e->items, p, i) {
			if (i) printf(",\n");
			printf("\t");
			if (p.designator) {
				Designator *d = p.designator;
				while (d) {
					switch(d->type) {
					case DES_INDEX:
						printf("[");
						expr_print(self, d->index, simple);
						printf("]");
						break;
					case DES_FIELD:
						printf(".%s", d->field);
						break;
					case DES_INDEXRANGE:
						printf("[");
						expr_print(self, d->index, simple);
						printf(" ... ");
						expr_print(self, d->indexhigh, simple);
						printf("]");
						break;
					}
					d = d->next;
				}
				printf(" = ");
			}
			expr_print2(self, p.value, simple);
		}
		printf("\n}");
		break;
	}
	case EXPR_VASTART: {
		ExprVASTART *e = (ExprVASTART *) h;
		printf("__builtin_va_start(");
		expr_print2(self, e->ap, simple);
		printf(", ");
		printf("%s ", e->last);
		printf(")");
		break;
	}
	case EXPR_VAARG: {
		ExprVAARG *e = (ExprVAARG *) h;
		printf("__builtin_va_arg(");
		expr_print2(self, e->ap, simple);
		printf(", ");
		type_print_vardecl(self, 0, e->type, "", simple);
		printf(")");
		break;
	}
	case EXPR_VAEND: {
		ExprVAEND *e = (ExprVAEND *) h;
		printf("__builtin_va_end(");
		expr_print1(self, e->ap, simple);
		printf(")");
		break;
	}
	case EXPR_OFFSETOF: {
		ExprOFFSETOF *e = (ExprOFFSETOF *) h;
		printf("__builtin_offsetof( ");
		type_print_vardecl(self, 0, e->type, "", simple);
		printf(", ");
		print_memlist(self, e->mem);
		printf(")");
		break;
	}
	case EXPR_STMT: {
		ExprSTMT *e = (ExprSTMT *) h;
		if (simple)
			printf("({/* ... */})");
		else {
			printf("(");
			stmt_print(self, (Stmt *) e->s, 0);
			printf(")");
		}
		break;
	}
	case EXPR_GENERIC: {
		ExprGENERIC *e = (ExprGENERIC *) h;
		printf("_Generic(");
		expr_print2(self, e->expr, simple);
		GENERICPair *item;
		int i;
		avec_foreach_ptr(&e->items, item, i) {
			printf(", ");
			if (item->type)
				type_print_vardecl(self, 0, item->type, "", simple);
			else
				printf("default");
			printf(": ");
			expr_print2(self, item->expr, simple);
		}
		printf(")");
		break;
	}
	case EXPR_TYPESCOMPATIBLE: {
		ExprTYPESCOMPATIBLE *e = (ExprTYPESCOMPATIBLE *) h;
		printf("__builtin_types_compatible_p( ");
		type_print_vardecl(self, 0, e->type1, "", simple);
		printf(", ");
		type_print_vardecl(self, 0, e->type2, "", simple);
		printf(")");
		break;
	}
	case EXPR_TYPENAME: {
		ExprTYPENAME *e = (ExprTYPENAME *) h;
		printf("0 /* __typename__( ");
		type_print_vardecl(self, 0, e->type, "", simple);
		printf(") */");
		break;
	}
	}
}

static void print_level(int level)
{
	for (int i = 0; i < level; i++)
		printf("\t");
}

static void stmt_printb(Printer *self, Stmt *h, int level)
{
	if (h->type == STMT_BLOCK)
		level--;
	stmt_print(self, h, level);
}

static void attrs_print(Printer *self, Attribute *attrs)
{
	printf("__attribute__((");
	for (Attribute *a = attrs; a; a = a->next) {
		printf("%s", a->name);
		if (a->args.length) {
			printf("(");
			Expr *p;
			int i;
			avec_foreach(&a->args, p, i) {
				if (i) printf(", ");
				expr_print2(self, p, false);
			}
			printf(")");
		}
		if (a->next)
			printf(", ");
	}
	printf("))");
}

static void stmt_print(Printer *self, Stmt *h, int level)
{
	if (h->type == STMT_PRAGMA) {
		StmtPRAGMA *s = (StmtPRAGMA *) h;
		printf("\n#pragma %s\n", s->line);
		return;
	}
	if (h->type != STMT_DECLS) {
		if (level > 0 &&
		    (h->type == STMT_CASE ||
		     h->type == STMT_DEFAULT ||
		     h->type == STMT_LABEL))
			level--;
		print_level(level);
	}
	switch (h->type) {
	case STMT_EXPR: {
		StmtEXPR *s = (StmtEXPR *) h;
		expr_print(self, s->expr, false);
		printf(";\n");
		break;
	}
	case STMT_IF: {
		StmtIF *s = (StmtIF *) h;
		printf("if (");
		expr_print_cond(self, s->cond, false);
		printf(")\n");
		stmt_printb(self, s->body1, level + 1);
		if (s->body2) {
			print_level(level);
			printf("else\n");
			if (s->body2->type == STMT_IF) {
				stmt_printb(self, s->body2, level);
			} else {
				stmt_printb(self, s->body2, level + 1);
			}
		}
		printf("\n");
		break;
	}
	case STMT_WHILE: {
		StmtWHILE *s = (StmtWHILE *) h;
		printf("while (");
		expr_print_cond(self, s->cond, false);
		printf(")\n");
		stmt_printb(self, s->body, level + 1);
		printf("\n");
		break;
	}
	case STMT_DO: {
		StmtDO *s = (StmtDO *) h;
		printf("do\n");
		stmt_printb(self, s->body, level + 1);
		print_level(level);
		printf("while (");
		expr_print_cond(self, s->cond, false);
		printf(");\n");
		break;
	}
	case STMT_FOR: {
		StmtFOR *s = (StmtFOR *) h;
		printf("for (");
		if (s->init) expr_print(self, s->init, false);
		printf("; ");
		if (s->cond) expr_print_cond(self, s->cond, false);
		printf("; ");
		if (s->step) expr_print(self, s->step, false);
		printf(")\n");
		stmt_printb(self, s->body, level + 1);
		printf("\n");
		break;
	}
	case STMT_FOR99: {
		StmtFOR99 *s = (StmtFOR99 *) h;
		if (s->init->type == STMT_VARDECL) {
			printf("for (");
			stmt_print(self, s->init, 0);
			if (s->cond) expr_print_cond(self, s->cond, false);
			printf("; ");
			if (s->step) expr_print(self, s->step, false);
			printf(")\n");
			stmt_printb(self, s->body, level + 1);
			printf("\n");
		} else if (s->init->type == STMT_DECLS) {
			printf("{\n");
			stmt_print(self, s->init, level);
			print_level(level);
			printf("for (; ");
			if (s->cond) expr_print_cond(self, s->cond, false);
			printf("; ");
			if (s->step) expr_print(self, s->step, false);
			printf(")\n");
			stmt_printb(self, s->body, level + 1);
			print_level(level);
			printf("}\n");
		} else {
			abort();
		}
		break;
	}
	case STMT_BREAK: {
		printf("break;\n");
		break;
	}
	case STMT_CONTINUE: {
		printf("continue;\n");
		break;
	}
	case STMT_SWITCH: {
		StmtSWITCH *s = (StmtSWITCH *) h;
		printf("switch (");
		expr_print(self, s->expr, false);
		printf(")\n");
		stmt_printb(self, s->body, level + 1);
		printf("\n");
		break;
	}
	case STMT_CASE: {
		StmtCASE *s = (StmtCASE *) h;
		printf("case ");
		expr_print1(self, s->expr, false);
		printf(":\n");
		stmt_print(self, s->stmt, level + 1);
		break;
	}
	case STMT_DEFAULT: {
		StmtDEFAULT *s = (StmtDEFAULT *) h;
		printf("default:\n");
		stmt_print(self, s->stmt, level + 1);
		break;
	}
	case STMT_LABEL: {
		StmtLABEL *s = (StmtLABEL *) h;
		printf("%s:\n", s->name);
		stmt_print(self, s->stmt, level + 1);
		break;
	}
	case STMT_GOTO: {
		StmtGOTO *s = (StmtGOTO *) h;
		printf("goto %s;\n", s->name);
		break;
	}
	case STMT_RETURN: {
		StmtRETURN *s = (StmtRETURN *) h;
		if (s->expr) {
			printf("return ");
			expr_print(self, s->expr, false);
			printf(";\n");
		} else {
			printf("return;\n");
		}
		break;
	}
	case STMT_SKIP: {
		StmtSKIP *s = (StmtSKIP *) h;
		printf("/*skip*/");
		if (s->attrs) {
			printf(" ");
			attrs_print(self, s->attrs);
		}
		printf(";\n");
		break;
	}
	case STMT_BLOCK: {
		StmtBLOCK *s = (StmtBLOCK *) h;
		printf("{\n");
		Stmt *p;
		int i;
		avec_foreach(&s->items, p, i) {
			stmt_print(self, p, level + 1);
		}
		print_level(level);
		printf("}\n");
		break;
	}
	case STMT_FUNDECL: {
		StmtFUNDECL *s = (StmtFUNDECL *) h;
		if (self->print_type_annot) {
			printf("// fundecl: %s, type: ", s->name);
			type_print_annot(self, &s->type->h, true);
			printf("\n");
			print_level(level);
		}
		type_print_fundecl(self, s->flags, s->type, s->args, s->name, s->ext.gcc_attribute);
		if (s->ext.gcc_asm_name) {
			printf(" __asm__(\"");
			int len = strlen(s->ext.gcc_asm_name);
			print_quoted(s->ext.gcc_asm_name, len);
			printf("\")");
		}
		if (s->body) {
			printf("\n");
			stmt_print(self, &s->body->h, level);
		} else {
			printf(";\n");
		}
		break;
	}
	case STMT_VARDECL: {
		StmtVARDECL *s = (StmtVARDECL *) h;
		if (self->print_type_annot) {
			if (s->name)
				printf("// vardecl: %s, type: ", s->name);
			else
				printf("// vardecl: /* unnamed */, type: ");
			type_print_annot(self, s->type, true);
			if (s->bitfield) {
				printf(", bitfield : ");
				expr_print1(self, s->bitfield, true);
			}
			printf("\n");
			print_level(level);
		}
		if (s->ext.c11_alignas) {
			printf("_Alignas(");
			if (s->ext.c11_alignas->type == EXPR_ALIGNOF) {
				ExprALIGNOF *e = (ExprALIGNOF *) (s->ext.c11_alignas);
				type_print_vardecl(self, 0, e->t, "", false);
			} else {
				expr_print(self, s->ext.c11_alignas, false);
			}
			printf(") ");
		}
		type_print_vardecl(self, s->flags, s->type, s->name, false);
		if (s->bitfield) {
			printf(" : ");
			expr_print1(self, s->bitfield, false);
		}
		if (s->ext.gcc_asm_name) {
			printf(" __asm__(\"");
			int len = strlen(s->ext.gcc_asm_name);
			print_quoted(s->ext.gcc_asm_name, len);
			printf("\")");
		}
		if (s->ext.gcc_attribute) {
			printf(" ");
			attrs_print(self, s->ext.gcc_attribute);
		}
		if (s->init) {
			printf(" = ");
			expr_print2(self, s->init, false);
		}
		printf(";\n");
		break;
	}
	case STMT_TYPEDEF: {
		StmtTYPEDEF *s = (StmtTYPEDEF *) h;
		if (self->print_type_annot) {
			printf("// typedef: %s, type: ", s->name);
			type_print_annot(self, s->type, true);
			printf("\n");
			print_level(level);
		}
		if (s->name) {
			if (strcmp(s->name, "_Float32") == 0 ||
			    strcmp(s->name, "_Float64") == 0 ||
			    strcmp(s->name, "_Float32x") == 0 ||
			    strcmp(s->name, "_Float64x") == 0)
				printf("// ");
		}
		printf("typedef ");
		type_print_vardecl(self, 0, s->type, s->name, false);
		if (s->ext.gcc_attribute) {
			printf(" ");
			attrs_print(self, s->ext.gcc_attribute);
		}
		printf(";\n");
		break;
	}
	case STMT_DECLS: {
		StmtDECLS *s = (StmtDECLS *) h;
		Stmt *p;
		int i;
		avec_foreach(&s->items, p, i) {
			stmt_print(self, p, level);
		}
		break;
	}
	case STMT_GOTOADDR: {
		StmtGOTOADDR *s = (StmtGOTOADDR *) h;
		printf("goto ");
		expr_print(self, s->expr, false);
		printf(";\n");
		break;
	}
	case STMT_LABELDECL: {
		StmtLABELDECL *s = (StmtLABELDECL *) h;
		printf("__label__ %s;\n", s->name);
		break;
	}
	case STMT_CASERANGE: {
		StmtCASERANGE *s = (StmtCASERANGE *) h;
		printf("case ");
		expr_print1(self, s->low, false);
		printf(" ... ");
		expr_print1(self, s->high, false);
		printf(":\n");
		stmt_print(self, s->stmt, level + 1);
		break;
	}
	case STMT_ASM: {
		int i;
		StmtASM *s = (StmtASM *) h;
		printf("__asm__");
		if (s->flags & ASM_FLAG_VOLATILE)
			printf(" volatile");
		if (s->flags & ASM_FLAG_INLINE)
			printf(" inline");
		if (s->flags & ASM_FLAG_GOTO)
			printf(" goto");
		printf(" (\"");
		print_quoted(s->content, strlen(s->content));
		printf("\"");
		if (s->outputs.length == 0 &&
		    s->inputs.length == 0 &&
		    s->clobbers.length == 0 &&
		    s->gotolabels.length == 0) {
			printf(");\n");
			break;
		}
		printf(" : ");
		ASMOper *oper;
		avec_foreach_ptr(&s->outputs, oper, i) {
			if (i) printf(", ");
			if (oper->symbol)
				printf("[%s] ", oper->symbol);
			printf("\"");
			print_quoted(oper->constraint, strlen(oper->constraint));
			printf("\" (");
			expr_print(self, oper->variable, false);
			printf(")");
		}
		printf(" : ");
		avec_foreach_ptr(&s->inputs, oper, i) {
			if (i) printf(", ");
			if (oper->symbol)
				printf("[%s] ", oper->symbol);
			printf("\"");
			print_quoted(oper->constraint, strlen(oper->constraint));
			printf("\" (");
			expr_print(self, oper->variable, false);
			printf(")");
		}
		printf(" : ");
		const char *clobber;
		avec_foreach(&s->clobbers, clobber, i) {
			if (i) printf(", ");
			printf("\"");
			print_quoted(clobber, strlen(clobber));
			printf("\"");
		}
		if (s->gotolabels.length) {
			printf(" : ");
			const char *label;
			avec_foreach(&s->gotolabels, label, i) {
				if (i) printf(", ");
				printf("%s", label);
			}
		}
		printf(");\n");
		break;
	}
	case STMT_STATICASSERT: {
		StmtSTATICASSERT *s = (StmtSTATICASSERT *) h;
		printf("_Static_assert(");
		expr_print2(self, s->expr, false);
		if (s->errmsg) {
			printf(", \"");
			print_quoted(s->errmsg, strlen(s->errmsg));
			printf("\"");
		}
		printf(");\n");
		break;
	}
	default:
		abort();
	}
}

void printer_print_translation_unit(Printer *self, StmtBLOCK *s)
{
	Stmt *p;
	int i;
	avec_foreach(&s->items, p, i) {
		stmt_print(self, p, 0);
	}
}

void printer_set_print_type_annot(Printer *p, bool b)
{
	p->print_type_annot = b;
}

static void printer_init(Printer *p)
{
	p->print_type_annot = false;
}

static void printer_free(Printer *p)
{
}

Printer *printer_new()
{
	Printer *self = malloc(sizeof(Printer));
	printer_init(self);
	return self;
}

void printer_delete(Printer *p)
{
	printer_free(p);
	free(p);
}
