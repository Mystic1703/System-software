#pragma once
#include "structures.h"
#include <map>
#include <list>
#include <regex>
class Linker
{
public:
	int load_binary_file(string filename);
	void link(list<string> files);
	void linker(string operation, list<string> input_files, string output_file, list<string> places);
	void parse_start_address(list<string> line);

private:
	string operation;
	string output_filename;
	map<string, map<string, SymbolTableRow>> symbol_tables;
	map<string, list<RellocationTableRow>> rellocation_tables;
	map<string, map<string, LinkerSectionTable>> section_tables;
	map<string, SymbolTableRow> extern_symbols;
	list<string> input_files;
	int next_address = 0;

	list<string> split_string(string line, string delimiter);
	int calculate_section_sizes_and_start_address_hex();
	int calculate_section_sizes_and_start_address_linkable();
	map<string, LinkerSectionTable> main_section_table;
	map<string, SymbolTableRow> main_symbol_table;
	list<LinkerRellocationTableRow> main_rellocation_table;
	int literal_to_int(string lit);
	map<string, int> start_addresses;
	bool check_for_intersection();
	int find_max_start_address();
	int create_main_symbol_table();
	int create_main_rellocation_table();
	void print_symbol_table();
	void print_main_section_table();
	void print_rellocation_table();
	int merge_section_data();
	int resolve_rellocations();
	int generate_output_hex_file();
	int generate_output_linkable_file();
	int generate_binary_file();
	int generate_object_file();
};