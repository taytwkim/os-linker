#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <string.h>
#include <string>

/* 
 * Implement a two-pass linker.
 * 
 * The first pass scans the input file and builds the
 * module_base_table and the symbol_table.
 * 
 * get_token() and read* are helper functions used in
 * both passes to consume one token at a time from the
 * file stream. 
 */

// File state
FILE *fp;
int linenum;
int curr_line_offset;

// Helpful in finding the EOF offset.
int end_of_line_offset;

// Map module # -> module base, updated after first pass.
std::map<int, int> module_base_table;

// Map symbol -> absolute address, updated after first pass.
std::map<std::string, int> symbol_table;

// Map symbol -> error messages from first pass.
std::map<std::string, std::string> errors;

int inst_num;

/*
 * Read and return one token at a time from a file stream.
 * 
 * Delimiters are space, tab, and new line.
 * 
 * File state is maintained as a global fp variable.
 * 
 * Return nullptr if we reached EOF and there is no token to read.
 * 
 * linenum and curr_line_offset are updated after each call 
 * to the position of the first character of the token.
 * 
 * If we are at EOF, they are set to one past the position of the 
 * last character in the file.
 */
char *getToken()
{
    // Static variables are not reset at each call.
    static char *line;
    static char *pch;
    static size_t capacity;
    const char *delim = " \t\n";

    while (pch == nullptr) {
        ssize_t len = getline(&line, &capacity, fp);

        if (len == -1) {
            curr_line_offset = end_of_line_offset;
            return nullptr;
        }

        end_of_line_offset = line[len-1] == '\n' ? len : len + 1;

        ++linenum;

        pch = std::strtok(line, delim);

        if (pch != nullptr) {
            curr_line_offset = pch - line + 1;
            return pch;
        }
    }

    // If we are at the end of line, pch could be nullptr.
    pch = std::strtok(nullptr, delim);

    if (pch == nullptr) {
        // If we are at the end of line,
        // we have to find and return the next valid line/token
        // instead of returning nullptr.
        return getToken();
    }

    curr_line_offset = pch - line + 1;

    return pch;
}

// Read one token and return an integer value.
// Return -1 if we reached EOF.
int readInt()
{
    char *tok = getToken();

    if (tok == nullptr) {
        // EOF
        return -1;
    }

    return std::stoi(tok);
}

// Read one symbol and return a string value.
// Return nullopt if we reached EOF.
std::optional<std::string> readSym()
{
    char *tok = getToken();

    if (tok == nullptr) {
        return std::nullopt;
    }

    std::string s = tok;
    return s;
}

// Return one address mode (M, A, R, I, E).
// Return nullopt if we reached EOF.
std::optional<char> readMARIE()
{
    char *tok = getToken();

    if (tok == nullptr) {
        // EOF
        return std::nullopt;
    }

    return *tok;
}

// Make first pass over the input file.
// Builds module_base_table and symbol table
// needed in the second pass.
void pass1()
{
    int module_num = 0;
    int module_base = 0;

    // One iteration corresponds to one module.
    while (true) {
        int defcount = readInt();

        if (defcount < 0) {
            return;
        }

        for (int i = 0; i < defcount; ++i) {
            std::string sym = readSym().value_or("EOF");

            if (sym == "EOF") {
                return;
            }

            int val = readInt();

            auto it = symbol_table.find(sym);
            
            if (it != symbol_table.end()) {
                // RULE 2
                // If a symbol is defined multiple times,  
                // print an error message and use the first definition.  
                // The error message is to appear as part of printing the symbol table 
                // (following symbol=value printout on the same line).

                std::string msg = "Error: This variable is multiple times defined; first value used";
                errors[sym] = msg;
            }
            else {
                symbol_table[sym] = module_base + val;
            }
        }

        int useCount = readInt();

        for (int i = 0; i < useCount; ++i) {
            std::string sym = readSym().value_or("EOF");

            if (sym == "EOF") {
                return;
            }
        }

        int instCount = readInt();

        for (int i = 0; i < instCount; ++i) {
            char addressMode = readMARIE().value_or('F');
            
            if (addressMode == 'F') {
                // F means EOF
                return;
            }
            
            int operand = readInt();
        }

        module_base_table[module_num] = module_base;
        ++module_num;
        module_base += instCount;
    }

    std::cout << "\n";
}

