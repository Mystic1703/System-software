#pragma once
#include <regex>
#include <string>
using namespace std;

class AssemblerRegex
{
public:
    //string helpers
    string asm_symbol_regex = "[a-zA-Z][_A-Za-z0-9]*";
    string literal_dec = "-?[0-9]+";
    string literal_hex = "0x[0-9A-F]+";
    string reg = "(r[0-7]|psw)";
    regex asm_regex = regex("^(" + asm_symbol_regex + ")$");
    //filter regexes
    regex any = regex("(.*)");
    regex more_spaces = regex(" {2,}");
    regex tabs = regex("\\t");
    regex comma_space = regex(" ?, ?");
    regex label_space = regex(" ?: ?");
    regex boundary_space = regex("^( *)([^ ].*[^ ])( *)$");
    regex comments = regex("^([^#]*)#.*");
    //labels
    regex label_with_data = regex("^(" + asm_symbol_regex + "):(.*)$");
    regex label_only = regex("^(" + asm_symbol_regex + "):$");
    //directive regexes
    regex global = regex("^\\.global (" + asm_symbol_regex + "(," + asm_symbol_regex + ")*)$");
    regex extern_directive = regex("^\\.extern (" + asm_symbol_regex + "(," + asm_symbol_regex + ")*)$");
    regex section = regex("^\\.section (" + asm_symbol_regex + ")$");
    regex word = regex("^\\.word ((" + asm_symbol_regex + "|" + literal_dec
        + "|" + literal_hex + ")(,(" + asm_symbol_regex + "|" + 
            literal_dec + "|" + literal_hex + "))*)$");
    regex skip = regex("^\\.skip (" + literal_dec + "|" + literal_hex + ")$");
    regex equ = regex("^\\.equ (" + asm_symbol_regex + "),(" + literal_dec + "|" + literal_hex + ")$");
    regex end = regex("^\\.end$");
    //operation regexes
    //--------------------------------------
    regex no_operand = regex("^(halt|iret|ret)$");
    regex one_operand_jump = regex("^(jmp|jeq|jne|jgt|call) (.*)$");
    regex one_register_instructions = regex("^(int|push|pop|not) " + reg + "$");
    regex two_register_instructions = regex("^(xchg|add|sub|mul|div|cmp|and|or|xor|test|shl|shr) " + reg + "," + reg + "$");
    regex one_register_one_operand_instuctions = regex("^(ldr|str) " + reg + ",(.*)$");
    //operands
    regex absolute_operand = regex("^\\$(" + literal_dec + "|" + literal_hex + "|" + asm_symbol_regex + ")$");
    regex mem_operand_absolute_address = regex("^(" + literal_dec + "|" + literal_hex + "|" + asm_symbol_regex +")$");
    regex mem_operand_pc_relative = regex("^%(" + asm_symbol_regex + ")$");
    regex mem_operand_register = regex("^\\[" + reg + "\\]$");
    regex operand_register = regex("^" + reg + "$");
    regex mem_operand_register_add = regex("^\\[" + reg + " \\+ (" + literal_dec + "|" + literal_hex + "|" + asm_symbol_regex + ")\\]$");
    //jump operand
    regex jump_operand_absolute = regex("^(" + literal_dec + "|" + literal_hex + "|" + asm_symbol_regex + "|" + ")$");
    regex jump_mem_operand = regex("^\\*(" + literal_dec + "|" + literal_hex + "|" + asm_symbol_regex + ")$");
    regex jump_reg_operand = regex("^\\*" + reg + "$");
    regex jump_mem_register_operand = regex("^\\*\\[" + reg + "\\]$");
    regex jump_mem_register_add_operand = regex("^\\*\\[" + reg + " \\+ (" + literal_dec + "|" + literal_hex + "|" + asm_symbol_regex + ")\\]$");
};