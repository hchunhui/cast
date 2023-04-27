static int parse_oldfunargs1(Parser *p, Declarator *d, TypeFUN *n)
{
	if (P == TOK_IDENT && symlookup(p, PS) != SYM_TYPE) {
		enter_scope(p);
		StmtBLOCK *funargs = d->funargs ? NULL : stmtBLOCK();
		Type *t = typeINT(0);
		if (funargs) {
			const char *id = get_and_next(p);
			stmtBLOCK_append(funargs, stmtVARDECL(0, id, t, NULL, NULL,
							      (Extension) {}));
		}
		typeFUN_append(n, t);
		while(match(p, ',')) {
			F(P == TOK_IDENT && symlookup(p, PS) != SYM_TYPE, leave_scope(p));
			if (funargs) {
				const char *id = get_and_next(p);
				stmtBLOCK_append(funargs, stmtVARDECL(0, id, t, NULL, NULL,
								      (Extension) {}));
			}
			typeFUN_append(n, t);
		}
		F(match(p, ')'), leave_scope(p));
		d->type = &n->h;
		if (funargs) {
			d->funargs = funargs;
			assert (d->funscope == NULL);
			d->funscope = get_scope(p);
			d->old_fundecl = true;
		} else {
			leave_scope(p);
		}
		return 1;
	}
	return -1;
}

static bool replace_funarg(Stmt *decl, StmtBLOCK *funargs, TypeFUN *nft)
{
	switch(decl->type) {
	case STMT_VARDECL: {
		StmtVARDECL *vd = (StmtVARDECL *) decl;
		if (vd->init)
			return false;
		if (vd->flags & (DFLAG_EXTERN | DFLAG_STATIC))
			return false;
		if (vd->name) {
			if (!funargs)
				return false;
			int i;
			Stmt **s;
			bool found = false;
			avec_foreach_ptr(&funargs->items, s, i) {
				assert((*s)->type == STMT_VARDECL);
				StmtVARDECL *vd1 = (StmtVARDECL *) *s;
				if (strcmp(vd1->name, vd->name) == 0) {
					*s = (Stmt *) vd;
					nft->at.data[i] = vd->type;
					found = true;
					break;
				}
			}
			if (!found)
				return false;
		}
		return true;
	}
	case STMT_DECLS: {
		StmtDECLS *s = (StmtDECLS *) decl;
		int i;
		Stmt *s1;
		avec_foreach(&s->items, s1, i) {
			if (!replace_funarg(s1, funargs, nft))
				return false;
		}
		return true;
	}
	default:
		return false;
	}
}

static int parse_oldfunargs2(Parser *p, Declarator *d) {
	Stmt *decl;
	if (P == ';')
	   return -1;
	if (parse_decl(p, &decl)) {
		StmtBLOCK *funargs = d->funargs;
		TypeFUN *oft = (TypeFUN *) d->type;
		TypeFUN *nft = typeFUN(oft->rt);
		int i;
		Type *ti;
		avec_foreach(&oft->at, ti, i) {
			typeFUN_append(nft, ti);
		}
		nft->va_arg = oft->va_arg;
		do {
			if (!replace_funarg(decl, funargs, nft))
				return 0;
		} while (parse_decl(p, &decl));
		d->type = (Type *) nft;
		F(P == '{');
		return 1;
	}
	return -1;
}
