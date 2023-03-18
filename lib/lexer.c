#include "lexer.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include "vec.h"
#include "map.h"

typedef struct {
	char (*peek)(TextStream *);
	void (*next)(TextStream *);
	void (*prev)(TextStream *);
	void (*del)(TextStream *);
} TextStreamOps;

struct TextStream_ {
	const TextStreamOps *ops;
};

void text_stream_delete(TextStream *ts)
{
	ts->ops->del(ts);
}

char text_stream_peek(TextStream *ts)
{
	return ts->ops->peek(ts);
}

void text_stream_next(TextStream *ts)
{
	ts->ops->next(ts);
}

void text_stream_prev(TextStream *ts)
{
	ts->ops->prev(ts);
}

typedef struct {
	TextStream h;
	FILE *fp;
	int c0;
	int c;
} FileTextStream;

static void file_text_stream_delete(TextStream *ts)
{
	FileTextStream *ios = (FileTextStream *) ts;
	fclose(ios->fp);
	free(ts);
}

static char file_text_stream_peek(TextStream *ts)
{
	FileTextStream *ios = (FileTextStream *) ts;
	if (ios->c == EOF)
		return 0;
	return ios->c;
}

static void file_text_stream_next(TextStream *ts)
{
	FileTextStream *ios = (FileTextStream *) ts;
	ios->c0 = ios->c;
	ios->c = fgetc(ios->fp);
}

static void file_text_stream_prev(TextStream *ts)
{
	FileTextStream *ios = (FileTextStream *) ts;
	ungetc(ios->c, ios->fp);
	ios->c = ios->c0;
}

static const TextStreamOps file_text_stream_ops = {
	file_text_stream_peek, file_text_stream_next,
	file_text_stream_prev, file_text_stream_delete,
};

typedef struct {
	TextStream h;
	char *buf;
	long len;
	long i;
} MemTextStream;

static void mem_text_stream_delete(TextStream *ts)
{
	MemTextStream *ms = (MemTextStream *) ts;
	free(ms->buf);
	free(ts);
}

static char mem_text_stream_peek(TextStream *ts)
{
	MemTextStream *ms = (MemTextStream *) ts;
	if (ms->i < ms->len) {
		return ms->buf[ms->i];
	} else {
		return 0;
	}
}

static void mem_text_stream_next(TextStream *ts)
{
	MemTextStream *ms = (MemTextStream *) ts;
	ms->i++;
}

static void mem_text_stream_prev(TextStream *ts)
{
	MemTextStream *ms = (MemTextStream *) ts;
	ms->i--;
}

static const TextStreamOps mem_text_stream_ops = {
	mem_text_stream_peek, mem_text_stream_next,
	mem_text_stream_prev, mem_text_stream_delete,
};

TextStream *text_stream_new(const char *file)
{
	FileTextStream *ios = malloc(sizeof(FileTextStream));
	if (strcmp(file, "-") == 0)
		ios->fp = stdin;
	else
		ios->fp = fopen(file, "r");
	if (ios->fp)
		ios->c = fgetc(ios->fp);
	else
		ios->c = EOF;
	ios->h.ops = &file_text_stream_ops;
	return &ios->h;
}

TextStream *text_stream_from_string(const char *string)
{
	MemTextStream *ms = malloc(sizeof(MemTextStream));
	ms->buf = strdup(string);
	ms->len = strlen(string);
	ms->i = 0;
	ms->h.ops = &mem_text_stream_ops;
	return &ms->h;
}

struct Lexer_ {
	TextStream *ts;
	bool hol;
	map_int_t kws;

	int tok_type;
	vec_char_t tok;
	union {
		long long int_cst;
		unsigned long long uint_cst;
		char char_cst;
		double float_cst;
	} u;
};

