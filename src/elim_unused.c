#include <cast/parser.h>
#include <cast/map.h>

#include <cast/printer.h>

#include <stdio.h>

BEGIN_MANAGED

typedef struct {
	bool progress;
	map_int_t symbol_set;
} State;

static void mark_expr(State *st, Expr *h);
static void mark_stmt(State *st, Stmt *h);
static void mark_type(State *st, Type *type);

static void mark_attrs(State *st, Attribute *attrs)
{
	for (Attribute *a = attrs; a; a = a->next) {
		if (a->args.length) {
			Expr *p;
			int i;
			avec_foreach(&a->args, p, i) {
				mark_expr(st, p);
			}
		}
	}
}

static void mark_type(State *st, Type *type)
{
	switch(type->type) {
	case TYPE_PTR:
		mark_type(st, ((TypePTR *) type)->t);
		break;
	case TYPE_ARRAY:
		mark_type(st, ((TypeARRAY *) type)->t);
		if (((TypeARRAY *) type)->n)
			mark_expr(st, ((TypeARRAY *) type)->n);
		break;
	case TYPE_FUN: {
		Type *p;
		int i;
		vec_foreach(&((TypeFUN *) type)->at, p, i) {
			mark_type(st, p);
		}
		mark_type(st, ((TypeFUN *) type)->rt);
		break;
	}
	case TYPE_TYPEDEF:
		TypeTYPEDEF *t = (TypeTYPEDEF *) type;
		st->progress = st->progress ||
			map_get(&st->symbol_set, t->name) == NULL;
		map_set(&st->symbol_set, t->name, 1);
		break;
	case TYPE_STRUCT: {
		TypeSTRUCT *t = (TypeSTRUCT *) type;
		if (t->tag) {
			st->progress = st->progress ||
				map_get(&st->symbol_set, t->tag) == NULL;
			map_set(&st->symbol_set, t->tag, 1);
		}
		if (t->decls) {
			mark_stmt(st, (Stmt *) t->decls);
		}
		if (t->attrs)
			mark_attrs(st, t->attrs);
		break;
	}
	case TYPE_ENUM: {
		TypeENUM *t = (TypeENUM *) type;
		if (t->tag) {
			st->progress = st->progress ||
				map_get(&st->symbol_set, t->tag) == NULL;
			map_set(&st->symbol_set, t->tag, 1);
		}
		if (t->list) {
			struct EnumPair_ *p;
			int i;
			vec_foreach_ptr(&(t->list->items), p, i) {
				if (p->val) {
					mark_expr(st, p->val);
				}
			}
		}
		if (t->attrs)
			mark_attrs(st, t->attrs);
		break;
	}
	case TYPE_TYPEOF: {
		TypeTYPEOF *t = (TypeTYPEOF *) type;
		mark_expr(st, t->e);
		break;
	}
	case TYPE_AUTO:
		break;
	}
}

