#include "lexer.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include "vec.h"
#include "map.h"

struct TextStream_ {
	char *buf;
	long len;
	long i;
};

static void text_stream_init(TextStream *ts, const char *file)
{
	FILE *fp = fopen(file, "r");
	fseek(fp, 0, SEEK_END);
	long len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *buf = (char *) malloc(len);
	fread(buf, 1, len, fp);
	fclose(fp);
	ts->buf = buf;
	ts->len = len;
	ts->i = 0;
}

static void text_stream_free(TextStream *ts)
{
	free(ts->buf);
}

TextStream *text_stream_new(const char *file)
{
	TextStream *self = malloc(sizeof(TextStream));
	text_stream_init(self, file);
	return self;
}

TextStream *text_stream_from_string(const char *string)
{
	TextStream *self = malloc(sizeof(TextStream));
	self->buf = strdup(string);
	self->len = strlen(string);
	self->i = 0;
	return self;
}

void text_stream_delete(TextStream *ts)
{
	text_stream_free(ts);
	free(ts);
}

char text_stream_peek(TextStream *ts)
{
	if (ts->i < ts->len) {
		return ts->buf[ts->i];
	} else {
		return 0;
	}
}

char text_stream_next(TextStream *ts)
{
	ts->i++;
}

struct Lexer_ {
	TextStream *ts;
	map_int_t kws;

	// TODO
	map_void_t typedef_types;

	int tok_type;
	vec_char_t tok;
	union {
		int int_cst;
		char char_cst;
	} u;
};

#define P text_stream_peek(l->ts)
#define N text_stream_next(l->ts)
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
	case '~':
	case '(': case ')':
	case '[': case ']':
	case '{': case '}':
	case ',':
	case '.':
	case ';':
	case ':':
	case '#':
	case '?':
		N; return c;
	default:
		return 0;
	}
}

static int lex_char(Lexer *l)
{
	char c = P;
	if (c == '\'') {
		N; char d = P;
		if (d == '\'') {
			abort(); // TODO
		} else if (d == '\\') {
			N;
			abort(); // TODO
		} else {
			N; char e = P;
			if (e == '\'') {
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

LEX(alpha, isalpha(c))
LEX(alpha_, isalpha(c) || c == '_')
LEX(alphadigit_, isalpha(c) || isdigit(c) || c == '_')
LEX(digit, isdigit(c))
LEX(space, isspace(c))
LEX(strchar, c != '"' && c != '\\')
LEX(quote, c == '"')

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

void lexer_next(Lexer *l)
{
	vec_clear(&l->tok);
	lex_many_ignore(l, lex_space);
	if (lex_one(l, lex_alpha_)) {
		lex_many(l, lex_alphadigit_);
		vec_push(&l->tok, 0);

		int *type = map_get(&l->kws, l->tok.data);
		if (type) {
			l->tok_type = *type;
		} else {
			void **t = map_get(&l->typedef_types, l->tok.data);
			if (t) {
				l->tok_type = TOK_TYPE;
			} else {
				l->tok_type = TOK_IDENT;
			}
		}
	} else if (lex_many(l, lex_digit)) {
		vec_push(&l->tok, 0);
		l->u.int_cst = atoi(l->tok.data);
		l->tok_type = TOK_INT_CST;
	} else if (text_stream_peek(l->ts) == 0) {
		l->tok_type = TOK_END;
	} else {
		int type = lex_punct(l);
		if (type) {
			l->tok_type = type;
			return;
		}
		if (lex_one_ignore(l, lex_quote)) {
			lex_many(l, lex_strchar);
			if (lex_one_ignore(l, lex_quote)) {
				l->tok_type = TOK_STRING_CST;
				return;
			}
			l->tok_type = TOK_ERROR;
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

int lexer_peek_int(Lexer *l)
{
	return l->u.int_cst;
}

char lexer_peek_char(Lexer *l)
{
	return l->u.char_cst;
}

static void lexer_init(Lexer *l, TextStream *ts)
{
	l->ts = ts;
	vec_init(&l->tok);

	map_init(&l->kws);
#define KWS(str, type) map_set(&l->kws, str, type)
#include "keywords.def"
#undef KWS

	map_init(&l->typedef_types);
#define TYPE(str) map_set(&l->typedef_types, str, str)
#undef TYPE
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
	case TOK_TYPE: printf("typedef type %s\n", lexer_peek_string(l)); break;
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
