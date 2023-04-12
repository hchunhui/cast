#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <cast/parser.h>

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
	case TYPE_INT: flags = ((TypeINT *) t)->flags; break;
	case TYPE_SHORT: flags = ((TypeSHORT *) t)->flags; break;
	case TYPE_LONG: flags = ((TypeLONG *) t)->flags; break;
	case TYPE_LLONG: flags = ((TypeLLONG *) t)->flags; break;
	case TYPE_UINT: flags = ((TypeUINT *) t)->flags; break;
	case TYPE_USHORT: flags = ((TypeUSHORT *) t)->flags; break;
	case TYPE_ULONG: flags = ((TypeULONG *) t)->flags; break;
	case TYPE_ULLONG: flags = ((TypeULLONG *) t)->flags; break;
	case TYPE_BOOL: flags = ((TypeBOOL *) t)->flags; break;
	case TYPE_FLOAT: flags = ((TypeFLOAT *) t)->flags; break;
	case TYPE_LDOUBLE: flags = ((TypeLDOUBLE *) t)->flags; break;
	case TYPE_DOUBLE: flags = ((TypeDOUBLE *) t)->flags; break;
	case TYPE_CHAR: flags = ((TypeCHAR *) t)->flags; break;
	case TYPE_SCHAR: flags = ((TypeSCHAR *) t)->flags; break;
	case TYPE_UCHAR: flags = ((TypeUCHAR *) t)->flags; break;
	case TYPE_PTR: flags = ((TypePTR *) t)->flags; break;
	case TYPE_ARRAY: flags = ((TypeARRAY *) t)->flags; break;
	case TYPE_FUN: break;
	case TYPE_TYPEDEF: flags = ((TypeTYPEDEF *) t)->flags; break;
	case TYPE_STRUCT:  flags = ((TypeSTRUCT *) t)->flags; break;
	case TYPE_ENUM: flags = ((TypeENUM *) t)->flags; break;
	case TYPE_INT128: flags = ((TypeINT128 *) t)->flags; break;
	case TYPE_UINT128: flags = ((TypeUINT128 *) t)->flags; break;
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
}

static void attrs_print(Attribute *attrs);

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

static void expr_print(Expr *h, bool simple);
static void expr_print1(Expr *h, bool simple)
{
	if (h->type == EXPR_EXPR)
		return expr_print1(((ExprEXPR *) h)->e, simple);
	if (expr_isprim(h) || h->type == EXPR_INIT) {
		expr_print(h, simple);
	} else {
		lp(); expr_print(h, simple); rp();
	}
}

static void expr_print2(Expr *h, bool simple)
{
	if (h->type == EXPR_EXPR)
		return expr_print2(((ExprEXPR *) h)->e, simple);
	if (h->type == EXPR_BOP &&
	    ((ExprBOP *) h)->op == EXPR_OP_COMMA) {
		lp(); expr_print(h, simple); rp();
	} else {
		expr_print(h, simple);
	}
}

static void expr_print_cond(Expr *h, bool simple)
{
	if (h->type == EXPR_EXPR) {
		lp(); expr_print(h, simple); rp();
	} else {
		expr_print(h, simple);
	}
}

static void print_memlist(Expr *h)
{
	if (h->type == EXPR_MEM) {
		ExprMEM *m = (ExprMEM *) h;
		print_memlist(m->a);
		printf(".%s", m->id);
	} else if (h->type == EXPR_BOP &&
		   ((ExprBOP *) h)->op == EXPR_OP_IDX) {
		ExprBOP *i = (ExprBOP *) h;
		print_memlist(i->a);
		printf("[");
		expr_print(i->b, false);
		printf("]");
	} else {
		expr_print(h, false);
	}
}