static void mark_expr(State *st, Expr *h)
{
	switch (h->type) {
	case EXPR_IDENT: {
		ExprIDENT *e = (ExprIDENT *) h;
		st->progress = st->progress ||
			map_get(&st->symbol_set, e->id) == NULL;
		map_set(&st->symbol_set, e->id, 1);
		break;
	}
	case EXPR_MEM: {
		ExprMEM *e = (ExprMEM *) h;
		mark_expr(st, e->a);
		break;
	}
	case EXPR_CALL: {
		ExprCALL *e = (ExprCALL *) h;
		mark_expr(st, e->func);
		Expr *p;
		int i;
		vec_foreach(&e->args, p, i) {
			mark_expr(st, p);
		}
		break;
	}
	case EXPR_BOP: {
		ExprBOP *e = (ExprBOP *) h;
		mark_expr(st, e->a);
		mark_expr(st, e->b);
		break;
	}
	case EXPR_UOP: {
		ExprUOP *e = (ExprUOP *) h;
		mark_expr(st, e->e);
		break;
	}
	case EXPR_COND: {
		ExprCOND *e = (ExprCOND *) h;
		mark_expr(st, e->c);
		if (e->a) mark_expr(st, e->a);
		mark_expr(st, e->b);
		break;
	}
	case EXPR_CAST: {
		ExprCAST *e = (ExprCAST *) h;
		mark_type(st, e->t);
		mark_expr(st, e->e);
		break;
	}
	case EXPR_SIZEOF: {
		ExprSIZEOF *e = (ExprSIZEOF *) h;
		mark_expr(st, e->e);
		break;
	}
	case EXPR_SIZEOFT: {
		ExprSIZEOFT *e = (ExprSIZEOFT *) h;
		mark_type(st, e->t);
		break;
	}
	case EXPR_ALIGNOF: {
		ExprALIGNOF *e = (ExprALIGNOF *) h;
		mark_type(st, e->t);
		break;
	}
	case EXPR_INIT: {
		ExprINIT *e = (ExprINIT *) h;
		ExprINITItem p;
		int i;
		vec_foreach(&e->items, p, i) {
			for (Designator *des = p.designator;
			     des; des = des->next) {
				switch (des->type) {
				case DES_FIELD:
					break;
				case DES_INDEX:
					mark_expr(st, des->index);
					break;
				case DES_INDEXRANGE:
					mark_expr(st, des->index);
					mark_expr(st, des->indexhigh);
					break;
				}
			}
			mark_expr(st, p.value);
		}
		break;
	}
	case EXPR_VASTART: {
		ExprVASTART *e = (ExprVASTART *) h;
		mark_expr(st, e->ap);
		break;
	}
	case EXPR_VAARG: {
		ExprVAARG *e = (ExprVAARG *) h;
		mark_expr(st, e->ap);
		mark_type(st, e->type);
		break;
	}
	case EXPR_VAEND: {
		ExprVAEND *e = (ExprVAEND *) h;
		mark_expr(st, e->ap);
		break;
	}
	case EXPR_OFFSETOF: {
		ExprOFFSETOF *e = (ExprOFFSETOF *) h;
		mark_type(st, e->type);
		break;
	}
	case EXPR_STMT: {
		ExprSTMT *e = (ExprSTMT *) h;
		mark_stmt(st, (Stmt *) e->s);
		break;
	}
	case EXPR_GENERIC: {
		ExprGENERIC *e = (ExprGENERIC *) h;
		mark_expr(st, e->expr);
		GENERICPair *item;
		int i;
		avec_foreach_ptr(&e->items, item, i) {
			if (item->type)
				mark_type(st, item->type);
			mark_expr(st, item->expr);
		}
		break;
	}
	case EXPR_TYPESCOMPATIBLE: {
		ExprTYPESCOMPATIBLE *e = (ExprTYPESCOMPATIBLE *) h;
		mark_expr(st, e->type1);
		mark_expr(st, e->type2);
		break;
	}
	case EXPR_TYPENAME: {
		ExprTYPENAME *e = (ExprTYPENAME *) h;
		mark_expr(st, e->type);
		break;
	}
	}
}

