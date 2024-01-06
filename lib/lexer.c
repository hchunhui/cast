#include "lexer.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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
		unsigned long long uint_cst;
		char char_cst;
		double float_cst;
	} u;

	int line;
	char path[256];
	vec_char_t tok_temp;

	char file[256];
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
		return c;
	case '<':
		// < <= << <<= <% <:
		N; d = P;
		if (d == '=') {
			N; return TOK_LE;
		} else if (d == '<') {
			N; e = P;
			if (e == '=') {
				N; return TOK_ASSIGNBSHL;
			}
			return TOK_BSHL;
		} else if (d == '%') {
			N; return '{';
		} else if (d == ':') {
			N; return '[';
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
		// % %= %>
		N; d = P;
		if (d == '=') {
			N; return TOK_ASSIGNMOD;
		} else if (d == '>') {
			N; return '}';
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
	case ':':
		// : :> ::
		N; d = P;
		if (d == '>') {
			N; return ']';
		} else if (d == ':') {
			N; return TOK_COL2;
		}
		return c;
	case '~':
	case '(': case ')':
	case '[': case ']':
	case '{': case '}':
	case ',':
	case ';':
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
		case 'e': N; return '\033'; // gcc extension
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
		case '(': case '{': case '[': case '%':
		/* gcc extension
		   comment from gcc/libcpp/charset.c:
		   '\(', etc, can be used at the beginning of a line in a long
		   string split onto multiple lines with \-newline, to prevent
		   Emacs or other text editors from getting confused.  '\%' can
		   be used to prevent SCCS from mangling printf format strings.
		*/
			N; return c;
		default: return 256;
		}
	}
	return 256;
}

