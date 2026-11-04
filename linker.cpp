#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <string.h>
#include <string>
#include <unordered_set>

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

#define MAX_DEF_IN_MODULE 16
#define MAX_USE_IN_MODULE 16
#define MACHINE_SIZE 512
#define MAX_VALID_INT (1 << 30)
#define MAX_SYM_LEN 16

// File state
FILE *fp;
int linenum;
int curr_line_offset;
int end_of_line_offset; // Helpful in finding the EOF offset.

// Map module # -> module base, updated after first pass.
std::map<int, int> module_to_base;

std::map<std::string, int> symbol_to_module;

// Map symbol -> absolute address, updated after first pass.
std::map<std::string, int> symbol_to_addr;

// Map symbol -> error messages from first pass.
std::map<std::string, std::string> errors;

// Symbols referenced by E instructions. For RULE 4.
std::unordered_set<std::string> symbols_used_global;

void parse_error(int errcode)
{
    std::vector<std::string> errmsg = {
        "TOO_MANY_DEF_IN_MODULE",   // > 16
        "TOO_MANY_USE_IN_MODULE",   // > 16
        "TOO_MANY_INSTR",           // total num_instr exceeds memory size (512)
        "NUM_EXPECTED",             // Number expect, anything >= 2^30 is not a number either
        "SYM_EXPECTED",             // Symbol Expected
        "MARIE_EXPECTED",           // Addressing Expected which is M/A/R/I/E
        "SYM_TOO_LONG",             // Symbol Name is too long
    };

    std::cout << "Parse Error line " << linenum << " offset " << curr_line_offset << " : " << errmsg[errcode] << "\n";
    exit(1);
}

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
        return -1;  // EOF
    }

    std::string s = tok;

    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            parse_error(3); // NUM_EXPECTED
        }
    }

    int val = std::stoi(s);

    if (val > MAX_VALID_INT) {
        parse_error(3); // NUM_EXPECTED
    }

    return val;
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

    if (!std::isalpha(s[0])) {
        parse_error(4); // SYM_EXPECTED
    }

    for (char c : s) {
        if (!std::isalnum(c)) {
            parse_error(4); // SYM_EXPECTED
        }
    }

    if (s.size() > MAX_SYM_LEN) {
       parse_error(6);
    }
    
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

    std::unordered_set<char> MARIE = {'M', 'A', 'R', 'I', 'E'};

    auto it = MARIE.find(*tok);

    if (it == MARIE.end()) {
        parse_error(5); // Invalid address mode
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
        std::vector<std::pair<std::string, int>> symbols_defined_in_module;

        int defcount = readInt();

        if (defcount < 0) {
            // EOF
            return;
        }

        if (defcount > MAX_DEF_IN_MODULE) {
            parse_error(0);
        }

        for (int i = 0; i < defcount; ++i) {
            std::string sym = readSym().value_or("EOF");

            if (sym == "EOF") {
                return;
            }

            int val = readInt();

            auto it = symbol_to_addr.find(sym);
            
            if (it != symbol_to_addr.end()) {
                // RULE 2
                // If a symbol is defined multiple times,  
                // print an error message and use the first definition.  
                // The error message is to appear as part of printing the symbol table 
                // (following symbol=value printout on the same line).

                std::string msg = "Error: This variable is multiple times defined; first value used";
                errors[sym] = msg;

                // RULE 5
                // If the symbol is a redefinition then print a warning message
                std::cout << "Warning: Module " << module_num << ":" << sym << "redefinition ignored\n";
            }
            else {
                symbol_to_module[sym] = module_num;
                symbol_to_addr[sym] = module_base + val;
                symbols_defined_in_module.push_back({sym, val});
            }
        }

        int useCount = readInt();

        if (useCount > MAX_USE_IN_MODULE) {
            parse_error(1);
        }

        for (int i = 0; i < useCount; ++i) {
            std::string sym = readSym().value_or("EOF");

            if (sym == "EOF") {
                return;
            }
        }

        int instCount = readInt();

        if (instCount > MACHINE_SIZE) {
            parse_error(2);
        }

        for (int i = 0; i < instCount; ++i) {
            char addressMode = readMARIE().value_or('x');
            
            if (addressMode == 'x') {
                // EOF
                return;
            }
            
            int operand = readInt();
        }

        for (auto [sym, val] : symbols_defined_in_module) {
            if (val > instCount) {
                // RULE 5
                // If an address appearing in a first definition of a symbol 
                // exceeds the size of the module, print a warning message 
                // in pass1 after processing the module and 
                // treat the address given as 0 (relative to the module).

                symbol_to_addr[sym] = module_base;

                std::cout << "Warning: Module " << module_num <<": " 
                          << sym << "=" << val << " valid=[0.." << instCount << "] assume zero relative\n";
            }
        }

        module_to_base[module_num] = module_base;
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

    for (auto [sym, addr] : symbol_to_addr) {
        std::cout << sym << "=" << addr;

        auto it = errors.find(sym);
        
        if (it != errors.end()) {
            std::string err_msg = it->second;
            std::cout << " " << err_msg; 
        }

        std::cout << "\n";
    }
}

void printInst(int inst_num, int opcode, int operand, std::string error_msg)
{
    std::string opcode_s = std::to_string(opcode);
    std::string operand_s = std::to_string(operand);

    std::cout << std::setfill('0') << std::setw(3) << std::to_string(inst_num) << ": "
              << std::to_string(opcode)
              << std::setfill('0') << std::setw(3) << std::to_string(operand) 
              << " " << error_msg << "\n";
}