static void mark_stmt(State *st, Stmt *h)
{
	switch (h->type) {
	case STMT_EXPR: {
		StmtEXPR *s = (StmtEXPR *) h;
		mark_expr(st, s->expr);
		break;
	}
	case STMT_IF: {
		StmtIF *s = (StmtIF *) h;
		mark_expr(st, s->cond);
		mark_stmt(st, s->body1);
		if (s->body2) {
			mark_stmt(st, s->body2);
		}
		break;
	}
	case STMT_WHILE: {
		StmtWHILE *s = (StmtWHILE *) h;
		mark_expr(st, s->cond);
		mark_stmt(st, s->body);
		break;
	}
	case STMT_DO: {
		StmtDO *s = (StmtDO *) h;
		mark_stmt(st, s->body);
		mark_expr(st, s->cond);
		break;
	}
	case STMT_FOR: {
		StmtFOR *s = (StmtFOR *) h;
		if (s->init) mark_expr(st, s->init);
		if (s->cond) mark_expr(st, s->cond);
		if (s->step) mark_expr(st, s->step);
		mark_stmt(st, s->body);
		break;
	}
	case STMT_FOR99: {
		StmtFOR99 *s = (StmtFOR99 *) h;
		mark_stmt(st, (Stmt *) s->init);
		if (s->cond) mark_expr(st, s->cond);
		if (s->step) mark_expr(st, s->step);
		mark_stmt(st, s->body);
		break;
	}
	case STMT_BREAK: break;
	case STMT_CONTINUE: break;
	case STMT_SWITCH: {
		StmtSWITCH *s = (StmtSWITCH *) h;
		mark_expr(st, s->expr);
		mark_stmt(st, s->body);
		break;
	}
	case STMT_CASE: {
		StmtCASE *s = (StmtCASE *) h;
		mark_expr(st, s->expr);
		mark_stmt(st, s->stmt);
		break;
	}
	case STMT_DEFAULT: {
		StmtDEFAULT *s = (StmtDEFAULT *) h;
		mark_stmt(st, s->stmt);
		break;
	}
	case STMT_LABEL: {
		StmtLABEL *s = (StmtLABEL *) h;
		mark_stmt(st, s->stmt);
		break;
	}
	case STMT_GOTO: break;
	case STMT_RETURN: {
		StmtRETURN *s = (StmtRETURN *) h;
		if (s->expr) {
			mark_expr(st, s->expr);
		}
		break;
	}
	case STMT_SKIP: {
		StmtSKIP *s = (StmtSKIP *) h;
		if (s->attrs)
			mark_attrs(st, s->attrs);
		break;
	}
	case STMT_BLOCK: {
		StmtBLOCK *s = (StmtBLOCK *) h;
		Stmt *p;
		int i;
		vec_foreach(&s->items, p, i) {
			mark_stmt(st, p);
		}
		break;
	}
	case STMT_FUNDECL: {
		StmtFUNDECL *s = (StmtFUNDECL *) h;
		mark_type(st, (Type *) &s->type);
		if (s->body) {
			mark_stmt(st, (Stmt *) &s->body);
		}
		if (s->ext.gcc_attribute)
			mark_attrs(st, s->ext.gcc_attribute);
		break;
	}
	case STMT_VARDECL: {
		StmtVARDECL *s = (StmtVARDECL *) h;
		mark_type(st, s->type);
		if (s->init) {
			mark_expr(st, s->init);
		}
		if (s->ext.gcc_attribute)
			mark_attrs(st, s->ext.gcc_attribute);
		break;
	}
	case STMT_TYPEDEF: {
		StmtTYPEDEF *s = (StmtTYPEDEF *) h;
		mark_type(st, s->type);
		if (s->ext.gcc_attribute)
			mark_attrs(st, s->ext.gcc_attribute);
		break;
	}
	case STMT_DECLS: {
		StmtDECLS *s = (StmtDECLS *) h;
		Stmt *p;
		int i;
		vec_foreach(&s->items, p, i) {
			mark_stmt(st, p);
		}
		break;
	}
	case STMT_GOTOADDR: {
		StmtGOTOADDR *s = (StmtGOTOADDR *) h;
		mark_expr(st, s->expr);
		break;
	}
	case STMT_LABELDECL: break;
	case STMT_CASERANGE: {
		StmtCASERANGE *s = (StmtCASERANGE *) h;
		mark_expr(st, s->low);
		mark_expr(st, s->high);
		mark_stmt(st, s->stmt);
		break;
	}
	case STMT_ASM: {
		int i;
		StmtASM *s = (StmtASM *) h;
		ASMOper *oper;
		avec_foreach_ptr(&s->outputs, oper, i) {
			mark_expr(st, oper->variable);
		}
		avec_foreach_ptr(&s->inputs, oper, i) {
			mark_expr(st, oper->variable);
		}
		break;
	}
	case STMT_STATICASSERT: {
		StmtSTATICASSERT *s = (StmtSTATICASSERT *) h;
		mark_expr(st, s->expr);
		break;
	}
	}
}

static void mark_topstmt(State *st, Stmt *h)
{
	switch (h->type) {
	case STMT_FUNDECL: {
		StmtFUNDECL *s = (StmtFUNDECL *) h;
		if (s->body) {
			if (s->name)
				map_set(&st->symbol_set, s->name, 1);
			mark_type(st, (Type *) s->type);
			mark_stmt(st, (Stmt *) s->body);
			if (s->ext.gcc_attribute)
				mark_attrs(st, s->ext.gcc_attribute);
		} else if (!(s->flags & DFLAG_EXTERN) &&
			   (s->ext.gcc_attribute || s->ext.gcc_asm_name ||
			    s->ext.c11_alignas)) {
			if (s->name)
				map_set(&st->symbol_set, s->name, 1);
		}
		break;
	}
	case STMT_VARDECL: {
		StmtVARDECL *s = (StmtVARDECL *) h;
		if (!(s->flags & DFLAG_EXTERN) && s->name) {
			map_set(&st->symbol_set, s->name, 1);
			mark_type(st, s->type);
			if (s->init)
				mark_expr(st, s->init);
			if (s->ext.gcc_attribute)
				mark_attrs(st, s->ext.gcc_attribute);
		} else if ((s->flags & DFLAG_EXTERN) && s->name) {
			Attribute *attr = s->ext.gcc_attribute;
			while (attr) {
				if (strcmp(attr->name, "unused") == 0) {
					mark_attrs(st, s->ext.gcc_attribute);
					map_set(&st->symbol_set, s->name, 1);
					break;
				}
				attr = attr->next;
			}
		}
		break;
	}
	case STMT_DECLS: {
		StmtDECLS *s = (StmtDECLS *) h;
		Stmt *p;
		int i;
		vec_foreach(&s->items, p, i) {
			mark_topstmt(st, p);
		}
		break;
	}
	case STMT_ASM:
	case STMT_STATICASSERT:
		mark_stmt(st, h);
		break;
	}
}