// void print_pass1_results()
// {
//     std::cout << "\nPass 1 Results: \n";

//     std::cout << "\nModule Base Table: \n";

//     for (auto [mod_num, mod_base] : module_base_table) {
//         std::cout << mod_num << ": " << mod_base << "\n";
//     }

//     std::cout << "\nSymbol Table: \n";

//     for (auto [sym, addr] : symbol_table) {
//         std::cout << sym << ": " << addr << "\n";
//     }
// }

void printSymbolTable()
{
    std::cout << "Symbol Table\n";

    for (auto [sym, addr] : symbol_table) {
        std::cout << sym << "=" << addr;

        auto it = errors.find(sym);
        
        if (it != errors.end()) {
            std::string err_msg = it->second;
            std::cout << " " << err_msg; 
        }

        std::cout << "\n";
    }
}

void printInst(int opcode, int operand)
{
    std::string opcode_s = std::to_string(opcode);
    std::string operand_s = std::to_string(operand);

    std::cout << std::setfill('0') << std::setw(3) << std::to_string(inst_num) << ": "
              << std::to_string(opcode)
              << std::setfill('0') << std::setw(3) << std::to_string(operand) << "\n";
}

void processMARIE(char addressMode, int inst)
{
    int opcode = inst / 1000;
    int operand = inst % 10;

    switch (addressMode) {
        case 'M':
            // Module addressing
            // Operand is the number of a valid module 
            // and is replaced with the base address of that module.

            operand = module_base_table[operand];
            printInst(opcode, operand);
            break;

        case 'A':
            // Absolute addressing
            // Operand is an absolute address which will never be changed in pass2; 
            // however it can’t be “>=” the machine size (512).

            break;

        case 'R':
            // Relative addressing
            // Replace the relative address with the absolute address,
            // where absolute_addr = module_base+relative_addr. 
            
            break;

        case 'I':
            // Immediate addressing
            // Immediate operand is unchanged, but must be less than 900.
            
            break;

        case 'E':
            // External addressing
            // Operand is an external address which is represented 
            // as an index into the uselist.
            
            break;

        default:
            // Invalid addressing mode
            break;
    }

    ++inst_num;
}

void pass2()
{
    std::cout << "Memory Map\n";

    while (true) {
        int defcount = readInt();

        if (defcount < 0) {
            return;
        }

        for (int i = 0; i < defcount; ++i) {
            std::string sym = readSym().value_or("EOF");

            if (sym == "EOF") {
                return;
            }

            int val = readInt();
        }

        int useCount = readInt();

        for (int i = 0; i < useCount; ++i) {
            std::string sym = readSym().value_or("EOF");

            if (sym == "EOF") {
                return;
            }
        }

        int instCount = readInt();

        for (int i = 0; i < instCount; ++i) {
            char addressMode = readMARIE().value_or('F');
            
            if (addressMode == 'F') {
                // F means EOF
                return;
            }
            
            int inst = readInt();

            processMARIE(addressMode, inst);
        }
    }
}

int main (int argc, char *argv[])
{
    if (argc != 2) {
        std::cout << "Invalid arguments." << std::endl;
        return EXIT_FAILURE;
    }

    char *filename = argv[1];

    fp = fopen(filename, "r");

    if (fp == nullptr) {
        std::cout << "Failed to open file." << std::endl;
        return EXIT_FAILURE;
    }
    
    pass1();
    
    printSymbolTable();

    pass2();

    fclose(fp);

    return EXIT_SUCCESS;
}