static StmtBLOCK *parse_block_stmt(Parser *p);
static Expr *parse_parentheses_gnu_post(Parser *p)
{
	F(P == '{');
	StmtBLOCK *block;
	F(block = parse_block_stmt(p));
	F(match(p, ')'));
	return exprSTMT(block);
}

static Stmt *parse_stmt_gnu_goto(Parser *p)
{
	F(P == '*');
	Expr *e;
	F(e = parse_expr(p));
	F(match(p, ';'));
	return stmtGOTOADDR(e);
}

static const char *const gcc_builtin_types[] = {
	"__builtin_va_list",
	"_Float16", "_Float16x",
	"_Float32", "_Float32x",
	"_Float64", "_Float64x",
	"_Float128", "_Float128x",
	"__int128_t", "__uint128_t",
	NULL
};

static Expr *parse_builtin_gnu(Parser *p, const char *id)
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
	return NULL;
}

static bool parse_gnu_asm_name(Parser *p, const char **name)
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

static Stmt *parse_stmt_gnu_label(Parser *p)
{
	F(P == TOK_IDENT);
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

static Stmt *parse_stmt_gnu_asm(Parser *p)
{
	unsigned int flags = 0;
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

static bool parse_gnu_attribute1(Parser *p, Attribute *a)
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

static bool parse_gnu_attribute(Parser *p, Attribute **pa)
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
				F_(parse_gnu_attribute1(p, a), false);
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
