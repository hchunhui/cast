#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <cast/allocator.h>
#include <cast/lexer.h>
#include <cast/parser.h>
#include <cast/printer.h>
#include <cast/map.h>

typedef struct {
	map_int_t managed_symbols;
	int managed_count;
} Patch;

#include <string.h>

BEGIN_MANAGED
static char *addprefix(const char *old)
{
	const char *prefix = "__managed_";
	int len = strlen(old) + strlen(prefix);
	char *newname = __new_(len + 1);
	strcpy(newname, prefix);
	strcat(newname, old);
	return newname;
}

static void patch_decl(Patch *ctx, Stmt *h)
{
	switch (h->type) {
	case STMT_FUNDECL: {
		StmtFUNDECL *s = (StmtFUNDECL *) h;
		if (s->name && (s->flags & DFLAG_MANAGED)) {
			map_set(&ctx->managed_symbols, s->name, 1);
			s->name = addprefix(s->name);
			Type *t = typePTR(typeTYPEDEF("Context", 0), 0);
			typeFUN_prepend(s->type, t);
			if (s->args == NULL) {
				s->args = stmtBLOCK();
			}
			stmtBLOCK_prepend(
				s->args,
				stmtVARDECL(0, "__myctx", t, NULL, NULL,
					    (Extension) {}));
		}
		break;
	}
	case STMT_DECLS: {
		StmtDECLS *s = (StmtDECLS *) h;
		int i;
		Stmt *s1;
		avec_foreach(&s->items, s1, i) {
			patch_decl(ctx, s1);
		}
		break;
	}
	default:
		break;
	}
}

static void patch_call1_expr(Patch *ctx, Expr *h)
{
	switch (h->type) {
	case EXPR_IDENT: {
		ExprIDENT *e = (ExprIDENT *) h;
		break;
	}
	case EXPR_MEM: {
		ExprMEM *e = (ExprMEM *) h;
		patch_call1_expr(ctx, e->a);
		break;
	}
	case EXPR_PMEM: {
		ExprPMEM *e = (ExprPMEM *) h;
		patch_call1_expr(ctx, e->a);
		break;
	}
	case EXPR_CALL: {
		ExprCALL *e = (ExprCALL *) h;
		if (ctx->managed_count == 0)
			break;
		if (e->func->type == EXPR_IDENT) {
			ExprIDENT *i = (ExprIDENT *) e->func;
			if (map_get(&ctx->managed_symbols, i->id)) {
				i->id = addprefix(i->id);
				exprCALL_prepend(
					e,
					exprIDENT("__myctx"));
			} else if (strcmp(i->id, "__new_") == 0) {
				e->func = exprIDENT("allocator_memalloc");
				exprCALL_prepend(e,
						 exprPMEM(exprIDENT("__myctx"),
							 "allocator"));
			}
		}
		Expr *e1;
		int i;
		avec_foreach(&e->args, e1, i) {
			patch_call1_expr(ctx, e1);
		}
		break;
	}
	case EXPR_BOP: {
		ExprBOP *e = (ExprBOP *) h;
		patch_call1_expr(ctx, e->a);
		patch_call1_expr(ctx, e->b);
		break;
	}
	case EXPR_UOP: {
		ExprUOP *e = (ExprUOP *) h;
		patch_call1_expr(ctx, e->e);
		break;
	}
	case EXPR_COND: {
		ExprCOND *e = (ExprCOND *) h;
		patch_call1_expr(ctx, e->c);
		if (e->a)
			patch_call1_expr(ctx, e->a);
		patch_call1_expr(ctx, e->b);
		break;
	}
	case EXPR_CAST: {
		ExprCAST *e = (ExprCAST *) h;
		patch_call1_expr(ctx, e->e);
		break;
	}
	case EXPR_SIZEOF: {
		ExprSIZEOF *e = (ExprSIZEOF *) h;
		patch_call1_expr(ctx, e->e);
		break;
	}
	case EXPR_SIZEOFT: {
		ExprSIZEOFT *e = (ExprSIZEOFT *) h;
		break;
	}
	case EXPR_INIT: {
		ExprINIT *e = (ExprINIT *) h;
		ExprINITItem item;
		int i;
		avec_foreach(&e->items, item, i) {
			patch_call1_expr(ctx, item.value);
		}
		break;
	}
	case EXPR_GENERIC: {
		ExprGENERIC *e = (ExprGENERIC *) h;
		patch_call1_expr(ctx, e->expr);
		GENERICPair *item;
		int i;
		avec_foreach_ptr(&e->items, item, i) {
			patch_call1_expr(ctx, item->expr);
		}
		break;
	}
	default:
		break;
	}
}