static void stmt_print(Stmt *h, int level);
static void type_print_annot(Type *type, bool simple)
{
	type_flags_print(type);
	switch(type->type) {
	case TYPE_VOID:
		printf("void");
		break;
	case TYPE_INT:
		printf("int");
		break;
	case TYPE_SHORT:
		printf("short");
		break;
	case TYPE_LONG:
		printf("long");
		break;
	case TYPE_LLONG:
		printf("long long");
		break;
	case TYPE_UINT:
		printf("unsigned int");
		break;
	case TYPE_USHORT:
		printf("unsigned short");
		break;
	case TYPE_ULONG:
		printf("unsigned long");
		break;
	case TYPE_ULLONG:
		printf("unsigned long long");
		break;
	case TYPE_BOOL:
		printf("_Bool");
		break;
	case TYPE_FLOAT:
		printf("float");
		break;
	case TYPE_LDOUBLE:
		printf("long double");
		break;
	case TYPE_DOUBLE:
		printf("double");
		break;
	case TYPE_CHAR:
		printf("char");
		break;
	case TYPE_SCHAR:
		printf("signed char");
		break;
	case TYPE_UCHAR:
		printf("unsigned char");
		break;
	case TYPE_INT128:
		printf("__int128");
		break;
	case TYPE_UINT128:
		printf("unsigned __int128");
		break;
	case TYPE_PTR:
		printf("pointer(");
		type_print_annot(((TypePTR *) type)->t, simple);
		printf(")");
		break;
	case TYPE_ARRAY:
		printf("array(");
		type_print_annot(((TypeARRAY *) type)->t, simple);
		printf(", ");
		if (((TypeARRAY *) type)->n) {
			if (((TypeARRAY *) type)->flags & TFLAG_ARRAY_STATIC)
				printf("static ");
			expr_print2(((TypeARRAY *) type)->n, simple);
		}
		printf(")");
		break;
	case TYPE_FUN:
		printf("(");
		Type *p;
		int i;
		avec_foreach(&((TypeFUN *) type)->at, p, i) {
			if (i) printf(", ");
			type_print_annot(p, simple);
		}
		if (((TypeFUN *) type)->va_arg)
			printf(", ...");
		printf(") -> ");
		type_print_annot(((TypeFUN *) type)->rt, simple);
		break;
	case TYPE_TYPEDEF:
		printf("%s", ((TypeTYPEDEF *) type)->name);
		break;
	case TYPE_STRUCT: {
		TypeSTRUCT *t = (TypeSTRUCT *) type;
		printf("%s", t->is_union ? "union" : "struct");
		if (t->attrs) {
			printf(" ");
			attrs_print(t->attrs);
		}
		if (t->tag) printf(" %s", t->tag);
		if (t->decls) {
			if (simple) {
				printf(" {/* ... */}");
			} else {
				printf(" ");
				stmt_print((Stmt *) t->decls, 0);
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
				printf(" {");
				struct EnumPair_ *p;
				int i;
				avec_foreach_ptr(&(t->list->items), p, i) {
					printf("%s", p->id);
					if (p->val) {
						printf(" = ");
						expr_print1(p->val, simple);
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
		expr_print(t->e, simple);
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
	case TYPE_VOID:
	case TYPE_INT:
	case TYPE_SHORT:
	case TYPE_LONG:
	case TYPE_LLONG:
	case TYPE_CHAR:
	case TYPE_SCHAR:
	case TYPE_UINT:
	case TYPE_USHORT:
	case TYPE_ULONG:
	case TYPE_ULLONG:
	case TYPE_UCHAR:
	case TYPE_BOOL:
	case TYPE_FLOAT:
	case TYPE_DOUBLE:
	case TYPE_LDOUBLE:
	case TYPE_INT128:
	case TYPE_UINT128:
	case TYPE_TYPEDEF:
	case TYPE_STRUCT:
	case TYPE_ENUM:
	case TYPE_TYPEOF:
	case TYPE_AUTO:
		return type;
	case TYPE_FUN:
		return type_get_basic(((TypeFUN *) type)->rt);
	case TYPE_PTR:
		return type_get_basic(((TypePTR *) type)->t);
	case TYPE_ARRAY:
		return type_get_basic(((TypeARRAY *) type)->t);
	default:
		assert(false);
	}
}

static void type_print_declarator1(Type *type)
{
	switch(type->type) {
	case TYPE_VOID:
	case TYPE_INT:
	case TYPE_SHORT:
	case TYPE_LONG:
	case TYPE_LLONG:
	case TYPE_CHAR:
	case TYPE_SCHAR:
	case TYPE_UINT:
	case TYPE_USHORT:
	case TYPE_ULONG:
	case TYPE_ULLONG:
	case TYPE_UCHAR:
	case TYPE_BOOL:
	case TYPE_FLOAT:
	case TYPE_DOUBLE:
	case TYPE_LDOUBLE:
	case TYPE_INT128:
	case TYPE_UINT128:
	case TYPE_TYPEDEF:
	case TYPE_TYPEOF:
	case TYPE_AUTO:
		return;
	case TYPE_FUN:
		type_print_declarator1(((TypeFUN *) type)->rt);
		return;
	case TYPE_PTR:
		type_print_declarator1(((TypePTR *) type)->t);
		printf("(* ");
		type_flags_print(type);
		return;
	case TYPE_ARRAY:
		type_print_declarator1(((TypeARRAY *) type)->t);
		printf("(");
		return;
	default:
		return;
	}
}

static void type_print_vardecl(unsigned int flags, Type *type, const char *name,
			       bool simple);
static void type_print_declarator2(Type *type)
{
	switch(type->type) {
	case TYPE_VOID:
	case TYPE_INT:
	case TYPE_SHORT:
	case TYPE_LONG:
	case TYPE_LLONG:
	case TYPE_CHAR:
	case TYPE_SCHAR:
	case TYPE_UINT:
	case TYPE_USHORT:
	case TYPE_ULONG:
	case TYPE_ULLONG:
	case TYPE_UCHAR:
	case TYPE_BOOL:
	case TYPE_FLOAT:
	case TYPE_DOUBLE:
	case TYPE_LDOUBLE:
	case TYPE_INT128:
	case TYPE_UINT128:
	case TYPE_TYPEDEF:
	case TYPE_TYPEOF:
	case TYPE_AUTO:
		return;
	case TYPE_FUN:
		printf("(");
		Type *p;
		int i;
		avec_foreach(&((TypeFUN *) type)->at, p, i) {
			if (i) printf(", ");
			type_print_annot(type_get_basic(p), false);
			printf(" ");
			type_print_declarator1(p);
			type_print_declarator2(p);
		}
		if (((TypeFUN *) type)->at.length == 0)
			printf("void");
		if (((TypeFUN *) type)->va_arg)
			printf(", ...");
		printf(")");
		type_print_declarator2(((TypeFUN *) type)->rt);
		return;
	case TYPE_PTR:
		printf(")");
		type_print_declarator2(((TypePTR *) type)->t);
		return;
	case TYPE_ARRAY:
		printf("[");
		type_flags_print(type);
		if (((TypeARRAY *) type)->n) {
			if (((TypeARRAY *) type)->flags & TFLAG_ARRAY_STATIC)
				printf("static ");
			expr_print1(((TypeARRAY *) type)->n, false);
		}
		printf("])");
		type_print_declarator2(((TypeARRAY *) type)->t);
		return;
	default:
		return;
	}
}

static void type_print_vardecl(unsigned int flags, Type *type, const char *name, bool simple)
{
	if (simple) {
		type_print_annot(type, true);
		return;
	}
	decl_flags_print(flags);
	type_print_annot(type_get_basic(type), false);
	printf(" ");
	type_print_declarator1(type);
	if (name) {
		printf("%s", name);
	} else {
		printf("/* unnamed */");
	}
	type_print_declarator2(type);
}

static void type_print_fundecl(unsigned int flags, TypeFUN *type, StmtBLOCK *args, const char *name, Attribute *attrs)
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
		attrs_print(attrs);
		printf(" ");
	}
	type_print_annot(type_get_basic(rt), false);
	printf(" ");
	type_print_declarator1(rt);
	printf("%s(", name);
	if (args) {
		Stmt *p;
		int i;
		avec_foreach(&args->items, p, i) {
			StmtVARDECL *p1 = (StmtVARDECL *) p;
			if (i)
				printf(", ");
			type_print_vardecl(
				p1->flags,
				p1->type,
				p1->name,
				false);
			if (p1->ext.gcc_attribute) {
				printf(" ");
				attrs_print(p1->ext.gcc_attribute);
			}
		}
	} else {
		printf("void");
	}
	if (type->va_arg)
		printf(", ...");
	printf(")");
	type_print_declarator2(rt);
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

static const char *uopname(ExprUnOp op)
{
	switch (op) {
	case EXPR_OP_NEG: return "neg";
	case EXPR_OP_POS: return "pos";
	case EXPR_OP_NOT: return "!";
	case EXPR_OP_BNOT: return "~";

	case EXPR_OP_ADDROF: return "addrof";
	case EXPR_OP_DEREF: return "deref";

	case EXPR_OP_PREINC: return "preinc";
	case EXPR_OP_POSTINC: return "postinc";
	case EXPR_OP_PREDEC: return "predec";
	case EXPR_OP_POSTDEC: return "postdec";
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
		default:
			if (v[i] >= 0 && v[i] < 32) {
				printf("\"\"\\x%x\"\"", v[i]); break;
			} else {
				putchar(v[i]); break;
			}
		}
	}
}

static void expr_print(Expr *h, bool simple)
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
		if (e->wide) putchar('L');
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
		if (e->wide) putchar('L');
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
		expr_print1(e->a, simple);
		printf(" . %s", e->id);
		break;
	}
	case EXPR_PMEM: {
		ExprMEM *e = (ExprMEM *) h;
		expr_print1(e->a, simple);
		printf(" -> %s", e->id);
		break;
	}
	case EXPR_CALL: {
		ExprCALL *e = (ExprCALL *) h;
		expr_print1(e->func, simple);
		printf("(");
		Expr *p;
		int i;
		avec_foreach(&e->args, p, i) {
			if (i) printf(", ");
			expr_print2(p, simple);
		}
		printf(")");
		break;
	}
	case EXPR_BOP: {
		ExprBOP *e = (ExprBOP *) h;
		if (e->op == EXPR_OP_IDX) {
			expr_print1(e->a, simple);
			printf("["); expr_print(e->b, simple); printf("]");
		} else {
			expr_print1(e->a, simple);
			printf(" %s ", bopname(e->op));
			expr_print1(e->b, simple);
		}
		break;
	}
	case EXPR_UOP: {
		ExprUOP *e = (ExprUOP *) h;
		switch (e->op) {
		case EXPR_OP_NEG: printf("- "); expr_print1(e->e, simple); break;
		case EXPR_OP_POS: printf("+ "); expr_print1(e->e, simple); break;
		case EXPR_OP_NOT: printf("! "); expr_print1(e->e, simple); break;
		case EXPR_OP_BNOT: printf("~ "); expr_print1(e->e, simple); break;

		case EXPR_OP_ADDROF: printf("& "); expr_print1(e->e, simple); break;
		case EXPR_OP_DEREF: printf("* "); expr_print1(e->e, simple); break;

		case EXPR_OP_PREINC: printf("++ "); expr_print1(e->e, simple); break;
		case EXPR_OP_POSTINC: expr_print1(e->e, simple); printf(" ++"); break;
		case EXPR_OP_PREDEC: printf("-- "); expr_print1(e->e, simple); break;
		case EXPR_OP_POSTDEC: expr_print1(e->e, simple); printf(" --"); break;

		case EXPR_OP_ADDROFLABEL:
			printf("&& %s", ((ExprIDENT * )e->e)->id); break;
		default: abort();
		}
		break;
	}
	case EXPR_COND: {
		ExprCOND *e = (ExprCOND *) h;
		expr_print1(e->c, simple);
		printf(" ? ");
		if (e->a)
			expr_print1(e->a, simple);
		printf(" : ");
		expr_print1(e->b, simple);
		break;
	}
	case EXPR_EXPR: {
		ExprUOP *e = (ExprUOP *) h;
		expr_print(e->e, simple);
		break;
	}
	case EXPR_CAST: {
		ExprCAST *e = (ExprCAST *) h;
		printf("(");
		type_print_vardecl(0, e->t, "", simple);
		if (e->e->type == EXPR_INIT) {
			printf(") ");
			expr_print(e->e, simple);
		} else {
			printf(") ");
			expr_print1(e->e, simple);
		}
		break;
	}
	case EXPR_SIZEOF: {
		ExprSIZEOF *e = (ExprSIZEOF *) h;
		printf("sizeof ");
		expr_print1(e->e, simple);
		break;
	}
	case EXPR_SIZEOFT: {
		ExprSIZEOFT *e = (ExprSIZEOFT *) h;
		printf("sizeof (");
		type_print_vardecl(0, e->t, "", simple);
		printf(")");
		break;
	}
	case EXPR_ALIGNOF: {
		ExprALIGNOF *e = (ExprALIGNOF *) h;
		if (e->t->type == TYPE_TYPEOF) {
			TypeTYPEOF *t = (TypeTYPEOF *) e->t;
			printf("__alignof__(");
			expr_print(t->e, simple);
			printf(")");
		} else {
			printf("_Alignof (");
			type_print_vardecl(0, e->t, "", simple);
			printf(")");
		}
		break;
	}
	case EXPR_INIT: {
		ExprINIT *e = (ExprINIT *) h;
		printf("{");
		ExprINITItem p;
		int i;
		avec_foreach(&e->items, p, i) {
			if (i) printf(", ");
			if (p.designator) {
				Designator *d = p.designator;
				while (d) {
					switch(d->type) {
					case DES_INDEX:
						printf("[");
						expr_print(d->index, simple);
						printf("]");
						break;
					case DES_FIELD:
						printf(".%s", d->field);
						break;
					case DES_INDEXRANGE:
						printf("[");
						expr_print(d->index, simple);
						printf(" ... ");
						expr_print(d->indexhigh, simple);
						printf("]");
						break;
					}
					d = d->next;
				}
				printf(" = ");
			}
			expr_print2(p.value, simple);
		}
		printf("}");
		break;
	}
	case EXPR_VASTART: {
		ExprVASTART *e = (ExprVASTART *) h;
		printf("__builtin_va_start(");
		expr_print2(e->ap, simple);
		printf(", ");
		printf("%s ", e->last);
		printf(")");
		break;
	}
	case EXPR_VAARG: {
		ExprVAARG *e = (ExprVAARG *) h;
		printf("__builtin_va_arg(");
		expr_print2(e->ap, simple);
		printf(", ");
		type_print_vardecl(0, e->type, "", simple);
		printf(")");
		break;
	}
	case EXPR_VAEND: {
		ExprVAEND *e = (ExprVAEND *) h;
		printf("__builtin_va_end(");
		expr_print1(e->ap, simple);
		printf(")");
		break;
	}
	case EXPR_OFFSETOF: {
		ExprOFFSETOF *e = (ExprOFFSETOF *) h;
		printf("__builtin_offsetof( ");
		type_print_vardecl(0, e->type, "", simple);
		printf(", ");
		print_memlist(e->mem);
		printf(")");
		break;
	}
	case EXPR_STMT: {
		ExprSTMT *e = (ExprSTMT *) h;
		if (simple)
			printf("({/* ... */})");
		else {
			printf("(");
			stmt_print((Stmt *) e->s, 0);
			printf(")");
		}
		break;
	}
	case EXPR_GENERIC: {
		ExprGENERIC *e = (ExprGENERIC *) h;
		printf("_Generic(");
		expr_print2(e->expr, simple);
		GENERICPair *item;
		int i;
		avec_foreach_ptr(&e->items, item, i) {
			printf(", ");
			if (item->type)
				type_print_vardecl(0, item->type, "", simple);
			else
				printf("default");
			printf(": ");
			expr_print2(item->expr, simple);
		}
		printf(")");
		break;
	}
	case EXPR_TYPESCOMPATIBLE: {
		ExprTYPESCOMPATIBLE *e = (ExprTYPESCOMPATIBLE *) h;
		printf("__builtin_types_compatible_p( ");
		type_print_vardecl(0, e->type1, "", simple);
		printf(", ");
		type_print_vardecl(0, e->type2, "", simple);
		printf(")");
		break;
	}
	}
}

