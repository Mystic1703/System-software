#include "../inc/Linker.h"
using namespace std;
#include <iostream>
#include <sstream>
#include <iomanip>

int main(int argc, char *argv[])
{
	Linker linker;
	string firstArg = argv[1];
	list<string> place_args;
	list<string> input_files;
	string output_file_name = "";
	string operation = "";
	bool write_output_file = false;
	if (firstArg == "linker")
	{
		regex place_reg("-place=([a-zA-Z][_A-Za-z0-9]*)@0x([0-9A-Fa-f]+)");
		regex output_file("^([a-zA-Z][_A-Za-z0-9]*)\\.hex$");
		regex input_file_reg("^([a-zA-Z][_A-Za-z0-9]*)\\.o$");
		int i = 2;
		string arg = "";
		smatch match;
		do
		{
			string arg = argv[i];
			if (arg == "-hex")
			{
				if (operation != "")
				{
					cout << "Error: Multiple hex or linkable commands passed" << endl;
					return -2;
				}
				operation = "hex";
				i++;
			}
			else if (arg == "-linkable")
			{
				if (operation != "")
				{
					cout << "Error: Multiple hex or linkable commands passed" << endl;
					return -2;
				}
				operation = "linkable";
				i++;
			}
			else if (regex_search(arg, place_reg))
			{
				place_args.push_back(arg);
				i++;
			}
			else if (arg == "-o")
			{
				write_output_file = true;
				i++;
				string filename = argv[i];
				output_file_name = filename;
				if (!regex_search(filename, output_file))
				{
					filename = argv[i];
					if (!regex_search(filename, input_file_reg))
					{
						cout << "Error in output filename!" << endl;
						return -3;
					}
				}
				i++;
			}
			else
			{
				break;
			}

		} while (i < argc);

		bool output_file_given = output_file_name != "";
		if ((output_file_given && write_output_file) || (!output_file_given && !write_output_file))
		{

			arg = argv[i];
			if (regex_match(arg, input_file_reg))
			{
				while (i < argc)
				{
					arg = argv[i];
					i++;
					if (regex_search(arg, match, input_file_reg))
					{
						string input_filename = match.str(1);
						input_filename += ".dat";
						input_files.push_back(input_filename);
					}
				}
			}
			else
			{
				cout << "No input files provided!" << endl;
				return -4;
			}
		}
		else
		{
			cout << "Error in command line argument!" << endl;
			return -3;
		}
	}
	else
	{
		cout << "Error in command line arguments!" << endl;
		return -1;
	}
	linker.linker(operation, input_files, output_file_name, place_args);
}