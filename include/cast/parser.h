#ifndef PARSER_H
#define PARSER_H

#include "tree.h"

typedef struct Lexer_ Lexer;
typedef struct Parser_ Parser;

Parser *parser_new(Lexer *l);
void parser_delete(Parser *p);

BEGIN_MANAGED

StmtBLOCK *parse_translation_unit(Parser *p);

bool parse_decl(Parser *p, Stmt **);
Stmt *parse_stmt(Parser *p);
Type *parse_type(Parser *p);
Expr *parse_expr(Parser *p);

END_MANAGED

#endif /* PARSER_H */