#define P text_stream_peek(l->ts)
#define N text_stream_next(l->ts)
#define U text_stream_prev(l->ts)
static int lex_punct(Lexer *l)
{
	char c = P;
	char d, e;
	switch (c) {
	case '&':
		// & && &=
		N; d = P;
		if (d == '&') {
			N; return TOK_AND;
		} else if (d == '=') {
			N; return TOK_ASSIGNBAND;
		}
		return c;
	case '|':
		// | || |=
		N; d = P;
		if (d == '|') {
			N; return TOK_OR;
		} else if (d == '=') {
			N; return TOK_ASSIGNBOR;
		}
		return c;
	case '!':
		// ! !=
		N; d = P;
		if (d == '=') {
			N; return TOK_NEQ;
		}
		return c;
	case '^':
		// ^ ^=
		N; d = P;
		if (d == '=') {
			N; return TOK_ASSIGNBXOR;
		}
	case '<':
		// < <= << <<=
		N; d = P;
		if (d == '=') {
			N; return TOK_LE;
		} else if (d == '<') {
			N; e = P;
			if (e == '=') {
				N; return TOK_ASSIGNBSHL;
			}
			return TOK_BSHL;
		}
		return c;
	case '>':
		// > >= >> >>=
		N; d = P;
		if (d == '=') {
			N; return TOK_GE;
		} else if (d == '>') {
			N; e = P;
			if (e == '=') {
				N; return TOK_ASSIGNBSHR;
			}
			return TOK_BSHR;
		}
		return c;
	case '-':
		// - -- -= ->
		N; d = P;
		if (d == '-') {
			N; return TOK_DEC;
		} else if (d == '=') {
			N; return TOK_ASSIGNSUB;
		} else if (d == '>') {
			N; return TOK_PMEM;
		}
		return c;
	case '+':
		// + ++ +=
		N; d = P;
		if (d == '+') {
			N; return TOK_INC;
		} else if (d == '=') {
			N; return TOK_ASSIGNADD;
		}
		return c;
	case '*':
		// * *=
		N; d = P;
		if (d == '=') {
			N; return TOK_ASSIGNMUL;
		}
		return c;
	case '/':
		// / /=
		N; d = P;
		if (d == '=') {
			N; return TOK_ASSIGNDIV;
		}
		return c;
	case '%':
		N; d = P;
		if (d == '=') {
			N; return TOK_ASSIGNMOD;
		}
		return c;
	case '=':
		// = ==
		N; d = P;
		if (d == '=') {
			N; return TOK_EQ;
		}
		return c;
	case '.':
		// . ...
		N; d = P;
		if (d == '.') {
			N; d = P;
			if (d == '.') {
				N; return TOK_DOT3;
			} else {
				U; return '.';
			}
		} else {
			return '.';
		}
	case '~':
	case '(': case ')':
	case '[': case ']':
	case '{': case '}':
	case ',':
	case ';':
	case ':':
	case '#':
	case '?':
		N; return c;
	default:
		return 0;
	}
}

static unsigned int lex_escape(Lexer *l)
{
	unsigned int c, d;
	if (P == '\\') {
		N; c = P;
		if (!c)
			return 256;
		switch (c) {
		case '\'': case '"': case '?': case '\\': case '/':
			N; return c;
		case 'a': N; return '\a';
		case 'b': N; return '\b';
		case 'f': N; return '\f';
		case 'n': N; return '\n';
		case 'r': N; return '\r';
		case 't': N; return '\t';
		case 'v': N; return '\v';
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			d = 0;
			for (int i = 0; i < 3 && c >= '0' && c <= '7'; i++) {
				d = d * 8 + (c - '0');
				N; c = P;
			}
			return d % 256;
		case 'x':
			N; c = P; d = 0;
			while (c >= '0' && c <= '9' ||
			       c >= 'a' && c <= 'f' ||
			       c >= 'A' && c <= 'F') {
				if (c >= '0' && c <= '9')
					d = d * 16 + (c - '0');
				else if (c >= 'a' && c <= 'f')
					d = d * 16 + (c - 'a' + 10);
				else if (c >= 'A' && c <= 'F')
					d = d * 16 + (c - 'A' + 10);
				N; c = P;
			}
			return d % 256;
		default: return 256;
		}
	}
	return 256;
}

static bool lex_stringbody(Lexer *l)
{
	char c;
	unsigned int d;
	while ((c = P)) {
		switch (c) {
		case '"': return true;
		case '\\':
			d = lex_escape(l);
			if (d != 256)
				vec_push(&l->tok, d);
			else
				return false;
			break;
		default: N; vec_push(&l->tok, c);; break;
		}
	}
	return false;
}

static unsigned int lex_char(Lexer *l)
{
	char c = P;
	if (c == '\'') {
		N; char d = P;
		if (d == '\'') {
			return 256;
		} else if (d == '\\') {
			unsigned int e = lex_escape(l);
			if (e != 256 && P == '\'') {
				N; return e;
			}
		} else {
			N;
			if (P == '\'') {
				N; return d;
			}
		}
	}
	return 256;
}
#undef N
#undef P

#define LEX(name, cond) \
static char lex_##name(Lexer *l) \
{ \
	char c = text_stream_peek(l->ts); \
	if (cond) { \
		text_stream_next(l->ts); \
		return c; \
	} else { \
		return 0; \
	} \
}

