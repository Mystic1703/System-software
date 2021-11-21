#include "../inc/assembler.h"
#include "../inc/structures.h"
#include <regex>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

int Assembler::_symbol_id = 0;

void Assembler::filter_input_file()
{
    file_lines.clear();
    string line;
    ifstream in("../tests/" + input_file);
    if (!in.is_open())
    {
        cout << "Error while opening an input file!" << endl;
        return;
    }
    while (getline(in, line))
    {
        line = regex_replace(line, rx.comments, "$1");
        line = regex_replace(line, rx.tabs, " ");
        line = regex_replace(line, rx.more_spaces, " ");
        line = regex_replace(line, rx.boundary_space, "$2");
        line = regex_replace(line, rx.comma_space, ",");
        line = regex_replace(line, rx.label_space, ":");
        if (line == "" || line == " ")
        {
            continue;
        }
        file_lines.push_back(line);
    }
    for (string s : file_lines)
    {
        cout << s << endl;
    }
    in.close();
}

void Assembler::assemble(string input_file, string output_file)
{
    this->input_file = input_file;
    this->output_file = output_file;
    this->filter_input_file();
    if (file_lines.empty())
    {
        cout << "Input file hasn't been processed!" << endl;
        return;
    }
    else
    {
        int ret = this->assembler_first_pass();
        if (ret < 0)
            exit(-1);
        ret = this->assembler_second_pass();
        if (ret < 0)
            exit(-2);
        ret = make_binary_file_for_linker();
        if (ret < 0)
            exit(-3);
        ret = make_output_file();
        if (ret < 0)
            exit(-4);
        symbol_table.clear();
        rellocation_table.clear();
        section_tables.clear();
        this->_symbol_id = 0;
    }
}

int Assembler::process_label(string label_name)
{
    if (!this->check_symbol_in_table(label_name))
    {
        if (this->curr_section == "")
        {
            cout << "Error at line: " << this->curr_line << ": "
                 << "Label is not in section" << endl;
            return -2;
        }
        int id = ++this->_symbol_id;
        SymbolTableRow row(this->location_counter, this->curr_section, id, this->curr_section_id, true, true, false);
        symbol_table.insert({label_name, row});
        return 0;
    }
    else
    {
        SymbolTableRow &row = symbol_table.at(label_name);
        if (!row.defined)
        {
            row.defined = true;
            row.offset = this->location_counter;
            row.section = this->curr_section;
            row.section_id = this->curr_section_id;
            return 0;
        }
        else
        {
            cout << "Error at line " << curr_line << endl;
            return -1;
        }
    }
}

