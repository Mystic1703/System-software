#include "../inc/Linker.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
int Linker::load_binary_file(string filename)
{
	ifstream input("../tests/" + filename, ios::binary);
	if (!input.is_open())
	{
		cout << "Error while opening a binary file!" << endl;
		return -1;
	}

	unsigned section_count;
	input.read((char *)&section_count, sizeof(section_count));
	map<string, LinkerSectionTable> section_map;
	for (unsigned i = 0; i < section_count; i++)
	{
		string name;
		int size, id;

		unsigned len;
		input.read((char *)&len, sizeof(len));
		name.resize(len);
		input.read((char *)name.c_str(), len);
		input.read((char *)&size, sizeof(size));
		input.read((char *)&id, sizeof(id));
		LinkerSectionTable section(size, name, id, 0);

		unsigned offsets_size, bytes_size;

		input.read((char *)&offsets_size, sizeof(offsets_size));
		for (unsigned i = 0; i < offsets_size; i++)
		{
			int value;
			input.read((char *)&value, sizeof(value));
			section.offsets.push_back(value);
		}

		input.read((char *)&bytes_size, sizeof(bytes_size));
		for (unsigned i = 0; i < bytes_size; i++)
		{
			char byte;
			input.read((char *)&byte, sizeof(byte));
			section.bytes.push_back(byte);
		}
		section_map.insert({name, section});
	}
	if (!section_map.empty())
	{
		section_tables.insert({filename, section_map});
	}

	unsigned symbols_count;
	input.read((char *)&symbols_count, sizeof(symbols_count));

	map<string, SymbolTableRow> symbol_map;
	for (unsigned i = 0; i < symbols_count; i++)
	{
		string name;
		unsigned size;
		input.read((char *)&size, sizeof(size));
		name.resize(size);
		input.read((char *)name.c_str(), size);

		int sym_id, offset, section_id;
		string section;
		bool defined, local, extern_sym;

		input.read((char *)&sym_id, sizeof(sym_id));
		input.read((char *)&offset, sizeof(offset));
		input.read((char *)&section_id, sizeof(section_id));

		input.read((char *)&size, sizeof(size));
		section.resize(size);
		input.read((char *)section.c_str(), size);

		input.read((char *)&defined, sizeof(defined));
		input.read((char *)&local, sizeof(local));
		input.read((char *)&extern_sym, sizeof(extern_sym));
		SymbolTableRow row(offset, section, sym_id, section_id, local, defined, extern_sym);
		symbol_map.insert({name, row});
	}
	if (!symbol_map.empty())
	{
		symbol_tables.insert({filename, symbol_map});
	}

	unsigned relocation_table_count;
	input.read((char *)&relocation_table_count, sizeof(relocation_table_count));
	list<RellocationTableRow> rel_list;
	for (unsigned i = 0; i < relocation_table_count; i++)
	{
		unsigned size;
		string section;
		input.read((char *)&size, sizeof(size));
		section.resize(size);
		input.read((char *)section.c_str(), size);

		int offset, addend;
		string type, sym;
		bool endian;

		input.read((char *)&offset, sizeof(offset));

		input.read((char *)&size, sizeof(size));
		type.resize(size);
		input.read((char *)type.c_str(), size);

		input.read((char *)&size, sizeof(size));
		sym.resize(size);
		input.read((char *)sym.c_str(), size);

		input.read((char *)&endian, sizeof(endian));
		input.read((char *)&addend, sizeof(addend));

		RellocationTableRow row(section, offset, type, sym, addend, endian);
		rel_list.push_back(row);
	}
	if (!rel_list.empty())
	{
		rellocation_tables.insert({filename, rel_list});
	}
	input_files.push_back(filename);
	input.close();
	return 0;
}

void Linker::link(list<string> file_names)
{
	int ret;
	for (string file : file_names)
	{
		ret = load_binary_file(file);
		if (ret < 0)
			return;
	}
	if (operation == "hex")
	{
		ret = calculate_section_sizes_and_start_address_hex();
	}
	else
	{
		ret = calculate_section_sizes_and_start_address_linkable();
	}
	if (ret < 0)
		return;
	ret = this->create_main_symbol_table();
	if (ret < 0)
		return;
	print_symbol_table();
	ret = this->create_main_rellocation_table();
	if (ret < 0)
		return;
	print_rellocation_table();
	ret = merge_section_data();
	if (ret < 0)
		return;
	if (operation == "hex")
	{
		ret = resolve_rellocations();
		if (ret < 0)
			return;
	}
	this->print_main_section_table();
	if (operation == "hex")
	{
		ret = generate_output_hex_file();
	}
	else
	{
		ret = generate_output_linkable_file();
	}
	if (ret < 0)
		return;
}