LEX(alpha, (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
LEX(alpha_, (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
LEX(alphadigit_, \
    (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || \
    (c >= '0' && c <= '9') || c == '_')
LEX(digit, (c >= '0' && c <= '9'))

LEX(space, c == ' ' || c == '\t' || \
    c == '\r' || c == '\v' || c == '\f')
LEX(sharp, c == '#')
LEX(newline, c == '\n')
LEX(not_newline, c != '\n')

LEX(strchar, c != '"' && c != '\\')
LEX(quote, c == '"')
LEX(uU, c == 'u' || c == 'U')
LEX(l, c == 'l')
LEX(L, c == 'L')
LEX(0, c == '0')
LEX(xX, c == 'x' || c == 'X')
LEX(hex, (c >= '0' && c <= '9') || \
    (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
LEX(dot, c == '.')
LEX(eE, c == 'e' || c == 'E')
LEX(sign, c == '+' || c == '-')
LEX(fF, c == 'f' || c == 'F')
LEX(lL, c == 'l' || c == 'L')

typedef char (*lex_func_t)(Lexer *);

static bool lex_one(Lexer *l, lex_func_t lex)
{
	char c = lex(l);

	if (c) {
		vec_push(&l->tok, c);
		return true;
	} else {
		return false;
	}
}

static bool lex_one_ignore(Lexer *l, lex_func_t lex)
{
	char c = lex(l);
	if (c) {
		return true;
	} else {
		return false;
	}
}

static bool lex_many(Lexer *l, lex_func_t lex)
{
	bool res = lex_one(l, lex);
	if (res) {
		while (lex_one(l, lex)) {}
		return true;
	} else {
		return false;
	}
}

static bool lex_many_ignore(Lexer *l, lex_func_t lex)
{
	bool res = lex_one_ignore(l, lex);
	if (res) {
		while (lex_one_ignore(l, lex)) {}
		return true;
	} else {
		return false;
	}
}

static void skip_spaces(Lexer *l)
{
	while (true) {
		lex_many_ignore(l, lex_space);
		if (l->hol) {
			if (lex_one_ignore(l, lex_sharp)) {
				lex_many_ignore(l, lex_not_newline);
			}
			l->hol = false;
		}
		if (lex_many_ignore(l, lex_newline)) {
			l->hol = true;
		} else {
			break;
		}
	}
}

static void handle_int_cst(Lexer *l, int base)
{
	char *endp;
	if (lex_one_ignore(l, lex_uU)) {
		l->u.uint_cst = strtoull(l->tok.data, &endp, base);
		if (*endp != '\0') {
			l->tok_type = TOK_ERROR;
		} else {
			if (lex_one_ignore(l, lex_l)) {
				if (lex_one_ignore(l, lex_l)) {
					l->tok_type = TOK_ULLONG_CST;
				} else {
					l->tok_type = TOK_ULONG_CST;
				}
			} else {
				l->tok_type = TOK_UINT_CST;
			}
		}
	} else {
		l->u.int_cst = strtoll(l->tok.data, &endp, base);
		if (*endp != '\0') {
			l->tok_type = TOK_ERROR;
		} else {
			if (lex_one_ignore(l, lex_l)) {
				if (lex_one_ignore(l, lex_l)) {
					l->tok_type = TOK_LLONG_CST;
				} else {
					l->tok_type = TOK_LONG_CST;
				}
			} else {
				l->tok_type = TOK_INT_CST;
			}
		}
	}
}

static bool lex_float(Lexer *l)
{
	if (lex_one(l, lex_dot)) {
		if (lex_many(l, lex_digit)) {
			if (lex_one(l, lex_eE)) {
				lex_one(l, lex_sign);
				if (lex_many(l, lex_digit)) {
					vec_push(&l->tok, 0);
				} else {
					l->tok_type = TOK_ERROR;
					return false;
				}
			} else {
				vec_push(&l->tok, 0);
			}
		} else {
			if (l->tok.length == 1) {
				vec_pop(&l->tok);
				U;
				return false;
			} else {
				if (lex_one(l, lex_eE)) {
					lex_one(l, lex_sign);
					if (lex_many(l, lex_digit)) {
						vec_push(&l->tok, 0);
					} else {
						l->tok_type = TOK_ERROR;
						return false;
					}
				} else {
					vec_push(&l->tok, 0);
				}
			}
		}
		fprintf(stderr, "try: %s\n", l->tok.data);
		char *endp;
		if (lex_one_ignore(l, lex_fF)) {
			l->u.float_cst = strtof(l->tok.data, &endp);
			if (*endp != '\0') {
				l->tok_type = TOK_ERROR;
			} else {
				l->tok_type = TOK_FLOAT_CST;
			}
		} else {
			lex_one_ignore(l, lex_lL);
			l->u.float_cst = strtod(l->tok.data, &endp);
			if (*endp != '\0') {
				l->tok_type = TOK_ERROR;
			} else {
				l->tok_type = TOK_DOUBLE_CST;
			}
		}
	} else {
		return false;
	}
}

void lexer_next(Lexer *l)
{
	vec_clear(&l->tok);
	skip_spaces(l);
	if (lex_one(l, lex_alpha_)) {
		lex_many(l, lex_alphadigit_);
		vec_push(&l->tok, 0);

		int *type = map_get(&l->kws, l->tok.data);
		if (type) {
			l->tok_type = *type;
		} else {
			l->tok_type = TOK_IDENT;
		}
	} else if (lex_one(l, lex_0)) {
		if (lex_one(l, lex_xX)) {
			if (lex_many(l, lex_hex)) {
				vec_push(&l->tok, 0);
				handle_int_cst(l, 16);
			} else {
				l->tok_type = TOK_ERROR;
			}
		} else {
			if (lex_many(l, lex_digit)) {
				if (!lex_float(l)) {
					vec_push(&l->tok, 0);
					if (l->tok.data[0] == '0') {
						handle_int_cst(l, 8);
					} else {
						handle_int_cst(l, 10);
					}
				}
			} else {
				if (!lex_float(l)) {
					vec_push(&l->tok, 0);
					handle_int_cst(l, 10); // zero
				}
			}
		}
	} else if (lex_many(l, lex_digit)) {
		if (!lex_float(l)) {
			vec_push(&l->tok, 0);
			handle_int_cst(l, 10);
		}
	} else if (text_stream_peek(l->ts) == 0) {
		l->tok_type = TOK_END;
	} else {
		if (lex_float(l)) {
			return;
		}
		int type = lex_punct(l);
		if (type) {
			l->tok_type = type;
			return;
		}
		if (lex_one_ignore(l, lex_quote)) {
			vec_clear(&l->tok);
			do {
				if (!lex_stringbody(l) ||
				    !lex_one_ignore(l, lex_quote)) {
					l->tok_type = TOK_ERROR;
					return;
				}
				skip_spaces(l);
			} while (lex_one_ignore(l, lex_quote));
			vec_push(&l->tok, 0);
			l->tok_type = TOK_STRING_CST;
			return;
		}
		int res = lex_char(l);
		if (res != 256) {
			l->tok_type = TOK_CHAR_CST;
			l->u.char_cst = res;
			return;
		}
		l->tok_type = TOK_ERROR;
	}
}

int lexer_peek(Lexer *l)
{
	return l->tok_type;
}

const char *lexer_peek_string(Lexer *l)
{
	return l->tok.data;
}

int lexer_peek_string_len(Lexer *l)
{
	return l->tok.length;
}

long long lexer_peek_int(Lexer *l)
{
	return l->u.int_cst;
}

unsigned long long lexer_peek_uint(Lexer *l)
{
	return l->u.uint_cst;
}

char lexer_peek_char(Lexer *l)
{
	return l->u.char_cst;
}

double lexer_peek_float(Lexer *l)
{
	return l->u.float_cst;
}

static void lexer_init(Lexer *l, TextStream *ts)
{
	l->ts = ts;
	l->hol = true;
	vec_init(&l->tok);

	map_init(&l->kws);
#define KWS(str, type) map_set(&l->kws, str, type)
#include "keywords.def"
#undef KWS

	lexer_next(l);
}

static void lexer_free(Lexer *l)
{
	vec_deinit(&l->tok);
	map_deinit(&l->kws);
}

Lexer *lexer_new(TextStream *ts)
{
	Lexer *self = malloc(sizeof(Lexer));
	lexer_init(self, ts);
	return self;
}

void lexer_delete(Lexer *l)
{
	lexer_free(l);
	free(l);
}

#if 0
static int token_print(Lexer *l)
{
	switch (lexer_peek(l)) {
	case TOK_IDENT:
		printf("ident %s\n", lexer_peek_string(l));
		break;
#define KWS(str, type) case type: printf("keyword %s\n", str); break
#include "keywords.def"
#undef KWS
	case TOK_INT_CST:
		printf("int %d\n", lexer_peek_int(l));
		break;
	case TOK_CHAR_CST:
		printf("char %c\n", lexer_peek_char(l));
		break;
	case TOK_STRING_CST:
		printf("string %s\n", l->tok.data); // XXX
		break;
	case TOK_ERROR:
		printf("error\n");
	case TOK_END:
		return 0;
	default:
		if (l->tok_type < 256) {
			if (l->tok_type < 128) {
				printf("%c\n", l->tok_type);
			} else {
				printf("type %d\n", l->tok_type);
			}
			break;
		}
		abort();
	}
	return 1;
}
#endif
