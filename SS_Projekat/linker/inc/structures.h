#pragma once
#include <string>
#include <vector>
using namespace std;

class SymbolTableRow
{
public:
    int offset;
    string section;
    int section_id;
    int sym_id;
    bool local;
    bool defined;
    bool extern_sym;
    SymbolTableRow(int off, string sec, int symbol_id, int sec_id, bool isLocal, bool isDefined, bool ext);
    SymbolTableRow(int off, string sec, bool isLocal, bool isDefined, bool ext);
};

class RellocationTableRow
{
public:
    string section;
    int offset;
    string type;
    string sym;
    bool big_endian;
    int addend;
    RellocationTableRow(string sec, int off, string descr, string symbol, int add, bool big_endian);
};

class SectionTable
{
public:
    int size;
    string name;
    int id;
    vector<int> offsets;
    vector<char> bytes;
    SectionTable(int sz, string section_name, int section_id);
};

class LinkerSectionTable : public SectionTable
{
public:
    int start_address;
    LinkerSectionTable(int sz, string section_name, int section_id, int start_address);
};

class LinkerRellocationTableRow : public RellocationTableRow
{
public:
    string file;
    LinkerRellocationTableRow(string sec, int off, string descr, string symbol, int add, bool endian, string file_name): RellocationTableRow(sec, off, descr, symbol, add, endian), file(file_name) {}
};