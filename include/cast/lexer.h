#ifndef LEXER_H
#define LEXER_H

typedef struct TextStream_ TextStream;

TextStream *text_stream_new(const char *file);
TextStream *text_stream_from_string(const char *string);
void text_stream_delete(TextStream *ts);
char text_stream_peek(TextStream *ts);
void text_stream_next(TextStream *ts);

typedef struct Lexer_ Lexer;
enum {
	TOK_AND = 128,  // &&
	TOK_ASSIGNBAND, // &=
	TOK_OR,         // ||
	TOK_ASSIGNBOR,  // |=
	TOK_NEQ,        // !=
	TOK_ASSIGNBXOR, // ^=
	TOK_LE,         // <=
	TOK_BSHL,       // <<
	TOK_ASSIGNBSHL, // <<=
	TOK_GE,         // >=
	TOK_BSHR,       // >>
	TOK_ASSIGNBSHR, // >>=
	TOK_DEC,        // --
	TOK_ASSIGNSUB,  // -=
	TOK_PMEM,       // ->
	TOK_INC,        // ++
	TOK_ASSIGNADD,  // +=
	TOK_ASSIGNMUL,  // *=
	TOK_ASSIGNDIV,  // /=
	TOK_ASSIGNMOD,  // %=
	TOK_EQ,         // ==
	TOK_DOT3,       // ...
	TOK_END = 256,
	TOK_ERROR,
	TOK_IDENT,
	TOK_INT_CST,
	TOK_UINT_CST,
	TOK_LONG_CST,
	TOK_ULONG_CST,
	TOK_LLONG_CST,
	TOK_ULLONG_CST,
	TOK_CHAR_CST,
	TOK_STRING_CST,
	TOK_BOOL_CST,
	TOK_FLOAT_CST,
	TOK_DOUBLE_CST,
	TOK_WCHAR_CST,
	TOK_WSTRING_CST,

	TOK_KEYWORD_START,
	TOK_FOR,
	TOK_WHILE,
	TOK_DO,
	TOK_BREAK,
	TOK_CONTINUE,
	TOK_RETURN,
	TOK_IF,
	TOK_ELSE,
	TOK_SWITCH,
	TOK_CASE,
	TOK_DEFAULT,
	TOK_GOTO,
	TOK_TYPEDEF,
	TOK_STRUCT,
	TOK_UNION,
	TOK_ENUM,

	TOK_STATIC,
	TOK_AUTO,
	TOK_REGISTER,
	TOK_EXTERN,
	TOK_INLINE,
	TOK_VOLATILE,
	TOK_RESTRICT,
	TOK_SIZEOF,

	TOK_VOID,
	TOK_INT,
	TOK_LONG,
	TOK_SHORT,
	TOK_CHAR,
	TOK_SIGNED,
	TOK_UNSIGNED,
	TOK_CONST,
	TOK_FLOAT,
	TOK_DOUBLE,
	TOK_BOOL,

	// C11
	TOK_ATOMIC,
	TOK_THREADLOCAL,
	TOK_NORETURN,
	TOK_ALIGNOF,
	TOK_GENERIC,
	TOK_STATICASSERT,

	// gcc extensions
	TOK_INT128,
	TOK_EXTENSION,
	TOK_AUTOTYPE,
	TOK_TYPEOF,
	TOK_ATTRIBUTE,
	TOK_ASM,
	TOK_LABEL,
	TOK_KEYWORD_END,

	TOK_MANAGED = 16384,
};

Lexer *lexer_new(TextStream *ts);
void lexer_delete(Lexer *l);
void lexer_next(Lexer *l);
int lexer_peek(Lexer *l);
const char *lexer_peek_string(Lexer *l);
int lexer_peek_string_len(Lexer *l);
unsigned long long lexer_peek_uint(Lexer *l);
double lexer_peek_float(Lexer *l);
char lexer_peek_char(Lexer *l);

const char *lexer_report_path(Lexer *l);
int lexer_report_line(Lexer *l);

#endif /* LEXER_H */
