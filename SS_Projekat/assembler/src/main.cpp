#include "../inc/assembler.h"
using namespace std;
#include <iostream>
#include <sstream>
#include <iomanip>
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cout << "Not enough command line arguments passed" << endl;
        return -1;
    }
    string firstArg = argv[1];
    if (firstArg == "asembler")
    {
        if (argc < 4)
        {
            cout << "Not enough command line arguments passed" << endl;
            return -2;
        }
        else
        {
            string second_arg = argv[2];
            if (second_arg != "-o")
            {
                cout << "Error in command line arguments!" << endl;
                return -5;
            }
            Assembler assembler;
            assembler.assemble(argv[4], argv[3]);
            return 0;
        }
    }
}