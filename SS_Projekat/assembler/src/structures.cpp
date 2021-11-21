#include "../inc/structures.h"

SectionTable::SectionTable(int sz, string section_name, int section_id)
{
    this->size = sz;
    this->name = section_name;
    this->id = section_id;
}

SymbolTableRow::SymbolTableRow(int off, string sec, int symbol_id, int sec_id, bool isLocal, bool isDefined, bool ext) : offset(off), section(sec), sym_id(symbol_id), section_id(sec_id), local(isLocal), defined(isDefined), extern_sym(ext) {}

RellocationTableRow::RellocationTableRow(string sec, int off, string descr, string symbol, int add, bool big_endian) : section(sec), offset(off), type(descr), sym(symbol), addend(add), big_endian(big_endian) {}