static void patch_call1_stmt(Patch *ctx, Stmt *h)
{
	switch (h->type) {
	case STMT_EXPR: {
		StmtEXPR *s = (StmtEXPR *) h;
		patch_call1_expr(ctx, s->expr);
		break;
	}
	case STMT_IF: {
		StmtIF *s = (StmtIF *) h;
		patch_call1_expr(ctx, s->cond);
		patch_call1_stmt(ctx, s->body1);
		if (s->body2) {
			patch_call1_stmt(ctx, s->body2);
		}
		break;
	}
	case STMT_WHILE: {
		StmtWHILE *s = (StmtWHILE *) h;
		patch_call1_expr(ctx, s->cond);
		patch_call1_stmt(ctx, s->body);
		break;
	}
	case STMT_DO: {
		StmtDO *s = (StmtDO *) h;
		patch_call1_stmt(ctx, s->body);
		patch_call1_expr(ctx, s->cond);
		break;
	}
	case STMT_FOR: {
		StmtFOR *s = (StmtFOR *) h;
		if (s->init) patch_call1_expr(ctx, s->init);
		if (s->cond) patch_call1_expr(ctx, s->cond);
		if (s->step) patch_call1_expr(ctx, s->step);
		patch_call1_stmt(ctx, s->body);
		break;
	}
	case STMT_FOR99: {
		StmtFOR99 *s = (StmtFOR99 *) h;
		patch_call1_stmt(ctx, (Stmt *) s->init);
		if (s->cond) patch_call1_expr(ctx, s->cond);
		if (s->step) patch_call1_expr(ctx, s->step);
		patch_call1_stmt(ctx, s->body);
		break;
	}
	case STMT_BREAK: {
		break;
	}
	case STMT_CONTINUE: {
		break;
	}
	case STMT_SWITCH: {
		StmtSWITCH *s = (StmtSWITCH *) h;
		patch_call1_expr(ctx, s->expr);
		patch_call1_stmt(ctx, s->body);
		break;
	}
	case STMT_CASE: {
		StmtCASE *s = (StmtCASE *) h;
		patch_call1_expr(ctx, s->expr);
		patch_call1_stmt(ctx, s->stmt);
		break;
	}
	case STMT_DEFAULT: {
		StmtDEFAULT *s = (StmtDEFAULT *) h;
		patch_call1_stmt(ctx, s->stmt);
		break;
	}
	case STMT_LABEL: {
		StmtLABEL *s = (StmtLABEL *) h;
		patch_call1_stmt(ctx, s->stmt);
		break;
	}
	case STMT_GOTO: {
		StmtGOTO *s = (StmtGOTO *) h;
		break;
	}
	case STMT_RETURN: {
		StmtRETURN *s = (StmtRETURN *) h;
		if (s->expr) {
			patch_call1_expr(ctx, s->expr);
		}
		break;
	}
	case STMT_SKIP: {
		break;
	}
	case STMT_BLOCK: {
		StmtBLOCK *s = (StmtBLOCK *) h;
		Stmt *s1;
		int i;
		avec_foreach(&s->items, s1, i) {
			patch_call1_stmt(ctx, s1);
		}
		break;
	}
	case STMT_FUNDECL: {
		StmtFUNDECL *s = (StmtFUNDECL *) h;
		break;
	}
	case STMT_VARDECL: {
		StmtVARDECL *s = (StmtVARDECL *) h;
		if (s->init) {
			patch_call1_expr(ctx, s->init);
		}
		break;
	}
	case STMT_LABELDECL: {
		break;
	}
	case STMT_TYPEDEF: {
		StmtTYPEDEF *s = (StmtTYPEDEF *) h;
		break;
	}
	case STMT_DECLS: {
		StmtDECLS *s = (StmtDECLS *) h;
		Stmt *s1;
		int i;
		avec_foreach(&s->items, s1, i) {
			patch_call1_stmt(ctx, s1);
		}
		break;
	}
	case STMT_GOTOADDR: {
		StmtGOTOADDR *s = (StmtGOTOADDR *) h;
		patch_call1_expr(ctx, s->expr);
		break;
	}
	case STMT_CASERANGE: {
		StmtCASERANGE *s = (StmtCASERANGE *) h;
		patch_call1_expr(ctx, s->low);
		patch_call1_expr(ctx, s->high);
		patch_call1_stmt(ctx, s->stmt);
		break;
	}
	case STMT_ASM: {
		break;
	}
	case STMT_STATICASSERT: {
		break;
	}
	case STMT_PRAGMA: {
		break;
	}
	default:
		abort();
	}
}