bool Assembler::check_symbol_in_table(string symbol)
{
    if (this->symbol_table.count(symbol) == 1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

int Assembler::section_first_pass(smatch match)
{
    string section_name = match.str(1);
    if (this->check_symbol_in_table(section_name))
    {
        cout << "Error at line " << curr_line << ": Symbol with this name already exists" << endl;
        return -1;
    }
    int id = ++this->_symbol_id;
    SymbolTableRow row(0, section_name, id, id, true, true, false);
    this->symbol_table.insert({section_name, row});
    if (this->curr_section != "") //there was a section before
    {
        SectionTable section_table(this->location_counter, this->curr_section, this->curr_section_id);
        this->section_tables.insert({this->curr_section, section_table});
    }
    this->curr_section = section_name;
    this->curr_section_id = id;
    this->location_counter = 0;
    return 0;
}

int Assembler::global_first_pass(smatch match)
{
    string symbols = match.str(1);
    list<string> symbol_list = this->split_string(symbols, ",");
    for (string sym : symbol_list)
    {
        if (!this->check_symbol_in_table(sym))
        {
            SymbolTableRow row(this->location_counter, "UND", ++this->_symbol_id, 0, false, false, false); //global symbol not defined yet
            this->symbol_table.insert({sym, row});
        }
        else
        {
            SymbolTableRow &row = symbol_table.at(sym);
            if (row.defined)
            {
                row.local = false;
            }
            else
            {
                cout << "Error at line " << curr_line << endl;
                return -2;
            }
        }
    }
    return 0;
}

int Assembler::extern_first_pass(smatch match)
{
    string symbols = match.str(1);
    list<string> symbol_list = this->split_string(symbols, ",");
    for (string sym : symbol_list)
    {
        if (!this->check_symbol_in_table(sym))
        {
            SymbolTableRow row(0, "UND", ++this->_symbol_id, 0, false, false, true); //extern symbol
            this->symbol_table.insert({sym, row});
        }
        else
        {
            SymbolTableRow &row = symbol_table.at(sym);
            if (row.defined || !row.local)
            {
                cout << "Error in importing symbol " << sym << ": Definition already exists!" << endl;
                return -3;
            }
            else
            {
                /*row.extern_sym = true;
                row.offset = 0;*/
                cout << "Error at line " << curr_line << endl;
                return -3;
            }
        }
    }
    return 0;
}

int Assembler::literal_to_int(string lit)
{
    regex hex_regex = regex("^" + rx.literal_hex + "$");
    if (regex_search(lit, hex_regex))
    {
        unsigned int x = std::stoul(lit, nullptr, 16);
        return static_cast<int>(x);
    }
    else
    {
        stringstream ss(lit);
        int y;
        ss >> y;
        return y;
    }
}

int Assembler::skip_first_pass(smatch match)
{
    string literal_str = match.str(1);
    int value = literal_to_int(literal_str);
    if (value < 0)
    {
        cout << "Error at line " << curr_line << ": Skip directive can't have negative literal!" << endl;
        return -4;
    }
    this->location_counter += value;
    return 0;
}

int Assembler::equ_first_pass(smatch match)
{
    string sym = match.str(1);
    string lit = match.str(2);
    int value = literal_to_int(lit);
    value &= 0xffff;
    SectionTable &abs_table = section_tables.at("ABS");
    if (!check_symbol_in_table(sym))
    {
        SymbolTableRow row(value, "ABS", ++_symbol_id, -1, true, true, false);
        symbol_table.insert({sym, row});
    }
    else
    {
        //can be defined as global, so it is in symbol table
        SymbolTableRow &row = symbol_table.at(sym);
        if (row.defined || row.extern_sym)
        {
            cout << "Error at line " << curr_line << ": Symbol already defined!" << endl;
            return -5;
        }
        row.offset = abs_table.size;
        row.section = "ABS";
        row.section_id = -1;
        row.defined = true;
    }
    abs_table.offsets.push_back(abs_table.size);
    abs_table.size += 2; //literals are 2 bytes long, enough to store any address in memory
    abs_table.bytes.push_back((char)value);
    abs_table.bytes.push_back((char)(value >> 8));
    return 0;
}

int Assembler::word_first_pass(smatch match)
{
    if (curr_section == "")
    {
        cout << "Error at line " << curr_line << ": Word directive is out of section" << endl;
        return -6;
    }
    string operands = match.str(1);
    list<string> operand_list = split_string(operands, ",");
    this->location_counter = this->location_counter + operand_list.size() * 2;
    return 0;
}

int Assembler::command_first_pass(string command)
{
    //directive check
    smatch match;
    int ret;
    if (regex_match(command, match, rx.section))
    {
        ret = section_first_pass(match);
        if (ret < 0)
            return ret;
    }
    else if (regex_match(command, match, rx.global))
    {
        ret = global_first_pass(match);
        if (ret < 0)
            return ret;
    }
    else if (regex_match(command, match, rx.equ))
    {
        ret = equ_first_pass(match);
        if (ret < 0)
            return ret;
    }
    else if (regex_match(command, match, rx.skip))
    {
        ret = skip_first_pass(match);
        if (ret < 0)
            return ret;
    }
    else if (regex_match(command, match, rx.extern_directive))
    {
        ret = extern_first_pass(match);
        if (ret < 0)
            return ret;
    }
    else if (regex_match(command, match, rx.word))
    {
        ret = word_first_pass(match);
        if (ret < 0)
            return ret;
    }
    else if (regex_search(command, rx.end))
    {
        if (finish_first_assembly_pass())
        {
            return 0;
        }
        else
        {
            cout << "Error at the end of first assembly pass!" << endl;
            return -10;
        }
    }
    else
    {
        ret = instruction_first_pass(command);
        if (ret < 0)
        {
            cout << "Error at line " << curr_line << ": Invalid command line" << endl;
            return -11;
        }
    }
    return 0;
}

int Assembler::calculate_jump_instruction_size(string operand)
{
    if (regex_search(operand, rx.jump_operand_absolute))
    {
        //literal or symbol with absolute addressing
        return 5;
    }
    else if (regex_search(operand, rx.mem_operand_pc_relative))
    {
        //pc relative addressing
        return 5;
    }
    else if (regex_search(operand, rx.jump_reg_operand))
    {
        //address in register
        return 3;
    }
    else if (regex_search(operand, rx.jump_mem_operand))
    {
        //address in memory at payload address
        return 5;
    }
    else if (regex_search(operand, rx.jump_mem_register_operand))
    {
        //address in memory on address stored in register
        return 3;
    }
    else if (regex_search(operand, rx.jump_mem_register_add_operand))
    {
        //address in memory on address reg + payload
        return 5;
    }
    else
    {
        cout << "Error at line " << curr_line << ": Invalid operand" << endl;
        return -1;
    }
}

int Assembler::calculate_data_instruction_size(string reg, string operand)
{
    if (!regex_match(reg, rx.operand_register))
    {
        cout << "Error at line " << curr_line << ": invalid register" << endl;
        return -1;
    }
    if (regex_match(operand, rx.operand_register) || regex_match(operand, rx.mem_operand_register))
    {
        return 3;
    }
    else if (regex_match(operand, rx.mem_operand_absolute_address) || regex_match(operand, rx.mem_operand_pc_relative) || regex_match(operand, rx.absolute_operand) || regex_match(operand, rx.mem_operand_register_add))
    {
        return 5;
    }
    else
    {
        cout << "Error at line " << curr_line << ": invalid operand format" << endl;
        return -2;
    }
}

int Assembler::instruction_first_pass(string instruction)
{
    if (curr_section == "")
    {
        cout << "Error at line " << curr_line << ": Instruction isn't in any section!" << endl;
        return -1;
    }
    //Only calculate size of instructions for location counter, dont proccess the instruction
    smatch match;
    if (regex_match(instruction, match, rx.no_operand))
    {
        //1B instructions
        location_counter++;
        return 0;
    }
    else if (regex_match(instruction, match, rx.two_register_instructions))
    {
        //2B instructions
        location_counter += 2;
        return 0;
    }
    else if (regex_match(instruction, match, rx.one_register_instructions))
    {
        string instr = match.str(1);
        if (instr == "inc" || instr == "not")
        {
            location_counter += 2;
            return 0;
        }
        else if (instr == "push" || instr == "pop")
        {
            location_counter += 3;
            return 0;
        }
        return -1; //dead code
    }
    else if (regex_match(instruction, match, rx.one_register_one_operand_instuctions))
    {
        string reg = match.str(2);
        string operand = match.str(3);
        int instruction_size = calculate_data_instruction_size(reg, operand);
        if (instruction_size < 0)
            return -3;
        location_counter += instruction_size;
        return 0;
    }
    else if (regex_match(instruction, match, rx.one_operand_jump))
    {
        string operand = match.str(2);
        int instruction_size = calculate_jump_instruction_size(operand);
        if (instruction_size < 0)
            return -2;
        location_counter += instruction_size;
        return 0;
    }
    else
    {
        cout << "Error at line " << curr_line << ": Can't parse instruction" << endl;
        return -4;
    }
}

list<string> Assembler::split_string(string line, string delimiter)
{
    size_t pos = 0;
    std::string token;
    list<string> elements;
    while ((pos = line.find(delimiter)) != std::string::npos)
    {
        token = line.substr(0, pos);
        elements.push_back(token);
        line.erase(0, pos + delimiter.length());
    }
    elements.push_back(line);
    return elements;
}

void Assembler::create_const_section()
{
    SectionTable abs(0, "ABS", -1);
    section_tables.insert({"ABS", abs});
    SymbolTableRow row(0, "ABS", ++this->_symbol_id, -1, true, true, false);
    symbol_table.insert({"ABS", row});
}

int Assembler::assembler_first_pass()
{
    this->location_counter = 0;
    this->curr_section = "";
    this->curr_line = 0;
    this->curr_section_id = 0;
    this->file_ended = false;
    create_const_section();

    for (string line : this->file_lines)
    {
        if (file_ended)
        {
            break;
        }
        this->curr_line++;
        smatch match;
        if (regex_search(line, match, rx.label_only))
        {
            string label_name = match.str(1);
            int ret = this->process_label(label_name);
            if (ret < 0)
                return ret;
        }
        else
        {
            string command;
            if (regex_search(line, match, rx.label_with_data))
            {
                string label_name = match.str(1);
                int ret = this->process_label(label_name);
                if (ret < 0)
                    return ret;
                command = match.str(2); //catching command after label
            }
            else
            {
                command = line; //line is command, if there is no label
            }
            int ret = this->command_first_pass(command);
            if (ret < 0)
                return ret;
        }
    }
    //display_structures();
    return 0;
}

void Assembler::display_structures()
{
    cout << "Symbol table:" << endl
         << endl;
    for (auto x : symbol_table)
    {
        SymbolTableRow row = x.second;
        cout << "Symbol name: " << x.first << "\t Data from row: " << row.sym_id << " " << row.section_id << " " << row.section << " " << row.offset << " " << row.local << " " << row.defined << endl;
    }
    cout << "Section tables:" << endl
         << endl;
    for (auto x : section_tables)
    {
        SectionTable table = x.second;
        cout << "Section name: " << x.first << "\tSize: " << table.size << "\tID: " << table.id << endl;
    }

    cout << "Absolute section data: " << endl;
    SectionTable &abs_table = section_tables.at("ABS");
    int i = 0;
    for (char c : abs_table.bytes)
    {
        if ((i % 2) == 0)
        {
            cout << "\tOffset " << i << ": ";
        }
        cout << setw(2) << setfill('0') << hex << ((int)c & 0xff);
        i++;
    }
}

bool Assembler::finish_first_assembly_pass()
{
    //close last section
    if (curr_section != "")
    {
        SectionTable table(location_counter, curr_section, curr_section_id);
        section_tables.insert({curr_section, table});
    }
    for (auto x : symbol_table)
    {
        SymbolTableRow row = x.second;
        if (!row.defined && !row.extern_sym)
        {
            cout << "Error at the end of first assembly pass: Symbol " << x.first << " isn't defined" << endl;
            return false;
        }
    }
    this->file_ended = true;
    return true;
}

int Assembler::assembler_second_pass()
{
    curr_line = 0;
    this->curr_section = "";
    this->curr_section_id = 0;
    this->file_ended = false;
    this->location_counter = 0;
    for (string line : file_lines)
    {
        if (file_ended)
        {
            break;
        }
        smatch match;
        curr_line++;
        string command = line;
        if (regex_search(line, rx.label_only))
        {
            //nothing to do here
            continue;
        }
        else if (regex_match(line, match, rx.label_with_data))
        {
            command = match.str(2);
        }
        int ret = command_second_pass(command);
        if (ret < 0)
            return ret;
    }
    return 0;
}

int Assembler::command_second_pass(string command)
{
    //if command is \\.(.*)
    smatch match;
    int ret;

    if (regex_match(command, match, rx.global))
    {
        //all done in first pass
        return 0;
    }
    else if (regex_match(command, match, rx.skip))
    {
        return skip_second_pass(match);
    }
    else if (regex_match(command, match, rx.section))
    {
        return section_second_pass(match);
    }
    else if (regex_search(command, rx.equ))
    {
        return 0;
    }
    else if (regex_match(command, match, rx.word))
    {
        return word_second_pass(match);
    }
    else if (regex_match(command, match, rx.extern_directive))
    {
        return 0;
    }
    else if (regex_match(command, match, rx.end))
    {
        this->file_ended = true;
        return 0;
    }
    else
    {
        //instruction process
        return instruction_second_pass(command);
    }
}

int Assembler::skip_second_pass(smatch match)
{
    string bytes = match.str(1);
    int size = this->literal_to_int(bytes);
    if (size < 0)
        return -1;
    SectionTable &table = section_tables.at(curr_section);
    table.offsets.push_back(location_counter);
    for (int i = 0; i < size; i++)
    {
        table.bytes.push_back(0x00);
    }
    location_counter += size;
    return 0;
}

int Assembler::section_second_pass(smatch match)
{
    string section = match.str(1);
    location_counter = 0;
    curr_section = section;
    SymbolTableRow &row = symbol_table.at(section);
    curr_section_id = row.sym_id;
    return 0;
}

int Assembler::word_second_pass(smatch match)
{
    string symbols = match.str(1);
    list<string> symbol_list = split_string(symbols, ",");
    SectionTable &curr_table = section_tables.at(curr_section);
    char high, low;
    for (string sym : symbol_list)
    {
        curr_table.offsets.push_back(location_counter);
        if (!regex_search(sym, rx.asm_regex))
        {
            //constant
            int value = literal_to_int(sym) & 0xffff;
            curr_table.bytes.push_back((char)value);
            curr_table.bytes.push_back((char)(value >> 8));
        }
        else
        {
            if (symbol_table.count(sym) == 0)
            {
                cout << "Error at line " << curr_line << ": Symbol isn't in symbol table" << endl;
                return -3;
            }
            SymbolTableRow &row = symbol_table.at(sym);
            if (row.section_id == -1) //from absolute section
            {
                curr_table.bytes.push_back((char)row.offset);
                curr_table.bytes.push_back((char)(row.offset >> 8));
            }
            else
            {
                if (row.defined)
                {
                    if (row.local)
                    {
                        int value = row.offset & 0xffff;
                        curr_table.bytes.push_back((char)value);
                        curr_table.bytes.push_back((char)(value >> 8));

                        RellocationTableRow rel_row(curr_section, location_counter, "R_PROC_16", row.section, 0, false); //local symbol, creating rel row for section
                        rellocation_table.push_back(rel_row);
                    }
                    else
                    {
                        //global defined symbol
                        curr_table.bytes.push_back(0x00);
                        curr_table.bytes.push_back(0x00);

                        RellocationTableRow rel_row(curr_section, location_counter, "R_PROC_16", sym, 0, false);
                        rellocation_table.push_back(rel_row);
                    }
                }
                else
                {
                    if (row.extern_sym)
                    {
                        curr_table.bytes.push_back(0x00);
                        curr_table.bytes.push_back(0x00);

                        RellocationTableRow rel_row(curr_section, location_counter, "R_PROC_16", sym, 0, false);
                        rellocation_table.push_back(rel_row);
                    }
                    else
                    {
                        cout << "Error at line " << curr_line << endl;
                        return -3;
                    }
                }
            }
        }
        location_counter += 2;
    }
    return 0;
}

int Assembler::instruction_second_pass(string line)
{
    smatch match;
    string mnemonic;
    SectionTable &table = section_tables.at(curr_section);
    int ret;

    if (regex_match(line, match, rx.no_operand))
    {
        mnemonic = match.str(1);
        table.offsets.push_back(location_counter);
        if (mnemonic == "halt")
        {
            table.bytes.push_back(0x00);
        }
        else if (mnemonic == "iret")
        {
            table.bytes.push_back(0x20);
        }
        else
        {
            table.bytes.push_back(0x40);
        }
        location_counter++;
        return 0;
    }
    else if (regex_match(line, match, rx.one_register_instructions))
    {
        mnemonic = match.str(1);
        string reg = match.str(2);
        return one_register_instruction_second_pass(mnemonic, reg, table);
    }
    else if (regex_match(line, match, rx.two_register_instructions))
    {
        mnemonic = match.str(1);
        string reg_dst = match.str(2);
        string reg_src = match.str(3);
        return two_register_instruction_second_pass(mnemonic, reg_src, reg_dst, table);
    }
    else if (regex_match(line, match, rx.one_operand_jump))
    {
        mnemonic = match.str(1);
        string operand = match.str(2);
        return one_operand_instruction_second_pass(mnemonic, operand, table);
    }
    else if (regex_match(line, match, rx.one_register_one_operand_instuctions))
    {
        mnemonic = match.str(1);
        string reg = match.str(2);
        string operand = match.str(3);
        return one_operand_one_register_instruction_second_pass(mnemonic, operand, reg, table);
    }
    else
    {
        cout << "Error at line " << curr_line << ": Can't parse instruction" << endl;
        return -10;
    }
}

int Assembler::get_reg_index(string reg)
{
    string reg_num;
    if (reg == "psw")
    {
        reg_num = "8";
    }
    else
    {
        reg_num = reg.substr(1, 2);
    }
    int reg_index = literal_to_int(reg_num);
    if (reg_index > 8 || reg_index < 0)
        return -1;
    return reg_index;
}

int Assembler::one_register_instruction_second_pass(string mnemonic, string reg, SectionTable &table)
{
    int reg_index = get_reg_index(reg);
    table.offsets.push_back(location_counter);
    if (mnemonic == "int")
    {
        table.bytes.push_back(0x10);
        table.bytes.push_back((char)((reg_index << 4) | 0x0f));
        location_counter += 2;
    }
    else if (mnemonic == "not")
    {
        table.bytes.push_back(0x80);
        table.bytes.push_back((char)((reg_index << 4) | reg_index));
        location_counter += 2;
    }
    else if (mnemonic == "push")
    {
        table.bytes.push_back(0xB0);
        table.bytes.push_back((char)((reg_index << 4) | 0x06));
        table.bytes.push_back(0x12);
        location_counter += 3;
    }
    else
    {
        //pop
        table.bytes.push_back(0xA0);
        table.bytes.push_back((char)((reg_index << 4) | 0x06));
        table.bytes.push_back(0x42);
        location_counter += 3;
    }
    return 0;
}

int Assembler::two_register_instruction_second_pass(string mnemonic, string reg_src, string reg_dst, SectionTable &table)
{
    int reg_dst_index = get_reg_index(reg_dst);
    int reg_src_index = get_reg_index(reg_src);
    //(xchg|add|sub|mul|div|cmp|and|or|xor|test|shl|shr)
    table.offsets.push_back(location_counter);

    if (mnemonic == "add")
    {
        table.bytes.push_back(0x70);
    }
    else if (mnemonic == "sub")
    {
        table.bytes.push_back(0x71);
    }
    else if (mnemonic == "mul")
    {
        table.bytes.push_back(0x72);
    }
    else if (mnemonic == "div")
    {
        table.bytes.push_back(0x73);
    }
    else if (mnemonic == "cmp")
    {
        table.bytes.push_back(0x74);
    }
    else if (mnemonic == "and")
    {
        table.bytes.push_back(0x81);
    }
    else if (mnemonic == "or")
    {
        table.bytes.push_back(0x82);
    }
    else if (mnemonic == "xor")
    {
        table.bytes.push_back(0x83);
    }
    else if (mnemonic == "test")
    {
        table.bytes.push_back(0x84);
    }
    else if (mnemonic == "shl")
    {
        table.bytes.push_back(0x90);
    }
    else if (mnemonic == "shr")
    {
        table.bytes.push_back(0x91);
    }
    else if (mnemonic == "xchg")
    {
        table.bytes.push_back(0x60);
    }
    else
    {
        //cant drop here
        return -5;
    }
    table.bytes.push_back((char)((reg_dst_index << 4) | reg_src_index));
    location_counter += 2;
    return 0;
}

int Assembler::one_operand_one_register_instruction_second_pass(string mnemonic, string operand, string reg, SectionTable &table)
{
    table.offsets.push_back(location_counter);
    int ret;
    smatch match;
    int reg_dst_index = get_reg_index(reg);
    if (reg_dst_index < 0)
        return -9;
    if (mnemonic == "ldr")
    {
        table.bytes.push_back(0xA0);
    }
    else //mnemonic == str
    {
        table.bytes.push_back(0xB0);
    }
    if (regex_search(operand, rx.operand_register) || regex_search(operand, rx.mem_operand_register))
    {
        int reg_src_index = -1;
        if (regex_match(operand, match, rx.operand_register))
        {
            reg_src_index = get_reg_index(match.str(1));
            if (reg_src_index < 0)
                return -9;
            ret = ldr_str_second_pass_three_bytes_register(reg_src_index, reg_dst_index, table);
        }
        else if (regex_match(operand, match, rx.mem_operand_register))
        {
            reg_src_index = get_reg_index(match.str(1));
            if (reg_src_index < 0)
                return -9;
            ret = ldr_str_second_pass_three_bytes_mem(reg_src_index, reg_dst_index, table);
        }
        else
        {
            cout << "Error at line " << curr_line << endl;
            ret = -9;
        }
        if (ret < 0)
            return ret;
        location_counter += 3;
        return 0;
    }
    else
    {
        //5B INSTRUCTIONS
        if (regex_match(operand, match, rx.absolute_operand))
        {
            string parsed_operand = match.str(1);
            ret = ldr_str_second_pass_absolute_operand(reg_dst_index, parsed_operand, table);
            if (ret < 0)
                return ret;
        }
        else if (regex_match(operand, match, rx.mem_operand_absolute_address))
        {
            string parsed_operand = match.str(1);
            ret = ldr_str_second_pass_mem_operand_absolute_address(reg_dst_index, parsed_operand, table);
            if (ret < 0)
                return ret;
        }
        else if (regex_match(operand, match, rx.mem_operand_pc_relative))
        {
            string parsed_operand = match.str(1);
            ret = ldr_str_second_pass_mem_operand_pc_relative_address(reg_dst_index, parsed_operand, table);
            if (ret < 0)
                return ret;
        }
        else if (regex_search(operand, match, rx.mem_operand_register_add))
        {
            string reg_src = match.str(1);
            string parsed_operand = match.str(2);
            int reg_src_index = get_reg_index(reg_src);
            ret = ldr_str_second_pass_mem_operand_register_add(reg_dst_index, reg_src_index, parsed_operand, table);
            if (ret < 0)
                return ret;
        }
        else
        {
            cout << "Error at line " << curr_line << endl;
            return -9;
        }
        location_counter += 5;
        return 0;
    }
}

int Assembler::ldr_str_second_pass_three_bytes_mem(int src, int dst, SectionTable &table)
{
    table.bytes.push_back((char)((dst << 4) | (src & 0xf)));
    table.bytes.push_back(0x02);
    return 0;
}

int Assembler::ldr_str_second_pass_three_bytes_register(int src, int dst, SectionTable &table)
{
    table.bytes.push_back((char)((dst << 4) | (src & 0xf)));
    table.bytes.push_back(0x01);
    return 0;
}

int Assembler::ldr_str_second_pass_absolute_operand(int reg, string operand, SectionTable &table)
{
    table.bytes.push_back((char)(reg << 4));
    table.bytes.push_back(0x00);
    if (regex_search(operand, rx.asm_regex)) //symbol
    {
        return embed_symbol_to_section_table_absolute(operand, table);
    }
    else //literal
    {
        int value = literal_to_int(operand);
        value &= 0xffff;
        table.bytes.push_back((char)(value >> 8));
        table.bytes.push_back((char)value);
        return 0;
    }
}

int Assembler::ldr_str_second_pass_mem_operand_absolute_address(int reg, string operand, SectionTable &table)
{
    table.bytes.push_back((char)(reg << 4));
    table.bytes.push_back(0x04);
    if (regex_search(operand, rx.asm_regex))
    {
        return embed_symbol_to_section_table_absolute(operand, table);
    }
    else //literal
    {
        int value = literal_to_int(operand);
        value &= 0xffff;
        table.bytes.push_back((char)(value >> 8));
        table.bytes.push_back((char)value);
        return 0;
    }
}

int Assembler::embed_symbol_to_section_table_absolute(string sym, SectionTable &curr_table)
{
    if (symbol_table.count(sym) == 0)
    {
        cout << "Error at line " << curr_line << ": Symbol isn't in symbol table" << endl;
        return -9;
    }
    SymbolTableRow &row = symbol_table.at(sym);
    if (row.section_id == -1) //ABS TABLE
    {
        SectionTable &abs = section_tables.at("ABS");
        curr_table.bytes.push_back((char)(row.offset >> 8)); //big endian in instruction
        curr_table.bytes.push_back((char)row.offset);
    }
    else
    {
        string description = "R_PROC_16";
        bool endian = true;
        int offset = location_counter + 3;
        int value;
        if (row.defined)
        {
            if (row.local)
            {
                value = row.offset & 0xffff;
                curr_table.bytes.push_back((char)(value >> 8));
                curr_table.bytes.push_back((char)value);
                RellocationTableRow rel_row(curr_section, offset, description, row.section, 0, endian);
                rellocation_table.push_back(rel_row);
            }
            else
            {
                curr_table.bytes.push_back(0x00);
                curr_table.bytes.push_back(0x00);
                RellocationTableRow rel_row(curr_section, offset, description, sym, 0, endian);
                rellocation_table.push_back(rel_row);
            }
        }
        else
        {
            if (row.extern_sym)
            {
                curr_table.bytes.push_back(0x00);
                curr_table.bytes.push_back(0x00);
                RellocationTableRow rel_row(curr_section, offset, description, sym, 0, endian);
                rellocation_table.push_back(rel_row);
            }
            else
            {
                cout << "Error at line " << curr_line << endl;
                return -9;
            }
        }
    }
    return 0;
}

int Assembler::ldr_str_second_pass_mem_operand_pc_relative_address(int reg, string operand, SectionTable &table)
{
    table.bytes.push_back((char)((reg << 4) | 0x7));
    table.bytes.push_back(0x03);
    if (regex_search(operand, rx.asm_regex))
    {
        return embed_symbol_to_section_table_pc_relative(operand, table);
    }
    else
    {
        cout << "Error at line " << curr_line << endl;
        return -9;
    }
}

int Assembler::embed_symbol_to_section_table_pc_relative(string sym, SectionTable &table)
{
    if (symbol_table.count(sym) == 0)
    {
        cout << "Error at line " << curr_line << ": Symbol isn't in symbol table" << endl;
        return -9;
    }
    SymbolTableRow &row = symbol_table.at(sym);
    int offset = location_counter + 3;
    bool endian = true;
    string description = "R_PROC_PC";
    int addend = -2;
    if (row.section_id == -1)
    {
        int value = addend & 0xffff;
        table.bytes.push_back((char)(value >> 8));
        table.bytes.push_back((char)value);
        RellocationTableRow rel_row(curr_section, offset, description, sym, 0, endian);
        rellocation_table.push_back(rel_row);
    }
    else
    {
        if (row.defined) //defined
        {
            if (row.local)
            {
                int value;
                if (row.section_id == curr_section_id) //local in same section as curr section
                {
                    value = row.offset - location_counter - 5; //?
                    value &= 0xffff;
                    table.bytes.push_back((char)(value >> 8));
                    table.bytes.push_back((char)value);
                }
                else //different section
                {
                    value = row.offset + addend;
                    value &= 0xffff;
                    table.bytes.push_back((char)(value >> 8));
                    table.bytes.push_back((char)value);
                    RellocationTableRow rel_row(curr_section, offset, description, row.section, 0, endian);
                    rellocation_table.push_back(rel_row);
                }
            }
            else //global
            {
                int value = addend & 0xffff;
                table.bytes.push_back((char)(value >> 8));
                table.bytes.push_back((char)value);
                RellocationTableRow rel_row(curr_section, offset, description, sym, 0, endian);
                rellocation_table.push_back(rel_row);
            }
        }
        else
        {
            if (row.extern_sym) //extern not defined
            {
                int value = addend & 0xffff;
                table.bytes.push_back((char)(value >> 8));
                table.bytes.push_back((char)value);
                RellocationTableRow rel_row(curr_section, offset, description, sym, 0, endian);
                rellocation_table.push_back(rel_row);
            }
            else
            {
                cout << "Error at line " << curr_line << endl;
                return -9;
            }
        }
    }
    return 0;
}

int Assembler::ldr_str_second_pass_mem_operand_register_add(int dst, int src, string operand, SectionTable &table)
{
    table.bytes.push_back((char)((dst << 4) | src));
    table.bytes.push_back(0x03);
    if (regex_match(operand, rx.asm_regex)) //symbol
    {
        return embed_symbol_to_section_table_absolute(operand, table);
    }
    else //literal
    {
        int value = literal_to_int(operand) & 0xffff;
        table.bytes.push_back((char)(value >> 8));
        table.bytes.push_back((char)value);
        return 0;
    }
}

int Assembler::one_operand_instruction_second_pass(string mnemonic, string operand, SectionTable &table)
{
    table.offsets.push_back(location_counter);
    if (mnemonic == "jmp")
    {
        table.bytes.push_back(0x50);
    }
    else if (mnemonic == "jeq")
    {
        table.bytes.push_back(0x51);
    }
    else if (mnemonic == "jne")
    {
        table.bytes.push_back(0x52);
    }
    else if (mnemonic == "jgt")
    {
        table.bytes.push_back(0x53);
    }
    else //call
    {
        table.bytes.push_back(0x30);
    }
    smatch match;
    if (regex_search(operand, rx.jump_reg_operand) || regex_search(operand, rx.jump_mem_register_operand)) //3B
    {
        string reg_src;
        if (regex_search(operand, match, rx.jump_reg_operand))
        {
            reg_src = match.str(1);
            int reg_src_index = get_reg_index(reg_src);
            if (reg_src_index < 0)
                return -10;

            table.bytes.push_back((0xf << 4) | ((char)reg_src_index));
            table.bytes.push_back(0x01);
        }
        else if (regex_search(operand, match, rx.jump_mem_register_operand))
        {
            reg_src = match.str(1);
            int reg_src_index = get_reg_index(reg_src);
            if (reg_src_index < 0)
                return -10;

            table.bytes.push_back((0xf << 4) | ((char)reg_src_index));
            table.bytes.push_back(0x02);
        }
        location_counter += 3;
    }
    else //5B
    {
        int ret;
        smatch match;
        if (regex_match(operand, match, rx.jump_operand_absolute))
        {
            ret = jump_second_pass_absolute_operand(operand, table);
            if (ret < 0)
                return ret;
        }
        else if (regex_match(operand, match, rx.mem_operand_pc_relative))
        {
            string parsed_operand = match.str(1);
            ret = jump_second_pass_mem_operand_pc_relative_address(parsed_operand, table);
            if (ret < 0)
                return ret;
        }
        else if (regex_match(operand, match, rx.jump_mem_operand))
        {
            string parsed_operand = match.str(1);
            ret = jump_second_pass_mem_operand(parsed_operand, table);
            if (ret < 0)
                return ret;
        }
        else if (regex_match(operand, match, rx.jump_mem_register_add_operand))
        {
            string reg_src = match.str(1);
            string parsed_operand = match.str(2);
            ret = jump_second_pass_mem_register_add_operand(reg_src, parsed_operand, table);
            if (ret < 0)
                return ret;
        }
        else
        {
            cout << "Error at line " << curr_line << ": Invalid operand type" << endl;
            return -10;
        }
        location_counter += 5;
    }
    return 0;
}

int Assembler::jump_second_pass_absolute_operand(string operand, SectionTable &table)
{
    table.bytes.push_back(0xf0);
    table.bytes.push_back(0x00);
    if (regex_match(operand, rx.asm_regex))
    {
        return embed_symbol_to_section_table_absolute(operand, table);
    }
    else //literal
    {
        int value = literal_to_int(operand) & 0xffff;
        table.bytes.push_back((char)(value >> 8));
        table.bytes.push_back((char)value);
        return 0;
    }
}

int Assembler::jump_second_pass_mem_operand(string operand, SectionTable &table)
{
    table.bytes.push_back(0xf0);
    table.bytes.push_back(0x04);
    if (regex_match(operand, rx.asm_regex))
    {
        return embed_symbol_to_section_table_absolute(operand, table);
    }
    else
    {
        int value = literal_to_int(operand) & 0xffff;
        table.bytes.push_back((char)(value >> 8));
        table.bytes.push_back((char)value);
        return 0;
    }
}

int Assembler::jump_second_pass_mem_register_add_operand(string reg, string operand, SectionTable &table)
{
    int reg_index = get_reg_index(reg);
    table.bytes.push_back((0xf << 4) | ((char)reg_index));
    table.bytes.push_back(0x03);

    if (regex_match(operand, rx.asm_regex))
    {
        return embed_symbol_to_section_table_absolute(operand, table);
    }
    else //literal
    {
        int value = literal_to_int(operand) & 0xffff;
        table.bytes.push_back((char)(value >> 8));
        table.bytes.push_back((char)value);
        return 0;
    }
}

int Assembler::jump_second_pass_mem_operand_pc_relative_address(string operand, SectionTable &table)
{
    table.bytes.push_back(0xf7);
    table.bytes.push_back(0x03);

    return embed_symbol_to_section_table_pc_relative(operand, table);
}

void Assembler::print_section_table(string table_name)
{
    SectionTable &table = section_tables.at(table_name);
    cout << "Table name: " << table.name << "\tTable size: " << table.size << endl;

    int start, end;
    cout << "Table data:\t";
    for (int i = 0; i < table.offsets.size(); i++)
    {
        start = table.offsets.at(i);
        if (i == table.offsets.size() - 1)
        {
            end = table.bytes.size();
        }
        else
        {
            end = table.offsets.at(i + 1);
        }
        cout << start << ": ";
        while (start < end)
        {
            cout << setw(2) << setfill('0') << hex << ((int)table.bytes.at(start) & 0xff) << " ";
            start++;
        }
    }
}
//        cout << setw(2) << setfill('0') << hex << ((int)c & 0xff);
int Assembler::make_output_file()
{
    ofstream output(this->output_file);
    if (!output.is_open())
    {
        cout << "Error while creating output file" << endl;
        return -1;
    }
    output << "Symbol table" << endl;
    output << "ID\tName\tSection\tValue\tLocal/Global" << endl;
    for (auto x : symbol_table)
    {
        SymbolTableRow row = x.second;
        string name = x.first;
        output << row.sym_id << "\t" << name << "\t" << row.section << "\t" << setw(4) << setfill('0') << hex << row.offset << "\t";
        if (row.local)
        {
            output << "local";
        }
        else
        {
            if (row.extern_sym)
            {
                output << "extern";
            }
            else
            {
                output << "global";
            }
        }
        output << endl;
    }
    output << "Section tables" << endl;

    for (auto x : section_tables)
    {
        SectionTable table = x.second;
        string section = x.first;
        output << "Name: " << section << endl;
        output << "Relocation data" << endl;
        output << "Offset\tType\tSymbol" << endl;
        for (RellocationTableRow y : rellocation_table)
        {
            if (y.section == section)
            {
                output << setw(4) << setfill('0') << hex << (y.offset & 0xffff) << "\t" << y.type << "\t" << y.sym << endl;
            }
        }
        output << endl;
        output << "Section data" << endl;
        output << "Offset\tData" << endl;
        int start, end;
        for (int i = 0; i < table.offsets.size(); i++)
        {
            start = table.offsets.at(i);
            if (i == table.offsets.size() - 1)
            {
                end = table.bytes.size();
            }
            else
            {
                end = table.offsets.at(i + 1);
            }
            output << start << ":\t";
            while (start < end)
            {
                output << setw(2) << setfill('0') << hex << ((int)table.bytes.at(start) & 0xff) << " ";
                start++;
            }
            output << endl;
        }
    }
    output.close();
    return 0;
}

int Assembler::make_binary_file_for_linker()
{
    regex file_name("^(" + rx.asm_symbol_regex + ").*");
    smatch match;
    if (regex_search(output_file, match, file_name))
    {
        ofstream linker_output("../../zadatak2/tests/" + match.str(1) + ".dat", ios::binary);
        if (!linker_output.is_open())
        {
            cout << "Error while opening an output binary file" << endl;
            return -1;
        }
        unsigned section_tables_count = section_tables.size();
        linker_output.write((char *)&section_tables_count, sizeof(section_tables_count));

        for (auto x : section_tables)
        {
            SectionTable table = x.second;
            unsigned size = table.name.length();
            linker_output.write((char *)&(size), sizeof(size));
            linker_output.write(table.name.c_str(), table.name.length());

            linker_output.write((char *)&table.size, sizeof(table.size));
            linker_output.write((char *)&table.id, sizeof(table.id));

            unsigned offsets_size = table.offsets.size();
            unsigned bytes_size = table.bytes.size();
            linker_output.write((char *)&offsets_size, sizeof(offsets_size));
            for (unsigned i = 0; i < offsets_size; i++)
            {
                int value = table.offsets.at(i);
                linker_output.write((char *)&value, sizeof(value));
            }
            linker_output.write((char *)&bytes_size, sizeof(bytes_size));
            for (unsigned i = 0; i < bytes_size; i++)
            {
                char byte = table.bytes.at(i);
                linker_output.write((char *)&byte, sizeof(byte));
            }
        }

        unsigned symbol_table_count = symbol_table.size();

        linker_output.write((char *)&symbol_table_count, sizeof(symbol_table_count));

        for (auto x : symbol_table)
        {
            string sym_name = x.first;
            SymbolTableRow row = x.second;

            unsigned size = sym_name.length();
            linker_output.write((char *)&(size), sizeof(size));
            linker_output.write(sym_name.c_str(), size);

            linker_output.write((char *)&row.sym_id, sizeof(row.sym_id));
            linker_output.write((char *)&row.offset, sizeof(row.offset));
            linker_output.write((char *)&row.section_id, sizeof(row.section_id));

            size = row.section.length();
            linker_output.write((char *)&(size), sizeof(size));
            linker_output.write(row.section.c_str(), size);

            linker_output.write((char *)&row.defined, sizeof(row.defined));
            linker_output.write((char *)&row.local, sizeof(row.local));
            linker_output.write((char *)&row.extern_sym, sizeof(row.extern_sym));
        }
        unsigned size = rellocation_table.size();
        linker_output.write((char *)&size, sizeof(size));

        for (RellocationTableRow rel : rellocation_table)
        {
            unsigned size = rel.section.length();
            linker_output.write((char *)&(size), sizeof(size));
            linker_output.write(rel.section.c_str(), size);

            linker_output.write((char *)&rel.offset, sizeof(rel.offset));

            size = rel.type.length();
            linker_output.write((char *)&(size), sizeof(size));
            linker_output.write(rel.type.c_str(), size);

            size = rel.sym.length();
            linker_output.write((char *)&(size), sizeof(size));
            linker_output.write(rel.sym.c_str(), size);

            linker_output.write((char *)&rel.big_endian, sizeof(rel.big_endian));

            linker_output.write((char *)&rel.addend, sizeof(rel.addend));
        }
        linker_output.close();
        return 0;
    }
    return -2;
}