void processMARIE(
    int module_num, 
    int module_size,
    const std::vector<std::string>& symbols_in_use_list, 
    std::unordered_set<int>& symbols_used_in_module, 
    int inst_num, 
    char addressMode, 
    int inst
)
{
    int opcode = inst / 1000;
    int operand = inst % 10;
    std::string error_msg = "";
    
    if (opcode >= 10) {
        // RULE 11
        // If an illegal opcode is encountered (i.e. op >= 10), 
        // print an error and convert the <opcode,operand> to 9999.
        opcode = 9;
        operand = 999;
        error_msg = "Error: Illegal opcode; treated as 9999";
    }

    switch (addressMode) {
        case 'M': {
            // Module addressing
            // Operand is the number of a valid module 
            // and is replaced with the base address of that module.

            auto it = module_to_base.find(operand);

            if (it == module_to_base.end()) {
                // RULE 12
                // If a module operand is invalid, print an error message and assume module 0.
                operand = 0;

                if (error_msg == "") {
                    error_msg = "Error: Illegal module operand ; treated as module=0";
                }
            }
            
            operand = module_to_base[operand];
            printInst(inst_num, opcode, operand, "");
            break;
        }
        case 'A': {
            // Absolute addressing
            // Operand is an absolute address which will never be changed in pass2; 
            // however it can’t be “>=” the machine size (512).
            if (operand > MACHINE_SIZE) {
                // RULE 8
                // If an absolute address exceeds the size of the machine, 
                // print an error message and use the absolute value zero.
                operand = 0;

                if (error_msg == "") {
                    error_msg = "Error: Absolute address exceeds machine size; zero used";
                }
            }

            printInst(inst_num, opcode, operand, error_msg);
            break;
        }
        case 'R': {
            // Relative addressing
            // Replace the relative address with the absolute address,
            // where absolute_addr = module_base + relative_addr.
            if (operand >= module_size) {
                // RULE 9
                // If a relative address exceeds the size of the module, 
                // print an error message and use the module relative value zero.
                // You still need to remap “0” to the correct absolute address).
                operand = 0;

                if (error_msg == "") {
                    error_msg = "Error: Relative address exceeds module size; relative zero used";
                }
            }

            operand = module_to_base[module_num] + operand;
            printInst(inst_num, opcode, operand, error_msg);
            break;
        }
        case 'I': {
            // Immediate addressing
            // Immediate operand is unchanged, but must be less than 900.

            // RULE 10
            // If an illegal immediate operand (I) is encountered (i.e. >= 900), 
            // print an error and convert the operand value to 999.
            if (operand >= 900) {
                operand = 999;

                if (error_msg == "") {
                    error_msg = "Error: Illegal immediate operand; treated as 999";
                }
            }

            printInst(inst_num, opcode, operand, error_msg);
            break;
        }
        case 'E': {
            if (operand >= (int) symbols_in_use_list.size()) {
                // RULE 6
                // If an external operand is too large to reference an entry in the use list, 
                // print an error message and treat the operand as relative=0.
                operand = module_to_base[module_num];

                if (error_msg == "") {
                    error_msg = "Error: External operand exceeds length of uselist; treated as relative=0";
                }
            }
            else {
                std::string sym = symbols_in_use_list[operand];

                symbols_used_in_module.insert(operand); // RULE 7

                auto it = symbol_to_addr.find(sym);

                if (it == symbol_to_addr.end()) {
                    // RULE 3
                    // If a symbol is used in an E-instruction but not defined anywhere, 
                    // print an error message and use the value absolute zero.
                    operand = 0;

                    if (error_msg == "") {
                        error_msg = "Error: " + sym + " is not defined; zero used";
                    }
                }
                else {
                    operand = it->second;
                    symbols_used_global.insert(sym); // RULE 4
                }
            }

            printInst(inst_num, opcode, operand, error_msg);
            break;
        }
        default:
            break;
    }
}

void pass2()
{
    std::cout << "Memory Map\n";

    int module_num = 0;
    int inst_num = 0;

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
        std::vector<std::string> symbols_in_use_list;

        for (int i = 0; i < useCount; ++i) {
            std::string sym = readSym().value_or("EOF");

            if (sym == "EOF") {
                return;
            }

            symbols_in_use_list.push_back(sym);
        }

        int instCount = readInt();

        // Indices of symbols used by an E instruction
        // within this module.
        // We need this information for RULE 7 to find out
        // which symbols in the use list weren't actually used.
        std::unordered_set<int> symbols_used_in_module;

        for (int i = 0; i < instCount; ++i) {
            char addressMode = readMARIE().value_or('F');
            
            if (addressMode == 'F') {
                // F here means EOF
                return;
            }
            
            int inst = readInt();

            processMARIE(
                module_num,
                instCount, 
                symbols_in_use_list, 
                symbols_used_in_module, 
                inst_num, 
                addressMode, 
                inst
            );
            
            ++inst_num;
        }

        for (int i = 0; i < (int) symbols_in_use_list.size(); ++i) {
            // RULE 7
            auto it = symbols_used_in_module.find(i);

            if (it == symbols_used_in_module.end()) {
                std::cout << "Warning: Module " << module_num 
                          << ": uselist[" << i << "]=" 
                          << symbols_in_use_list[i] << "was not used\n";
            }
        }

        ++module_num;
    }

    // RULE 4
    // If a symbol is defined but never used globally by an E instruction, 
    // print a warning message.
    for (auto [sym, addr] : symbol_to_addr) {
        auto it = symbols_used_global.find(sym);

        if (it == symbols_used_global.end()) {
            std::cout << "Warning: Module " 
                      << symbol_to_module[sym] << ":" << sym
                      << " was defined but never used\n";
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

    fclose(fp);

    fp = fopen(filename, "r");

    if (fp == nullptr) {
        std::cout << "Failed to open file." << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "\n";
    pass2();
    
    fclose(fp);

    return EXIT_SUCCESS;
}