static void patch_call_stmt(Patch *ctx, StmtBLOCK *s)
{
	Stmt *s1;
	int i;
	avec_foreach(&s->items, s1, i) {
		patch_call1_stmt(ctx, s1);
	}
}

static void patch_call(Patch *ctx, Stmt *h)
{
	switch (h->type) {
	case STMT_FUNDECL: {
		StmtFUNDECL *s = (StmtFUNDECL *) h;
		if (s->flags & DFLAG_MANAGED)
			ctx->managed_count++;
		if (s->body)
			patch_call_stmt(ctx, s->body);
		if (s->flags & DFLAG_MANAGED)
			ctx->managed_count--;
		break;
	}
	case STMT_DECLS: {
		StmtDECLS *s = (StmtDECLS *) h;
		int i;
		Stmt *s1;
		avec_foreach(&s->items, s1, i) {
			patch_call(ctx, s1);
		}
		break;
	}
	default:
		break;
	}
}

static void patch(StmtBLOCK *s)
{
	Patch pctx;
	map_init(&pctx.managed_symbols);

	Stmt *s1;
	int i;
	avec_foreach(&s->items, s1, i) {
		patch_decl(&pctx, s1);
	}

	pctx.managed_count = 0;
	avec_foreach(&s->items, s1, i) {
		patch_call(&pctx, s1);
	}
	map_deinit(&pctx.managed_symbols);
}
END_MANAGED

BEGIN_MANAGED
StmtBLOCK *elim_unused(StmtBLOCK *tu);
END_MANAGED

static int main1(const char *file)
{
	int ret = 0;
	TextStream *ts = text_stream_new(file);
	Lexer *l = lexer_new(ts);
	Parser *p = parser_new(l);

	Allocator *a = allocator_new();
	Context *ctx = context_new(a);
	StmtBLOCK *translation_unit = CALL_MANAGED(parse_translation_unit, ctx, p);
	if (translation_unit) {
		fprintf(stderr, "cast: preprocessing %s\n", lexer_report_file(l));
		CALL_MANAGED(patch, ctx, translation_unit);
#ifdef __CAST_MANAGED__
		translation_unit = CALL_MANAGED(elim_unused, ctx, translation_unit);
#endif
		Printer *pt = printer_new();
		printer_print_translation_unit(pt, translation_unit);
		printer_delete(pt);
	} else {
		fprintf(stderr, "%s:%d: syntax error\n",
			lexer_report_path(l),
			lexer_report_line(l));
		ret = 1;
	}

	parser_delete(p);
	context_delete(ctx);
	lexer_delete(l);
	text_stream_delete(ts);
	allocator_delete(a);
	return ret;
}

int main(int argc, char *argv[])
{
	return main1(argc == 1 ? "-" : argv[1]);
}