static bool lex_stringbody(Lexer *l, vec_char_t *dst)
{
	char c;
	unsigned int d;
	while ((c = P)) {
		switch (c) {
		case '"': return true;
		case '\\':
			d = lex_escape(l);
			if (d != 256)
				vec_push(dst, d);
			else
				return false;
			break;
		default: N; vec_push(dst, c);; break;
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

static bool skip_comment(Lexer *l)
{
	if (P == '/') {
		N; char c = P;
		if (c == '/') {
			N; while (P != '\n') {
				N;
			}
			N;
			l->line++;
			l->hol = true;
			return true;
		} else if (c == '*') {
			N; do {
				while ((c = P) != '*') {
					if (c == '\n') {
						l->line++;
						l->hol = true;
					}
					N;
				}
				N;
			} while (P != '/');
			N;
			return true;
		} else {
			U; return false;
		}
	}
	return false;
}

static bool lex_raw_stringbody(Lexer *l, vec_char_t *dst)
{
	char c = P;
	if (c == '"') {
		N;
		char delim[32];
		delim[0] = ')';
		int n = 1;
		bool found = false;
		while (true) {
			c = P;
			if (c == 0) {
				return false;
			}
			N;
			if (c == '(') {
				found = true;
				break;
			}
			if (c != ')' && c != '"' && n < 32) {
				delim[n++] = c;
			} else {
				return false;
			}
		}
		if (found) {
			int m = 0;
			while ((c = P)) {
				N;
				if (c == delim[m]) {
					m++;
				} else {
					for (int i = 0; i < m; i++)
						vec_push(dst, delim[i]);
					if (c == delim[0]) {
						m = 1;
					} else {
						vec_push(dst, c);
						m = 0;
					}
				}
				if (m == n) {
					c = P;
					if (c == '"') {
						N; vec_push(dst, 0);
						return true;
					}
				}
			}
		}
	}
	return false;
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

#define ALPHA ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
#define DIGIT ((c >= '0' && c <= '9'))
#define HEX   (DIGIT || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
LEX(alpha, ALPHA)
LEX(alpha_, ALPHA || c == '_')
LEX(alphadigit_, ALPHA || DIGIT || c == '_')
LEX(digit, DIGIT)
LEX(dsep, c == '\'')

LEX(space, c == ' ' || c == '\t' || \
    c == '\r' || c == '\v' || c == '\f')
LEX(sharp, c == '#')
LEX(newline, c == '\n')
LEX(not_newline, c != '\n')

LEX(quote, c == '"')
LEX(uU, c == 'u' || c == 'U')
LEX(l, c == 'l')
LEX(L, c == 'L')
LEX(0, c == '0')
LEX(xX, c == 'x' || c == 'X')
LEX(hex, HEX)
LEX(dot, c == '.')
LEX(eE, c == 'e' || c == 'E')
LEX(pP, c == 'p' || c == 'P')
LEX(sign, c == '+' || c == '-')
LEX(fF, c == 'f' || c == 'F')
LEX(lL, c == 'l' || c == 'L')

LEX(bB, c == 'b' || c == 'B')
LEX(bin, c == '0' || c == '1')

typedef char (*lex_func_t)(Lexer *);

#define LEX1(name, action) \
static bool lex_##name(Lexer *l, lex_func_t lex) \
{ \
	char c = lex(l); \
	if (c) { \
		action; \
		return true; \
	} else { \
		return false; \
	} \
}
LEX1(one, vec_push(&l->tok, c))
LEX1(one_temp, vec_push(&l->tok_temp, c))
LEX1(one_ignore,)

#define LEXM(name, one) \
static bool lex_##name(Lexer *l, lex_func_t lex) \
{ \
	bool res = lex_##one(l, lex); \
	if (res) { \
		while (lex_##one(l, lex)) {} \
		return true; \
	} else { \
		return false; \
	} \
}
LEXM(many, one)
LEXM(many_temp, one_temp)
LEXM(many_ignore, one_ignore)

static bool lex_many_dsep(Lexer *l, lex_func_t lex)
{
	while (lex_one_ignore(l, lex_dsep)) {
		if (!lex_many(l, lex)) {
			l->tok_type = TOK_ERROR;
			return false;
		}
	}
	return true;
}

static int skip_spaces(Lexer *l)
{
	while (true) {
		lex_many_ignore(l, lex_space);
		if (skip_comment(l))
			continue;
		if (l->hol) {
			if (lex_one_ignore(l, lex_sharp)) {
				lex_many_ignore(l, lex_space);
				vec_clear(&l->tok_temp);
				if (lex_many_temp(l, lex_digit)) {
					vec_push(&l->tok_temp, 0);
					l->line = atoi(l->tok_temp.data) - 1;
					lex_many_ignore(l, lex_space);
					if (lex_one_ignore(l, lex_quote)) {
						vec_clear(&(l->tok_temp));
						if (lex_stringbody(l, &l->tok_temp)) {
							vec_push(&l->tok_temp, 0);
							strncpy(l->path, l->tok_temp.data, 255);
							if (l->file[0] == 0)
								strcpy(l->file, l->path);
						}
					}
				} else if (lex_many_temp(l, lex_alpha)) {
					vec_push(&l->tok_temp, 0);
					if (strcmp(l->tok_temp.data, "pragma") == 0) {
						lex_many_ignore(l, lex_space);
						vec_clear(&l->tok_temp);
						lex_many_temp(l, lex_not_newline);
						vec_push(&l->tok_temp, 0);
						return TOK_PP_PRAGMA_LINE;
					}
				}
				lex_many_ignore(l, lex_not_newline);
			} else {
				l->line++;
			}
			l->hol = false;
		}
		if (lex_one_ignore(l, lex_newline)) {
			while(lex_one_ignore(l, lex_newline))
				l->line++;
			l->hol = true;
		} else {
			break;
		}
	}
	return 0;
}

#define CTREE_INT_MAX              2147483647ull
#define CTREE_UINT_MAX             4294967295ull
#define CTREE_LONG_MAX    9223372036854775807ull
#define CTREE_ULONG_MAX  18446744073709551615ull
#define CTREE_LLONG_MAX   9223372036854775807ull
#define CTREE_ULLONG_MAX 18446744073709551615ull
static void handle_int_cst(Lexer *l, int off, int base)
{
	unsigned long long v = strtoull(l->tok.data + off, NULL, base);
	bool is_unsigned = false;
	bool is_long = false;
	bool is_longlong = false;

	if (lex_one_ignore(l, lex_uU)) {
		is_unsigned = true;
	}
	if (lex_one_ignore(l, lex_l)) {
		if (lex_one_ignore(l, lex_l)) {
			is_longlong = true;
		} else {
			is_long = true;
		}
	} else if (lex_one_ignore(l, lex_L)) {
		if (lex_one_ignore(l, lex_L)) {
			is_longlong = true;
		} else {
			is_long = true;
		}
	}
	if (!is_unsigned) {
		if (lex_one_ignore(l, lex_uU)) {
			is_unsigned = true;
		}
	}
	if (base == 10 || is_unsigned) {
		if (is_unsigned) {
			if (!is_long && !is_longlong && v <= CTREE_UINT_MAX)
				l->tok_type = TOK_UINT_CST;
			else if (is_long && v <= CTREE_ULONG_MAX)
				l->tok_type = TOK_ULONG_CST;
			else
				l->tok_type = TOK_ULLONG_CST;
		} else {
			if (!is_long && !is_longlong && v <= CTREE_INT_MAX)
				l->tok_type = TOK_INT_CST;
			else if (is_long && v <= CTREE_LONG_MAX)
				l->tok_type = TOK_LONG_CST;
			else
				l->tok_type = TOK_LLONG_CST;
		}
	} else {
		if (!is_long && !is_longlong && v <= CTREE_INT_MAX)
			l->tok_type = TOK_INT_CST;
		else if (!is_long && !is_longlong && v <= CTREE_UINT_MAX)
			l->tok_type = TOK_UINT_CST;
		else if (is_long && v <= CTREE_LONG_MAX)
			l->tok_type = TOK_LONG_CST;
		else if (is_long && v <= CTREE_ULONG_MAX)
			l->tok_type = TOK_ULONG_CST;
		else if (v <= CTREE_LLONG_MAX)
			l->tok_type = TOK_LLONG_CST;
		else
			l->tok_type = TOK_ULLONG_CST;
	}
	l->u.uint_cst = v;
}

static void handle_float_cst(Lexer *l)
{
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
}

static bool lex_float(Lexer *l)
{
	if (lex_one(l, lex_dot)) {
		if (lex_many(l, lex_digit)) {
			if (!lex_many_dsep(l, lex_digit))
				return true;
			if (lex_one(l, lex_eE)) {
				lex_one(l, lex_sign);
				if (lex_many(l, lex_digit)) {
					if (!lex_many_dsep(l, lex_digit))
						return true;
					vec_push(&l->tok, 0);
				} else {
					l->tok_type = TOK_ERROR;
					return true;
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
						if (!lex_many_dsep(l, lex_digit))
							return true;
						vec_push(&l->tok, 0);
					} else {
						l->tok_type = TOK_ERROR;
						return true;
					}
				} else {
					vec_push(&l->tok, 0);
				}
			}
		}
		handle_float_cst(l);
		return true;
	} else {
		if (l->tok.length != 0) {
			if (lex_one(l, lex_eE)) {
				lex_one(l, lex_sign);
				if (lex_many(l, lex_digit)) {
					if (!lex_many_dsep(l, lex_digit))
						return true;
					vec_push(&l->tok, 0);
				} else {
					l->tok_type = TOK_ERROR;
					return true;
				}
			} else {
				return false;
			}
			handle_float_cst(l);
			return true;
		}
		return false;
	}
}

static bool lex_0xfloat(Lexer *l)
{
	if (lex_one(l, lex_dot)) {
		lex_many(l, lex_hex);
		if (!lex_many_dsep(l, lex_hex))
			return true;
		if (lex_one(l, lex_pP)) {
			lex_one(l, lex_sign);
			if (lex_many(l, lex_digit)) {
				if (!lex_many_dsep(l, lex_digit))
					return true;
				vec_push(&l->tok, 0);
				handle_float_cst(l);
				return true;
			}
		}
		l->tok_type = TOK_ERROR;
		return true;
	} else if (lex_one(l, lex_pP)) {
		lex_one(l, lex_sign);
		if (lex_many(l, lex_digit)) {
			if (!lex_many_dsep(l, lex_digit))
				return true;
			vec_push(&l->tok, 0);
			handle_float_cst(l);
			return true;
		}
		l->tok_type = TOK_ERROR;
		return true;
	}
	return false;
}

static bool lex_string_or_char(Lexer *l, int stype, int ctype)
{
	if (stype && lex_one_ignore(l, lex_quote)) {
		vec_clear(&l->tok);
		do {
			if (!lex_stringbody(l, &l->tok) ||
			    !lex_one_ignore(l, lex_quote)) {
				l->tok_type = TOK_ERROR;
				return true;
			}
			if (skip_spaces(l) != 0) {
				l->tok_type = TOK_ERROR;
				return true;
			}
		} while (lex_one_ignore(l, lex_quote));
		vec_push(&l->tok, 0);
		l->tok_type = stype;
		return true;
	}
	if (ctype) {
		int res = lex_char(l);
		if (res != 256) {
			l->tok_type = ctype;
			l->u.char_cst = res;
			return true;
		}
	}
	return false;
}

static bool lex_string_or_char_prefix(Lexer *l, const char *prefix)
{
	char c = text_stream_peek(l->ts);
	if (c == '"' || c == '\'') {
		int ctype = 0;
		int stype = 0;
		bool raw = false;
		switch(prefix[0]) {
		case 'L':
			raw = (prefix[1] == 'R' && prefix[2] == 0);
			if (raw || prefix[1] == 0) {
				ctype = TOK_WCHAR_CST;
				stype = TOK_WSTRING_CST;
			}
			break;
		case 'u':
			raw = (prefix[1] == 'R' && prefix[2] == 0);
			if (raw || prefix[1] == 0) {
				ctype = TOK_WCHAR_CST_u;
				stype = TOK_WSTRING_CST_u;
			}
			if (prefix[1] == '8') {
				raw = (prefix[2] == 'R' && prefix[3] == 0);
				if (raw || prefix[2] == 0) {
					ctype = TOK_WCHAR_CST_u8;
					stype = TOK_WSTRING_CST_u8;
				}
			}
			break;
		case 'U':
			raw = (prefix[1] == 'R' && prefix[2] == 0);
			if (raw || prefix[1] == 0) {
				ctype = TOK_WCHAR_CST;
				stype = TOK_WSTRING_CST;
			}
			break;
		case 'R':
			raw = (prefix[1] == 0);
			if (raw) {
				ctype = 0;
				stype = TOK_STRING_CST;
			}
			break;
		}
		if (raw) { // gcc extension
			vec_clear(&l->tok);
			if (lex_raw_stringbody(l, &l->tok)) {
				l->tok_type = stype;
			} else {
				l->tok_type = TOK_ERROR;
			}
			return true;
		}
		if (lex_string_or_char(l, stype, ctype)) {
			return true;
		}
	}
	return false;
}

void lexer_next(Lexer *l)
{
	int tt;
	vec_clear(&l->tok);
	tt = skip_spaces(l);
	if (tt) {
		l->tok_type = tt;
		vec_clear(&l->tok);
		vec_extend(&l->tok, &l->tok_temp);
		return;
	}
	if (lex_one(l, lex_alpha_)) {
		lex_many(l, lex_alphadigit_);
		vec_push(&l->tok, 0);
		if (lex_string_or_char_prefix(l, l->tok.data)) {
			return;
		}
		int *type = map_get(&l->kws, l->tok.data);
		if (type) {
			l->tok_type = *type;
		} else {
			l->tok_type = TOK_IDENT;
		}
	} else if (lex_one(l, lex_0)) {
		if (lex_one(l, lex_xX)) {
			if (lex_many(l, lex_hex)) {
				if (!lex_many_dsep(l, lex_hex))
					return;
				if (!lex_0xfloat(l)) {
					vec_push(&l->tok, 0);
					handle_int_cst(l, 2, 16);
				}
			} else {
				l->tok_type = TOK_ERROR;
			}
		} else if (lex_one(l, lex_bB)) { // gcc extension
			if (lex_many(l, lex_bin)) {
				if (!lex_many_dsep(l, lex_bin))
					return;
				vec_push(&l->tok, 0);
				handle_int_cst(l, 2, 2);
			} else {
				l->tok_type = TOK_ERROR;
			}
		} else {
			bool exist_dsep = lex_one_ignore(l, lex_dsep);
			if (lex_many(l, lex_digit)) {
				if (!lex_many_dsep(l, lex_digit))
					return;
				if (!lex_float(l)) {
					vec_push(&l->tok, 0);
					handle_int_cst(l, 1, 8);
				}
			} else {
				if (exist_dsep) {
					l->tok_type = TOK_ERROR;
					return;
				}
				if (!lex_float(l)) {
					vec_push(&l->tok, 0);
					handle_int_cst(l, 0, 10); // zero
				}
			}
		}
	} else if (lex_many(l, lex_digit)) {
		if (!lex_many_dsep(l, lex_digit))
			return;
		if (!lex_float(l)) {
			vec_push(&l->tok, 0);
			handle_int_cst(l, 0, 10);
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
		if (lex_string_or_char(l, TOK_STRING_CST, TOK_CHAR_CST)) {
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

	l->line = 1;
	l->path[255] = 0;
	strncpy(l->path, "", 255);
	vec_init(&l->tok_temp);

	l->file[0] = 0;

	lexer_next(l);
}

static void lexer_free(Lexer *l)
{
	vec_deinit(&l->tok);
	map_deinit(&l->kws);
	vec_deinit(&l->tok_temp);
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

const char *lexer_report_path(Lexer *l)
{
	return l->path;
}

const char *lexer_report_file(Lexer *l)
{
	return l->file;
}

int lexer_report_line(Lexer *l)
{
	return l->line;
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
