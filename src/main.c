#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <ctree/lexer.h>
#include <ctree/parser.h>

static void decl_flags_print(unsigned int flags)
{
	if (flags & DFLAG_EXTERN)
		printf("extern ");
	if (flags & DFLAG_STATIC)
		printf("static ");
	if (flags & DFLAG_INLINE)
		printf("inline ");
}

static void expr_print(Expr *h);
static void stmt_print(Stmt *h, int level);
static void type_print_annot(Type *type, bool simple)
{
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
	case TYPE_PTR:
		printf("pointer( ");
		type_print_annot(((TypePTR *) type)->t, simple);
		printf(" )");
		break;
	case TYPE_ARRAY:
		printf("array( ");
		type_print_annot(((TypeARRAY *) type)->t, simple);
		printf(" ,");
		if (((TypeARRAY *) type)->n)
			expr_print(((TypeARRAY *) type)->n);
		printf(")");
		break;
	case TYPE_FUN:
		printf("(");
		Type *p;
		int i;
		vec_foreach(&((TypeFUN *) type)->at, p, i) {
			if (i) printf(", ");
			type_print_annot(p, simple);
		}
		printf(") -> ");
		type_print_annot(((TypeFUN *) type)->rt, simple);
		break;
	case TYPE_TYPEDEF:
		printf("%s", ((TypeTYPEDEF *) type)->name);
		break;
	case TYPE_STRUCT: {
		TypeSTRUCT *t = (TypeSTRUCT *) type;
		printf("%s", t->is_union ? "union" : "struct");
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
				vec_foreach_ptr(&(t->list->items), p, i) {
					printf("%s", p->id);
					if (p->val) {
						printf(" =");
						expr_print(p->val);
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
	case TYPE_UINT:
	case TYPE_USHORT:
	case TYPE_ULONG:
	case TYPE_ULLONG:
	case TYPE_UCHAR:
	case TYPE_BOOL:
	case TYPE_FLOAT:
	case TYPE_DOUBLE:
	case TYPE_LDOUBLE:
	case TYPE_TYPEDEF:
	case TYPE_STRUCT:
	case TYPE_ENUM:
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
	case TYPE_UINT:
	case TYPE_USHORT:
	case TYPE_ULONG:
	case TYPE_ULLONG:
	case TYPE_UCHAR:
	case TYPE_BOOL:
	case TYPE_FLOAT:
	case TYPE_DOUBLE:
	case TYPE_LDOUBLE:
	case TYPE_TYPEDEF:
		return;
	case TYPE_FUN:
		type_print_declarator1(((TypeFUN *) type)->rt);
		return;
	case TYPE_PTR:
		type_print_declarator1(((TypePTR *) type)->t);
		printf("( * ");
		return;
	case TYPE_ARRAY:
		type_print_declarator1(((TypeARRAY *) type)->t);
		printf("( ");
		return;
	default:
		return;
	}
}

static void type_print_vardecl(unsigned int flags, Type *type, const char *name);
static void type_print_declarator2(Type *type)
{
	switch(type->type) {
	case TYPE_VOID:
	case TYPE_INT:
	case TYPE_SHORT:
	case TYPE_LONG:
	case TYPE_LLONG:
	case TYPE_CHAR:
	case TYPE_UINT:
	case TYPE_USHORT:
	case TYPE_ULONG:
	case TYPE_ULLONG:
	case TYPE_UCHAR:
	case TYPE_BOOL:
	case TYPE_FLOAT:
	case TYPE_DOUBLE:
	case TYPE_LDOUBLE:
	case TYPE_TYPEDEF:
		return;
	case TYPE_FUN:
		printf("( ");
		Type *p;
		int i;
		vec_foreach(&((TypeFUN *) type)->at, p, i) {
			if (i) printf(", ");
			type_print_annot(type_get_basic(type), false);
			printf(" ");
			type_print_declarator1(p);
			type_print_declarator2(p);
		}
		printf(" )");
		type_print_declarator2(((TypeFUN *) type)->rt);
		return;
	case TYPE_PTR:
		printf(" )");
		type_print_declarator2(((TypePTR *) type)->t);
		return;
	case TYPE_ARRAY:
		printf(" [");
		if (((TypeARRAY *) type)->n)
			expr_print(((TypeARRAY *) type)->n);
		printf("] )");
		type_print_declarator2(((TypeARRAY *) type)->t);
		return;
	default:
		return;
	}
}

static void type_print_vardecl(unsigned int flags, Type *type, const char *name)
{
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

static void type_print_fundecl(unsigned int flags, TypeFUN *type, StmtBLOCK *args, const char *name)
{
	Type *rt = type->rt;
	decl_flags_print(flags);
	type_print_annot(type_get_basic(rt), false);
	printf(" ");
	type_print_declarator1(rt);
	printf("%s(", name);
	if (args) {
		Stmt *p;
		int i;
		vec_foreach(&args->items, p, i) {
			if (i)
				printf(", ");
			type_print_vardecl(
				((StmtVARDECL *) p)->flags,
				((StmtVARDECL *) p)->type,
				((StmtVARDECL *) p)->name);
		}
	}
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

static void expr_print(Expr *h)
{
	switch (h->type) {
	case EXPR_INT_CST: {
		ExprINT_CST *e = (ExprINT_CST *) h;
		printf(" %d ", e->v);
		break;
	}
	case EXPR_CHAR_CST: {
		ExprCHAR_CST *e = (ExprCHAR_CST *) h;
		printf(" '%c' ", e->v); // TODO: escape
		break;
	}
	case EXPR_STRING_CST: {
		ExprSTRING_CST *e = (ExprSTRING_CST *) h;
		putchar('"');
		for (int i = 0; i < e->len - 1; i++) {
			switch(e->v[i]) {
			case '\\': printf("\\\\"); break;
			case '\"': printf("\\\""); break;
			case '\a': printf("\\a"); break;
			case '\b': printf("\\b"); break;
			case '\f': printf("\\f"); break;
			case '\n': printf("\\n"); break;
			case '\r': printf("\\r"); break;
			case '\t': printf("\\t"); break;
			default: putchar(e->v[i]); break;
			}
		}
		putchar('"');
		break;
	}
	case EXPR_BOOL_CST: {
		ExprBOOL_CST *e = (ExprBOOL_CST *) h;
		printf(" %s ", e->v ? "true" : "false");
		break;
	}
	case EXPR_IDENT: {
		ExprIDENT *e = (ExprIDENT *) h;
		printf(" %s ", e->id);
		break;
	}
	case EXPR_MEM: {
		ExprMEM *e = (ExprMEM *) h;
		printf(" (");
		expr_print(e->a);
		printf(").%s ", e->id);
		break;
	}
	case EXPR_PMEM: {
		ExprMEM *e = (ExprMEM *) h;
		printf(" (");
		expr_print(e->a);
		printf(")->%s ", e->id);
		break;
	}
	case EXPR_CALL: {
		ExprCALL *e = (ExprCALL *) h;
		printf(" (");
		expr_print(e->func);
		printf(")(");
		Expr *p;
		int i;
		vec_foreach(&e->args, p, i) {
			if (i) printf(",");
			expr_print(p);
		}
		printf(") ");
		break;
	}
	case EXPR_BOP: {
		ExprBOP *e = (ExprBOP *) h;
		if (e->op == EXPR_OP_IDX) {
			printf("((");
			expr_print(e->a);
			printf(")[");
			expr_print(e->b);
			printf("]) ");
		} else {
			printf(" (");
			expr_print(e->a);
			printf(") %s (", bopname(e->op));
			expr_print(e->b);
			printf(") ");
		}
		break;
	}
	case EXPR_UOP: {
		ExprUOP *e = (ExprUOP *) h;
		switch (e->op) {
		case EXPR_OP_NEG: printf(" - ("); expr_print(e->e); printf(") "); break;
		case EXPR_OP_POS: printf(" + ("); expr_print(e->e); printf(") "); break;
		case EXPR_OP_NOT: printf(" ! ("); expr_print(e->e); printf(") "); break;
		case EXPR_OP_BNOT: printf(" ~ ("); expr_print(e->e); printf(") "); break;

		case EXPR_OP_ADDROF: printf(" &("); expr_print(e->e); printf(") "); break;
		case EXPR_OP_DEREF: printf(" *("); expr_print(e->e); printf(") "); break;

		case EXPR_OP_PREINC: printf(" ++ ("); expr_print(e->e); printf(") "); break;
		case EXPR_OP_POSTINC: printf(" ("); expr_print(e->e); printf(") ++ "); break;
		case EXPR_OP_PREDEC: printf(" -- ("); expr_print(e->e); printf(") "); break;
		case EXPR_OP_POSTDEC: printf(" ("); expr_print(e->e); printf(") -- "); break;
		default: abort();
		}
		break;
	}
	case EXPR_COND: {
		ExprCOND *e = (ExprCOND *) h;
		printf(" (");
		expr_print(e->c);
		printf(") ? (");
		expr_print(e->a);
		printf(") : (");
		expr_print(e->b);
		printf(") ");
		break;
	}
	case EXPR_EXPR: {
		ExprUOP *e = (ExprUOP *) h;
		expr_print(e->e);
		break;
	}
	case EXPR_CAST: {
		ExprCAST *e = (ExprCAST *) h;
		printf(" ( ");
		type_print_vardecl(0, e->t, "");
		printf(") (");
		expr_print(e->e);
		printf(") ");
		break;
	}
	case EXPR_SIZEOF: {
		ExprSIZEOF *e = (ExprSIZEOF *) h;
		printf(" sizeof (");
		expr_print(e->e);
		printf(") ");
		break;
	}
	case EXPR_SIZEOFT: {
		ExprSIZEOFT *e = (ExprSIZEOFT *) h;
		printf(" sizeof ( ");
		type_print_vardecl(0, e->t, "");
		printf(") ");
		break;
	}
	case EXPR_INIT: {
		ExprINIT *e = (ExprINIT *) h;
		printf(" {");
		Expr *p;
		int i;
		vec_foreach(&e->items, p, i) {
			if (i) printf(",");
			expr_print(p);
		}
		printf("} ");
		break;
	}
	}
}

static void print_level(int level)
{
	for (int i = 0; i < level; i++)
		printf("    ");
}

static void stmt_print(Stmt *h, int level)
{
	print_level(level);
	switch (h->type) {
	case STMT_EXPR: {
		StmtEXPR *s = (StmtEXPR *) h;
		expr_print(s->expr);
		printf(";\n");
		break;
	}
	case STMT_IF: {
		StmtIF *s = (StmtIF *) h;
		printf("if (");
		expr_print(s->cond);
		printf(")\n");
		stmt_print(s->body1, level + 1);
		if (s->body2) {
			print_level(level);
			printf("else\n");
			stmt_print(s->body2, level + 1);
		}
		printf("\n");
		break;
	}
	case STMT_WHILE: {
		StmtWHILE *s = (StmtWHILE *) h;
		printf("while (");
		expr_print(s->cond);
		printf(")\n");
		stmt_print(s->body, level + 1);
		printf("\n");
		break;
	}
	case STMT_DO: {
		StmtDO *s = (StmtDO *) h;
		printf("do\n");
		stmt_print(s->body, level + 1);
		print_level(level);
		printf("while (");
		expr_print(s->cond);
		printf(");\n");
		break;
	}
	case STMT_FOR: {
		StmtFOR *s = (StmtFOR *) h;
		printf("for (");
		if (s->init) expr_print(s->init);
		printf(";");
		if (s->cond) expr_print(s->cond);
		printf(";");
		if (s->step) expr_print(s->step);
		printf(")\n");
		stmt_print(s->body, level + 1);
		printf("\n");
		break;
	}
	case STMT_FOR99: {
		abort();
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
		expr_print(s->expr);
		printf(")");
		stmt_print((Stmt *) (s->block), level + 1);
		printf(";\n");
		break;
	}
	case STMT_CASE: {
		StmtCASE *s = (StmtCASE *) h;
		printf("case ");
		expr_print(s->expr);
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
			printf("return (");
			expr_print(s->expr);
			printf(") ;\n");
		} else {
			printf("return;\n");
		}
		break;
	}
	case STMT_SKIP: {
		printf("skip;\n");
		break;
	}
	case STMT_BLOCK: {
		StmtBLOCK *s = (StmtBLOCK *) h;
		printf("{\n");
		Stmt *p;
		int i;
		vec_foreach(&s->items, p, i) {
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
		type_print_fundecl(s->flags, s->type, s->args, s->name);
		if (s->body) {
			printf("\n");
			stmt_print(&s->body->h, level);
		} else {
			printf(" ;\n");
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
		printf("\n");
		print_level(level);
		type_print_vardecl(s->flags, s->type, s->name);
		if (s->init) {
			printf(" =");
			expr_print(s->init);
		}
		printf(" ;\n");
		break;
	}
	case STMT_TYPEDEF: {
		StmtTYPEDEF *s = (StmtTYPEDEF *) h;
		printf("// typedef: %s, type: ", s->name);
		type_print_annot(s->type, true);
		printf("\n");
		print_level(level);
		printf("typedef ");
		type_print_vardecl(0, s->type, s->name);
		printf(" ;\n");
		break;
	}
	case STMT_DECLS: {
		StmtDECLS *s = (StmtDECLS *) h;
		Stmt *p;
		int i;
		vec_foreach(&s->items, p, i) {
			stmt_print(p, level);
		}
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
	vec_foreach(&s->items, p, i) {
		stmt_print(p, 0);
	}
}

void toplevel_test(const char *file)
{
	TextStream *ts = text_stream_new(file);
//	TextStream *ts = text_stream_from_string("int main() {}");
	Lexer *l = lexer_new(ts);
	Parser *p = parser_new(l);

	StmtBLOCK *translation_unit = parse_translation_unit(p);
	if (translation_unit) {
		toplevel_print(translation_unit);
	} else {
		printf("FAIL\n");
	}

	parser_delete(p);
	lexer_delete(l);
	text_stream_delete(ts);
}

void expr_test()
{
	TextStream *ts = text_stream_from_string("a==1?2:3");
	Lexer *l = lexer_new(ts);
	Parser *p = parser_new(l);

	Expr *e = parse_expr(p);
	if (e) {
		expr_print(e);
		printf("\n");
	} else {
		printf("FAIL\n");
	}

	parser_delete(p);
	lexer_delete(l);
	text_stream_delete(ts);
}

int main(int argc, char *argv[])
{
	toplevel_test(argv[1]);
	expr_test();
	return 0;
}