static void print_level(int level)
{
	for (int i = 0; i < level; i++)
		printf("    ");
}

static void stmt_printb(Stmt *h, int level)
{
	if (h->type == STMT_BLOCK)
		level--;
	stmt_print(h, level);
}

static void attrs_print(Attribute *attrs)
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
				expr_print2(p, false);
			}
			printf(")");
		}
		if (a->next)
			printf(", ");
	}
	printf("))");
}

static void stmt_print(Stmt *h, int level)
{
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
		expr_print(s->expr, false);
		printf(";\n");
		break;
	}
	case STMT_IF: {
		StmtIF *s = (StmtIF *) h;
		printf("if (");
		expr_print_cond(s->cond, false);
		printf(")\n");
		stmt_printb(s->body1, level + 1);
		if (s->body2) {
			print_level(level);
			printf("else\n");
			stmt_printb(s->body2, level + 1);
		}
		printf("\n");
		break;
	}
	case STMT_WHILE: {
		StmtWHILE *s = (StmtWHILE *) h;
		printf("while (");
		expr_print_cond(s->cond, false);
		printf(")\n");
		stmt_printb(s->body, level + 1);
		printf("\n");
		break;
	}
	case STMT_DO: {
		StmtDO *s = (StmtDO *) h;
		printf("do\n");
		stmt_printb(s->body, level + 1);
		print_level(level);
		printf("while (");
		expr_print_cond(s->cond, false);
		printf(");\n");
		break;
	}
	case STMT_FOR: {
		StmtFOR *s = (StmtFOR *) h;
		printf("for (");
		if (s->init) expr_print(s->init, false);
		printf("; ");
		if (s->cond) expr_print_cond(s->cond, false);
		printf("; ");
		if (s->step) expr_print(s->step, false);
		printf(")\n");
		stmt_printb(s->body, level + 1);
		printf("\n");
		break;
	}
	case STMT_FOR99: {
		StmtFOR99 *s = (StmtFOR99 *) h;
		printf("for (");
		stmt_print((Stmt *) s->init, 0);
		if (s->cond) expr_print_cond(s->cond, false);
		printf("; ");
		if (s->step) expr_print(s->step, false);
		printf(")\n");
		stmt_printb(s->body, level + 1);
		printf("\n");
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
		expr_print(s->expr, false);
		printf(")\n");
		stmt_printb(s->body, level + 1);
		printf("\n");
		break;
	}
	case STMT_CASE: {
		StmtCASE *s = (StmtCASE *) h;
		printf("case ");
		expr_print1(s->expr, false);
		printf(":\n");
		stmt_print(s->stmt, level + 1);
		break;
	}
	case STMT_DEFAULT: {
		StmtDEFAULT *s = (StmtDEFAULT *) h;
		printf("default:\n");
		stmt_print(s->stmt, level + 1);
		break;
	}
	case STMT_LABEL: {
		StmtLABEL *s = (StmtLABEL *) h;
		printf("%s:\n", s->name);
		stmt_print(s->stmt, level + 1);
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
			expr_print1(s->expr, false);
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
			attrs_print(s->attrs);
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
			stmt_print(p, level + 1);
		}
		print_level(level);
		printf("}\n");
		break;
	}
	case STMT_FUNDECL: {
		StmtFUNDECL *s = (StmtFUNDECL *) h;
		printf("// fundecl: %s, type: ", s->name);
		type_print_annot(&s->type->h, true);
		printf("\n");
		print_level(level);
		type_print_fundecl(s->flags, s->type, s->args, s->name, s->ext.gcc_attribute);
		if (s->body) {
			printf("\n");
			stmt_print(&s->body->h, level);
		} else {
			printf(";\n");
		}
		break;
	}
	case STMT_VARDECL: {
		StmtVARDECL *s = (StmtVARDECL *) h;
		if (s->name)
			printf("// vardecl: %s, type: ", s->name);
		else
			printf("// vardecl: /* unnamed */, type: ");
		type_print_annot(s->type, true);
		if (s->bitfield) {
			printf(", bitfield : ");
			expr_print1(s->bitfield, true);
		}
		printf("\n");
		print_level(level);
		type_print_vardecl(s->flags, s->type, s->name, false);
		if (s->bitfield) {
			printf(" : ");
			expr_print1(s->bitfield, false);
		}
		if (s->ext.gcc_asm_name) {
			printf(" __asm__(\"");
			int len = strlen(s->ext.gcc_asm_name);
			print_quoted(s->ext.gcc_asm_name, len);
			printf("\")");
		}
		if (s->ext.gcc_attribute) {
			printf(" ");
			attrs_print(s->ext.gcc_attribute);
		}
		if (s->init) {
			printf(" = ");
			expr_print2(s->init, false);
		}
		printf(";\n");
		break;
	}
	case STMT_TYPEDEF: {
		StmtTYPEDEF *s = (StmtTYPEDEF *) h;
		printf("// typedef: %s, type: ", s->name);
		type_print_annot(s->type, true);
		printf("\n");
		print_level(level);
		if (s->name) {
			if (strcmp(s->name, "_Float32") == 0 ||
			    strcmp(s->name, "_Float64") == 0 ||
			    strcmp(s->name, "_Float32x") == 0 ||
			    strcmp(s->name, "_Float64x") == 0)
				printf("// ");
		}
		printf("typedef ");
		type_print_vardecl(0, s->type, s->name, false);
		if (s->ext.gcc_attribute) {
			printf(" ");
			attrs_print(s->ext.gcc_attribute);
		}
		printf(";\n");
		break;
	}
	case STMT_DECLS: {
		StmtDECLS *s = (StmtDECLS *) h;
		Stmt *p;
		int i;
		avec_foreach(&s->items, p, i) {
			stmt_print(p, level);
		}
		break;
	}
	case STMT_GOTOADDR: {
		StmtGOTOADDR *s = (StmtGOTOADDR *) h;
		printf("goto ");
		expr_print(s->expr, false);
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
		expr_print1(s->low, false);
		printf(" ... ");
		expr_print1(s->high, false);
		printf(":\n");
		stmt_print(s->stmt, level + 1);
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
			expr_print(oper->variable, false);
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
			expr_print(oper->variable, false);
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
		expr_print2(s->expr, false);
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

void toplevel_print(StmtBLOCK *s)
{
	Stmt *p;
	int i;
	avec_foreach(&s->items, p, i) {
		stmt_print(p, 0);
	}
}