void Linker::print_symbol_table()
{
	cout << "Symbol table:" << endl
		 << endl;
	for (auto x : main_symbol_table)
	{
		SymbolTableRow row = x.second;
		cout << "Symbol name: " << x.first << "\t Data from row: " << row.sym_id << " " << row.section_id << " " << row.section << " " << setw(4) << setfill('0') << hex << (row.offset & 0xffff) << " " << row.local << " " << row.defined << endl;
	}
}

list<string> Linker::split_string(string line, string delimiter)
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

int Linker::calculate_section_sizes_and_start_address_hex()
{
	list<string>::iterator it;
	for (it = input_files.begin(); it != input_files.end(); it++)
	{
		map<string, LinkerSectionTable> &section_table_map = section_tables.at(it->c_str());
		for (auto &x : section_table_map)
		{
			int start_adr;
			string section_name = x.first;
			if (x.first == "ABS")
			{
				continue;
			}
			if (main_section_table.count(section_name) == 0)
			{
				if (start_addresses.count(section_name) == 1)
				{
					start_adr = start_addresses.at(section_name);
				}
				else
				{
					continue; //will be processed later
				}
				LinkerSectionTable table(x.second.size, x.second.name, x.second.id, start_adr);
				x.second.start_address = start_adr;

				list<string>::iterator it2;

				for (it2 = it; it2 != input_files.end(); it2++)
				{
					if (it2 == it)
					{
						continue;
					}
					map<string, LinkerSectionTable> &section_table_map_2 = section_tables.at(it2->c_str());
					if (section_table_map_2.count(x.first) == 1)
					{
						LinkerSectionTable &table_2 = section_table_map_2.at(x.first);
						table_2.start_address = table.start_address + table.size;
						table.size += table_2.size;
					}
				}
				main_section_table.insert({x.first, table});
			}
		}
	}
	//at the end of this for all place-address sections are added
	//check for intersection
	if (check_for_intersection())
	{
		cout << "Sections are intersected" << endl;
		return -2;
	}
	this->next_address = find_max_start_address();
	if (next_address < 0)
		next_address = 0;

	//add sections that havent predefined start address

	for (it = input_files.begin(); it != input_files.end(); it++)
	{
		map<string, LinkerSectionTable> &section_table_map = section_tables.at(it->c_str());

		for (auto &x : section_table_map)
		{
			int start_adr;
			string section_name = x.first;
			if (x.first == "ABS")
			{
				continue;
			}
			if (main_section_table.count(section_name) == 0)
			{
				if (start_addresses.count(section_name) == 1)
				{
					continue;
				}
				else
				{
					start_adr = this->next_address;
				}
				LinkerSectionTable table(x.second.size, x.second.name, x.second.id, start_adr);
				x.second.start_address = start_adr;

				list<string>::iterator it2;

				for (it2 = it; it2 != input_files.end(); it2++)
				{
					if (it2 == it)
					{
						continue;
					}
					map<string, LinkerSectionTable> &section_table_map_2 = section_tables.at(it2->c_str());
					if (section_table_map_2.count(x.first) == 1)
					{
						LinkerSectionTable &table_2 = section_table_map_2.at(x.first);
						table_2.start_address = table.start_address + table.size;
						table.size += table_2.size;
					}
				}
				next_address += table.size;
				main_section_table.insert({x.first, table});
			}
		}
	}
	//all sections are added to main section, without data
	return 0;
}

int Linker::calculate_section_sizes_and_start_address_linkable()
{
	list<string>::iterator it;
	for (it = input_files.begin(); it != input_files.end(); it++)
	{
		map<string, LinkerSectionTable> &section_table_map = section_tables.at(it->c_str());
		for (auto &x : section_table_map)
		{
			string section_name = x.first;
			if (x.first == "ABS")
			{
				continue;
			}
			if (main_section_table.count(section_name) == 0)
			{
				LinkerSectionTable table(x.second.size, x.second.name, x.second.id, 0);
				x.second.start_address = 0;

				list<string>::iterator it2;

				for (it2 = it; it2 != input_files.end(); it2++)
				{
					if (it2 == it)
					{
						continue;
					}
					map<string, LinkerSectionTable> &section_table_map_2 = section_tables.at(it2->c_str());
					if (section_table_map_2.count(x.first) == 1)
					{
						LinkerSectionTable &table_2 = section_table_map_2.at(x.first);
						table_2.start_address = table.start_address + table.size;
						table.size += table_2.size;
					}
				}
				main_section_table.insert({x.first, table});
			}
		}
	}
	return 0;
}

