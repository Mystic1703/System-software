#pragma once
#include <string>
#include <map>
#include <list>
#include "structures.h"
#include "regexes.h"
using namespace std;

class Assembler
{
public:
    void assemble(string input_file, string output_file);
private:
    string input_file;
    string output_file;
    vector<string> file_lines;
    AssemblerRegex rx;
    int location_counter;
    int curr_line;
    static int _symbol_id;
    string curr_section;
    int curr_section_id;
    bool file_ended;
    map<string, SymbolTableRow> symbol_table;
    list<RellocationTableRow> rellocation_table;
    map<string, SectionTable> section_tables;

    int assembler_first_pass();
    int assembler_second_pass();
    int process_label(string label_name);
    void filter_input_file();
    list<string> split_string(string line, string delimiter);
    int literal_to_int(string lit);
    bool check_symbol_in_table(string symbol);
    int command_first_pass(string command);
    void create_const_section();
    void display_structures();
    bool finish_first_assembly_pass();
    int global_first_pass(smatch match);
    int section_first_pass(smatch match);
    int extern_first_pass(smatch match);
    int skip_first_pass(smatch match);
    int equ_first_pass(smatch match);
    //int literal_to_int(string lit);
    int word_first_pass(smatch match);
    int instruction_first_pass(string instruction);
    string int_to_literal(int lit);
    int calculate_jump_instruction_size(string operand);
    int calculate_data_instruction_size(string reg, string operand);
    //second pass
    int command_second_pass(string command);
    int skip_second_pass(smatch match);
    int section_second_pass(smatch match);
    int word_second_pass(smatch match);
    int instruction_second_pass(string instruction);
    int one_register_instruction_second_pass(string mnemonic, string reg, SectionTable& table);
    int two_register_instruction_second_pass(string mnemonic, string reg_src, string reg_dst, SectionTable& table);
    int get_reg_index(string reg);
    int one_operand_instruction_second_pass(string mnemonic, string operand, SectionTable& table);
    int one_operand_one_register_instruction_second_pass(string mnemonic, string operand, string reg, SectionTable& table);
    int ldr_str_second_pass_three_bytes_mem(int src, int dst, SectionTable& table);
    int ldr_str_second_pass_three_bytes_register(int src, int dst, SectionTable& table);
    int ldr_str_second_pass_absolute_operand(int reg, string operand, SectionTable& table);
    int embed_symbol_to_section_table_absolute(string symbol, SectionTable& curr_table);
    int ldr_str_second_pass_mem_operand_absolute_address(int reg, string operand, SectionTable& table);
    int ldr_str_second_pass_mem_operand_pc_relative_address(int reg, string operand, SectionTable& table);
    int embed_symbol_to_section_table_pc_relative(string symbol, SectionTable& curr_table);
    int ldr_str_second_pass_mem_operand_register_add(int reg_dst, int reg_src, string operand, SectionTable& table);
    //jump
    int jump_second_pass_absolute_operand(string operand, SectionTable& table);
    int jump_second_pass_mem_operand(string operand, SectionTable& table);
    int jump_second_pass_mem_register_add_operand(string reg, string operand, SectionTable& table);
    int jump_second_pass_mem_operand_pc_relative_address(string operand, SectionTable& table);
    //print functions
    int make_binary_file_for_linker();
    void print_section_table(string table);
    int make_output_file();
};