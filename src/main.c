#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <ctree/lexer.h>
#include <ctree/parser.h>

void toplevel_print(StmtBLOCK *s);
int toplevel_test(const char *file)
{
	int ret = 0;
	TextStream *ts = text_stream_new(file);
	Lexer *l = lexer_new(ts);
	Parser *p = parser_new(l);

	StmtBLOCK *translation_unit = parse_translation_unit(p);
	if (translation_unit) {
		toplevel_print(translation_unit);
	} else {
		fprintf(stderr, "%s:%d: syntax error\n",
			lexer_report_path(l),
			lexer_report_line(l));
		ret = 1;
	}

	parser_delete(p);
	lexer_delete(l);
	text_stream_delete(ts);
	return ret;
}

int main(int argc, char *argv[])
{
	return toplevel_test(argv[1]);
}
