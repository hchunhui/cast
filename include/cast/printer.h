#ifndef PRINTER_H
#define PRINTER_H

#include "tree.h"

typedef struct Printer_ Printer;

Printer *printer_new();
void printer_delete(Printer *p);

void printer_set_print_type_annot(Printer *self, bool b);
void printer_print_translation_unit(Printer *self, StmtBLOCK *s);

#endif /* PRINTER_H */
