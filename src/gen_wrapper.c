#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <cast/allocator.h>
#include <cast/lexer.h>
#include <cast/parser.h>
#include <cast/printer.h>
#include <cast/map.h>

BEGIN_MANAGED
static const char *__new_cstring(const char *s)
{
	int len = strlen(s);
	char *str = __new_(len + 1);
	memcpy(str, s, len + 1);
	return str;
}

StmtBLOCK *wrap(StmtBLOCK *s)
{
	StmtBLOCK *out = stmtBLOCK();
	Stmt *p;
	int i;

	stmtBLOCK_append(
		out,
		stmtVARDECL(DFLAG_STATIC,
			    "__handle",
			    typePTR((Type *) typeVOID(0), 0),
			    NULL,
			    NULL,
			    (Extension){}));

	avec_foreach(&s->items, p, i) {
		switch (p->type) {
		case STMT_FUNDECL: {
			StmtFUNDECL *s = (StmtFUNDECL *) p;
			if (!s->body) {
				if (s->type->va_arg && s->args) {
					fprintf(stderr, "unable to wrap %s()\n",
						s->name);
					continue;
				}

				StmtBLOCK *body = stmtBLOCK();
				StmtBLOCK *args = NULL;
				Stmt *q;
				int j;
				if (s->args) {
					char buf[16];
					int ct = 0;

					args = stmtBLOCK();
					avec_foreach(&(s->args->items), q, j) {
						StmtVARDECL *q1 = (StmtVARDECL *) q;
						const char *name = q1->name;
						if (!name) {
							sprintf(buf, "__a%d", ct++);
							name = __new_cstring(buf);
						}
						stmtBLOCK_append(
							args,
							stmtVARDECL(q1->flags,
								    name,
								    q1->type,
								    q1->init,
								    q1->bitfield,
								    q1->ext));
					}
				}
				stmtBLOCK_append(
					body,
					stmtVARDECL(DFLAG_STATIC,
						    "f",
						    typePTR((Type *) s->type, 0),
						    NULL,
						    NULL,
						    (Extension){}));
				ExprCALL *call_dlsym = exprCALL(exprIDENT("dlsym"));
				exprCALL_append(call_dlsym, exprIDENT("__handle"));
				exprCALL_append(call_dlsym,
						exprSTRING_CST(
							s->name,
							strlen(s->name) + 1,
							WCK_NONE));
				ExprCALL *call_f = exprCALL(exprIDENT("f"));
				if (s->args) {
					avec_foreach(&(args->items), q, j) {
						StmtVARDECL *q1 = (StmtVARDECL *) q;
						exprCALL_append(call_f,
								exprIDENT(q1->name));
					}
				}
				stmtBLOCK_append(
					body,
					stmtIF(exprUOP(EXPR_OP_NOT, exprIDENT("f")),
					       stmtEXPR(
						       exprBOP(EXPR_OP_ASSIGN,
							       exprIDENT("f"),
							       (Expr *) call_dlsym)),
					       NULL));
				if (s->type->rt->type == TYPE_VOID) {
					stmtBLOCK_append(
						body,
						stmtEXPR((Expr *) call_f));
				} else {
					stmtBLOCK_append(
						body,
						stmtRETURN((Expr *) call_f));
				}
				Stmt *f = stmtFUNDECL(s->flags & ~DFLAG_EXTERN,
						      s->name, s->type,
						      args, body, s->ext);
				stmtBLOCK_append(out, f);
			}
			break;
		}
		}
	}
	return out;
}
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
		StmtBLOCK *out = CALL_MANAGED(wrap, ctx, translation_unit);
		Printer *pt = printer_new();
		printer_print_translation_unit(pt, out);
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