static Type *has_structenum(Type *t)
{
	switch (t->type) {
	case TYPE_ENUM:
	case TYPE_STRUCT:
		return t;
	case TYPE_PTR: {
		TypePTR *n = (TypePTR *) t;
		return has_structenum(n->t);
	}
	case TYPE_ARRAY: {
		TypeARRAY *n = (TypeARRAY *) t;
		return has_structenum(n->t);
	}
	}
	return NULL;
}

static bool check_nested(Stmt *p)
{
	switch(p->type) {
	case STMT_VARDECL: {
		StmtVARDECL *s = (StmtVARDECL *) p;
		Type *t = has_structenum(s->type);
		if (t) {
			if (t->type == TYPE_STRUCT) {
				TypeSTRUCT *ts = (TypeSTRUCT *) t;
				if (ts->tag)
					return true;
			} else if (t->type == TYPE_ENUM) {
				TypeENUM *te = (TypeENUM *) t;
				if (te->tag)
					return true;
			}
		}
		break;
	}
	case STMT_DECLS: {
		StmtDECLS *s = (StmtDECLS *) p;
		Stmt *q;
		int i;
		avec_foreach(&s->items, q, i) {
			if (check_nested(q))
				return true;
		}
		break;
	}
	}
	return false;
}

static void mark_topstmt2(State *st, Stmt *h)
{
	switch (h->type) {
	case STMT_FUNDECL: {
		StmtFUNDECL *s = (StmtFUNDECL *) h;
		if (s->name) {
			if (map_get(&st->symbol_set, s->name)) {
				mark_type(st, (Type *) s->type);
				if (s->ext.gcc_attribute)
					mark_attrs(st, s->ext.gcc_attribute);
			}
		}
		break;
	}
	case STMT_VARDECL: {
		StmtVARDECL *s = (StmtVARDECL *) h;
		if (s->name) {
			if (map_get(&st->symbol_set, s->name)) {
				mark_type(st, s->type);
			}
		}
		if (s->type->type == TYPE_STRUCT) {
			TypeSTRUCT *t = (TypeSTRUCT *) s->type;
			if (t->tag && map_get(&st->symbol_set, t->tag)) {
				mark_type(st, s->type);
			} if (t->decls) {
				// has nested struct/enum defs?
				Stmt *p;
				int i;
				avec_foreach(&(t->decls->items), p, i) {
					if (check_nested(p)) {
						mark_type(st, s->type);
						break;
					}
				}
			}
		} else if (s->type->type == TYPE_ENUM) {
			TypeENUM *t = (TypeENUM *) s->type;
			if (t->tag && map_get(&st->symbol_set, t->tag)) {
				mark_type(st, s->type);
			} else if (t->list) {
				struct EnumPair_ *p;
				int i;
				avec_foreach_ptr(&(t->list->items), p, i) {
					if (map_get(&st->symbol_set, p->id)) {
						mark_type(st, s->type);
						break;
					}
				}
			}
		}
		break;
	}
	case STMT_TYPEDEF: {
		StmtTYPEDEF *s = (StmtTYPEDEF *) h;
		if (s->name) {
			if (map_get(&st->symbol_set, s->name)) {
				mark_type(st, s->type);
			}
		}
		if (s->type->type == TYPE_STRUCT) {
			TypeSTRUCT *t = (TypeSTRUCT *) s->type;
			if (t->tag) {
				if (map_get(&st->symbol_set, t->tag)) {
					mark_type(st, s->type);
				}
			}
		} else if (s->type->type == TYPE_ENUM) {
			TypeENUM *t = (TypeENUM *) s->type;
			if (t->tag && map_get(&st->symbol_set, t->tag)) {
				mark_type(st, s->type);
			}
		}
		break;
	}
	case STMT_DECLS: {
		StmtDECLS *s = (StmtDECLS *) h;
		Stmt *p;
		int i;
		vec_foreach(&s->items, p, i) {
			mark_topstmt2(st, p);
		}
		break;
	}
	}
}