bool Linker::check_for_intersection()
{
	map<string, LinkerSectionTable>::iterator it1, it2;
	for (it1 = main_section_table.begin(); it1 != main_section_table.end(); it1++)
	{
		for (it2 = it1; it2 != main_section_table.end(); it2++)
		{
			if (it1 == it2)
				continue;
			int start_adr1 = it1->second.start_address;
			int start_adr2 = it2->second.start_address;
			int end_adr1 = it1->second.size + start_adr1;
			int end_adr2 = it2->second.size + start_adr2;
			bool intersect = ((start_adr1 <= end_adr2) && (start_adr2 <= end_adr1));
			if (intersect)
				return intersect;
		}
	}
	return false;
}

int Linker::find_max_start_address()
{
	int address = -1;
	int end_address = -1;
	for (auto x : main_section_table)
	{
		if (address < x.second.start_address)
		{
			address = x.second.start_address;
			end_address = address + x.second.size;
		}
	}
	return end_address;
}

int Linker::literal_to_int(string lit)
{
	regex hex_regex = regex("^0x[0-9A-Fa-f]+$");
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

void Linker::parse_start_address(list<string> line_list)
{
	regex place("-place=([a-zA-Z][_A-Za-z0-9]*)@0x([0-9A-Fa-f]+)");
	smatch match;
	for (string elem : line_list)
	{
		if (regex_match(elem, match, place))
		{
			string section = match.str(1);
			int address = literal_to_int("0x" + match.str(2));
			start_addresses.insert({section, address});
		}
	}
}

void Linker::print_main_section_table()
{
	for (auto x : main_section_table)
	{
		cout << "Name: " << x.first << " Start address: 0x" << setw(4) << setfill('0') << hex << (x.second.start_address & 0xffff) << " Size: " << x.second.size << endl;
		int start, end;
		cout << "Table data:\t";
		LinkerSectionTable table = x.second;
		for (int i = 0; i < table.offsets.size(); i++)
		{
			cout << endl;
			start = table.offsets.at(i);
			if (i == (table.offsets.size() - 1))
			{
				end = table.bytes.size() + table.start_address;
			}
			else
			{
				end = table.offsets.at(i + 1);
			}
			cout << start << ": ";
			while (start < end)
			{
				cout << setw(2) << setfill('0') << hex << ((int)table.bytes.at(start - table.start_address) & 0xff) << " ";
				start++;
			}
		}
		cout << endl;
	}
}

int Linker::create_main_symbol_table()
{
	//add sections to symbol table
	int sym_id = 1;
	for (auto &x : main_section_table)
	{
		LinkerSectionTable table = x.second;
		SymbolTableRow row(table.start_address, x.first, true, true, false);
		row.section_id = sym_id++;
		row.sym_id = row.section_id;
		x.second.id = row.section_id;
		main_symbol_table.insert({x.first, row});
	}

	list<string>::iterator it;
	for (it = input_files.begin(); it != input_files.end(); it++)
	{
		map<string, SymbolTableRow> &symbol_table_map = symbol_tables.at(it->c_str());
		for (auto &x : symbol_table_map)
		{
			if ((x.second.section == x.first) || (x.second.local && (x.second.section != "ABS")))
			{
				continue;
			}

			if (x.second.extern_sym)
			{
				extern_symbols.insert({x.first, x.second});
			}
			else //global defined symbols
			{
				if (main_symbol_table.count(x.first) == 1)
				{
					cout << "Global symbol with this name already exists in symbol table" << endl;
					return -3;
				}
				if (x.second.section == "ABS")
				{
					//LinkerSectionTable sec_table = section_tables.at(it->c_str()).at("ABS");
					//int high = (((int)sec_table.bytes.at(x.second.offset + 1)) << 8) & 0x0ff00;
					//int low = (int)sec_table.bytes.at(x.second.offset) & 0xff;
					//new_offset = (high | low) & 0xffff;
					SymbolTableRow row(x.second.offset, x.second.section, sym_id++, -1, false, true, false);
					main_symbol_table.insert({x.first, row});
				}
				else
				{
					LinkerSectionTable table = main_section_table.at(x.second.section);
					map<string, LinkerSectionTable> section_table_map = section_tables.at(it->c_str());
					int new_offset = section_table_map.at(x.second.section).start_address + x.second.offset;
					SymbolTableRow row(new_offset, x.second.section, sym_id++, table.id, false, true, false);
					main_symbol_table.insert({x.first, row});
				}
			}
		}
	}

	for (auto x : extern_symbols)
	{
		if (main_symbol_table.count(x.first) == 0)
		{
			cout << "Extern symbol " << x.first << " ins't defined in any file" << endl;
			return -3;
		}
	}
	return 0;
}

int Linker::create_main_rellocation_table()
{
	list<string>::iterator it;
	for (it = input_files.begin(); it != input_files.end(); it++)
	{
		list<RellocationTableRow> rel_table;
		if (rellocation_tables.count(it->c_str()) == 0)
		{
			continue;
		}
		rel_table = rellocation_tables.at(it->c_str());
		for (RellocationTableRow &row : rel_table)
		{
			map<string, LinkerSectionTable> section_table_map = section_tables.at(it->c_str());
			int new_offset = section_table_map.at(row.section).start_address + row.offset;
			LinkerRellocationTableRow rel_row(row.section, new_offset, row.type, row.sym, row.addend, row.big_endian, it->c_str());
			main_rellocation_table.push_back(rel_row);
		}
	}
	return 0;
}

void Linker::print_rellocation_table()
{
	cout << "Rellocation table data " << endl;
	for (RellocationTableRow row : main_rellocation_table)
	{
		cout << "Section: " << row.section << "\tSymbol: " << row.sym << "\tOffset: " << setw(4) << setfill('0') << hex << (row.offset & 0xffff) << endl;
	}
}

int Linker::merge_section_data()
{
	int address = 0;
	map<string, LinkerSectionTable>::iterator section_it;
	for (section_it = main_section_table.begin(); section_it != main_section_table.end(); section_it++)
	{
		LinkerSectionTable &main_section = section_it->second;
		address = main_section.start_address;
		list<string>::iterator string_it;
		for (string_it = input_files.begin(); string_it != input_files.end(); string_it++)
		{
			if (section_tables.at(string_it->c_str()).count(main_section.name) == 0)
			{
				continue;
			}
			LinkerSectionTable curr_section_table = section_tables.at(string_it->c_str()).at(main_section.name);
			address = curr_section_table.start_address;
			for (int i = 0; i < curr_section_table.offsets.size(); i++)
			{
				int start = curr_section_table.offsets.at(i);
				int end;
				if (i == curr_section_table.offsets.size() - 1)
				{
					end = curr_section_table.bytes.size();
				}
				else
				{
					end = curr_section_table.offsets.at(i + 1);
				}
				main_section.offsets.push_back(address + start);
				while (start < end)
				{
					main_section.bytes.push_back(curr_section_table.bytes.at(start));
					start++;
				}
			}
		}
	}
	return 0;
}

int Linker::resolve_rellocations()
{
	for (LinkerRellocationTableRow rel_row : main_rellocation_table)
	{
		LinkerSectionTable &section = main_section_table.at(rel_row.section);
		SymbolTableRow symbol = main_symbol_table.at(rel_row.sym);
		int old_value;
		char high, low;
		low = section.bytes.at(rel_row.offset - section.start_address);
		high = section.bytes.at(rel_row.offset - section.start_address + 1);
		if (rel_row.big_endian)
		{
			char tmp = low;
			low = high;
			high = tmp;
		}
		old_value = (((int)high << 8) & 0x0ff00) | ((int)low & 0x0ff);
		int new_value;
		if (rel_row.type == "R_PROC_16")
		{
			if (symbol.section == rel_row.sym) //check if sym == section then add section start from that file as offset to old value
			{
				map<string, LinkerSectionTable> section_table_map = section_tables.at(rel_row.file);
				int start_address = section_table_map.at(symbol.section).start_address;
				new_value = old_value + start_address;
			}
			else
			{
				new_value = old_value + symbol.offset;
			}
		}
		else
		{
			if (main_section_table.count(rel_row.sym) == 1) //it means symbol is local
			{
				new_value = old_value - rel_row.offset;
				map<string, LinkerSectionTable> section_table_map = section_tables.at(rel_row.file);
				int start_address = section_table_map.at(symbol.section).start_address;
				new_value += start_address;
			}
			else
			{
				new_value = old_value + symbol.offset - rel_row.offset;
			}
		}
		section.bytes.at(rel_row.offset - section.start_address + 1) = (char)(new_value);
		section.bytes.at(rel_row.offset - section.start_address) = (char)(new_value >> 8);
	}
	return 0;
}

int Linker::generate_output_hex_file()
{
	ofstream output(this->output_filename);
	if (!output.is_open())
	{
		cout << "Error while creating output file" << endl;
		return -1;
	}
	list<int> section_list;
	int start_address, end_address = -1;
	int bytes_written_in_line = 0, bytes_written_from_curr_section = 0;
	for (auto x : main_section_table)
	{
		section_list.insert(lower_bound(section_list.begin(), section_list.end(), x.second.start_address), x.second.start_address);
	}
	for (int x : section_list)
	{
		start_address = x;
		bytes_written_from_curr_section = 0;
		if (end_address != -1)
		{
			if (end_address != start_address)
			{
				if ((bytes_written_in_line % 8) != 0)
					output << endl;
				bytes_written_in_line = 0;
			}
		}
		for (auto y : main_section_table)
		{
			if (y.second.start_address == x)
			{
				LinkerSectionTable section = y.second;
				for (char c : section.bytes)
				{
					if ((bytes_written_in_line % 8) == 0)
					{
						output << setw(4) << setfill('0') << hex << ((start_address + bytes_written_in_line) & 0xffff) << ": ";
					}
					output << setw(2) << setfill('0') << hex << ((int)c & 0xff) << " ";
					bytes_written_in_line++;
					bytes_written_from_curr_section++;
					if ((bytes_written_in_line % 8) == 0)
						output << endl;
				}
				end_address = start_address + bytes_written_from_curr_section;
				break;
			}
		}
	}
	output.close();
	return 0;
}

void Linker::linker(string operation, list<string> input_files, string output_file, list<string> places)
{
	this->operation = operation;
	if (output_file != "")
	{
		this->output_filename = output_file;
	}
	if (operation == "hex")
	{
		this->parse_start_address(places);
	}
	this->link(input_files);
}

int Linker::generate_output_linkable_file()
{
	int ret = generate_binary_file();
	if (ret < 0)
	{
		return ret;
	}
	if (this->output_filename != "")
	{
		ret = generate_object_file();
		return ret;
	}
	else
	{
		return 0;
	}
}

int Linker::generate_binary_file()
{
	regex file_object_regex("^([a-zA-Z][_A-Za-z0-9]*)\\.o$");
	smatch match;
	if (this->output_filename == "")
	{
		this->output_filename = "default.o";
	}
	if (regex_search(this->output_filename, match, file_object_regex))
	{
		ofstream linker_output("../tests/" + match.str(1) + ".dat", ios::binary);
		if (!linker_output.is_open())
		{
			cout << "Error while opening an output binary file" << endl;
			return -1;
		}
		unsigned section_tables_count = main_section_table.size();
		linker_output.write((char *)&section_tables_count, sizeof(section_tables_count));

		for (auto x : main_section_table)
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

		unsigned symbol_table_count = main_symbol_table.size();

		linker_output.write((char *)&symbol_table_count, sizeof(symbol_table_count));

		for (auto x : main_symbol_table)
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
		unsigned size = main_rellocation_table.size();
		linker_output.write((char *)&size, sizeof(size));

		for (RellocationTableRow rel : main_rellocation_table)
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
		this->output_filename = "";
		return 0;
	}
	return -2;
}

int Linker::generate_object_file()
{
	regex file_object_regex("^([a-zA-Z][_A-Za-z0-9]*)\\.o$");
	string filename = this->output_filename;
	if (!regex_search(filename, file_object_regex))
	{
		cout << "Output filename has to be with .o extension!" << endl;
		return -1;
	}
	ofstream output(this->output_filename);
	if (!output.is_open())
	{
		cout << "Error while creating output file" << endl;
		return -1;
	}
	output << "Symbol table" << endl;
	output << "ID\tName\tSection\tValue\tLocal/Global" << endl;
	for (auto x : main_symbol_table)
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

	for (auto x : main_section_table)
	{
		SectionTable table = x.second;
		string section = x.first;
		output << "Name: " << section << endl;
		output << "Relocation data" << endl;
		output << "Offset\tType\tSymbol" << endl;
		for (RellocationTableRow y : main_rellocation_table)
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