static void sweep_topstmt(State *st, Stmt *h, StmtBLOCK *n)
{
	switch (h->type) {
	case STMT_FUNDECL: {
		StmtFUNDECL *s = (StmtFUNDECL *) h;
		if (s->name && map_get(&st->symbol_set, s->name)) {
			stmtBLOCK_append(n, h);
		} else {
			// the return type may implicit declare a struct/enum...
			Type *t = has_structenum(s->type->rt);
			if (t) {
				Type *nt = NULL;
				if (t->type == TYPE_STRUCT) {
					TypeSTRUCT *ts = (TypeSTRUCT *) t;
					nt = typeSTRUCT(ts->is_union, ts->tag,
							ts->decls, 0, ts->attrs);
				} else {
					TypeENUM *te = (TypeENUM *) t;
					if (te->list)
						nt = typeENUM(te->tag, te->list, 0,
							      te->attrs);
				}
				if (nt)
					stmtBLOCK_append(
						n,
						stmtVARDECL(0, NULL, nt, NULL, NULL,
							    (Extension) {}));
			}
		}
		break;
	}
	case STMT_VARDECL: {
		StmtVARDECL *s = (StmtVARDECL *) h;
		if (s->name && map_get(&st->symbol_set, s->name)) {
			stmtBLOCK_append(n, h);
		} else if (s->type->type == TYPE_STRUCT) {
			// struct declaration
			TypeSTRUCT *t = (TypeSTRUCT *) s->type;
			if (t->tag) {
				if (map_get(&st->symbol_set, t->tag)) {
					stmtBLOCK_append(n, h);
				}
			} else {
				if (!s->name) {
					// maybe an anonymous struct
					stmtBLOCK_append(n, h);
				}
			}
		} else if (s->type->type == TYPE_ENUM) {
			// enum declaration
			TypeENUM *t = (TypeENUM *) s->type;
			if (t->tag && map_get(&st->symbol_set, t->tag)) {
				stmtBLOCK_append(n, h);
			} else if (t->list) {
				struct EnumPair_ *p;
				int i;
				avec_foreach_ptr(&(t->list->items), p, i) {
					if (map_get(&st->symbol_set, p->id)) {
						stmtBLOCK_append(n, h);
						break;
					}
				}
			}
		}
		break;
	}
	case STMT_TYPEDEF: {
		StmtTYPEDEF *s = (StmtTYPEDEF *) h;
		if (s->name && map_get(&st->symbol_set, s->name)) {
			stmtBLOCK_append(n, h);
		} else if (s->type->type == TYPE_STRUCT) {
			TypeSTRUCT *t = (TypeSTRUCT *) s->type;
			if (t->tag) {
				if (map_get(&st->symbol_set, t->tag)) {
					stmtBLOCK_append(n, h);
				}
			}
		} else if (s->type->type == TYPE_ENUM) {
			TypeENUM *t = (TypeENUM *) s->type;
			if (t->tag && map_get(&st->symbol_set, t->tag)) {
				stmtBLOCK_append(n, h);
			} else if (t->list) {
				struct EnumPair_ *p;
				int i;
				avec_foreach_ptr(&(t->list->items), p, i) {
					if (map_get(&st->symbol_set, p->id)) {
						stmtBLOCK_append(n, h);
						break;
					}
				}
			}
		}
		break;
	}
	case STMT_DECLS: {
		StmtDECLS *s = (StmtDECLS *) h;
		Stmt *p;
		int i;
		vec_foreach(&s->items, p, i) {
			sweep_topstmt(st, p, n);
		}
		break;
	}
	default:
		stmtBLOCK_append(n, h);
		break;
	}
}

StmtBLOCK *elim_unused(StmtBLOCK *tu)
{
	State *st = __new(State);
	st->progress = false;
	map_init(&st->symbol_set);

	StmtBLOCK *res = stmtBLOCK();
	Stmt *p;
	int i;

	vec_foreach(&tu->items, p, i) {
		mark_topstmt(st, p);
	}

	do {
		st->progress = false;
		vec_foreach(&tu->items, p, i) {
			mark_topstmt2(st, p);
		}
	} while (st->progress);

	vec_foreach(&tu->items, p, i) {
		sweep_topstmt(st, p, res);
	}

	map_deinit(&st->symbol_set);
	return res;
}

END_MANAGED
