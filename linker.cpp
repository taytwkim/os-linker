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
 * The input file lists: 
 * 1. modules, 
 * 2. symbols defined or used in each module, and
 * 3. instructions in each module.
 *
 * The output file shows how symbols have been
 * resolved.
 *
 * In the first pass, we build a symbol table and 
 * determine the base address of each module based on its size. 
 * 
 * In the second pass, we rewrite some of the instructions, 
 * relocating relative addresses and resolving external 
 * symbol references. See `notes.md` for more details.
 * 
 * get_token() and readX are helper functions used in
 * both passes to consume and process one token at a time 
 * from the file stream.
 */

#define MAX_DEF_IN_MODULE 16
#define MAX_USE_IN_MODULE 16
#define MACHINE_SIZE 512
#define MAX_VALID_INT (1 << 30)
#define MAX_SYM_LEN 16

/* ========== MODULES AND SYMBOLS ========== */

// Map module # -> module base, updated after first pass.
std::map<int, int> module_to_base;

std::map<std::string, int> symbol_to_module;

// Map symbol -> absolute address, updated after first pass.
std::map<std::string, int> symbol_to_addr;

// Map symbol -> error messages from first pass.
// Error messages are stored because they have to be printed after the first pass.
std::map<std::string, std::string> first_pass_errors;

// Symbols referenced by E instructions (RULE 4), updated during second pass.
std::unordered_set<std::string> symbols_used_global;

/* ========== FILE STREAM STATE ========== */

FILE *fp;
int linenum;
int curr_line_offset;
int end_of_line_offset; // Helpful in finding the EOF offset.

/* ========== TOKENIZER ========== */

// Exit during first pass if input has invalid syntax.
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

    std::cout << "Parse Error line " << linenum << " offset " << curr_line_offset 
              << ": " << errmsg[errcode] << "\n";
    exit(1);
}

/*
 * Read and return one token at a time from a file stream.
 * 
 * Token delimiters are space, tab, and new line.
 * 
 * File state is maintained as a global fp variable.
 * 
 * Return nullptr if we reached EOF and there is no token to read.
 * 
 * linenum and curr_line_offset are updated after each call 
 * to the position of the first character of the token.
 * If we are at EOF, these are set to one past the position of the 
 * last character in the file (except for when the file ended with a \n).
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
        // we have to find and return the next valid line and token
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

// Read and return one address mode (M, A, R, I, E).
// Return nullopt if we reached EOF.
std::optional<char> readMARIE()
{
    char *tok = getToken();

    if (tok == nullptr) {
        return std::nullopt;
    }

    std::unordered_set<char> MARIE = {'M', 'A', 'R', 'I', 'E'};

    if (strlen(tok) != 1 || MARIE.find(*tok) == MARIE.end()) {
        parse_error(5);
    }

    return *tok;
}

/* ========== FIRST PASS ========== */

// Make first pass over the input file.
// Builds module_base_table and symbol table
// needed in the second pass.
// The first pass also checks for syntax errors.
// Warnings are printed during the pass,
// errors are printed next to the symbol in the symbol table.
void pass1()
{
    int module_num = 0;
    int module_base = 0;
    int total_inst_count = 0;

    // One iteration corresponds to one module.
    while (true) {
        std::vector<std::pair<std::string, int>> symbols_defined_in_module;

        int defcount = readInt();

        if (defcount < 0) {
            // Valid EOF, no more modules to process.
            return;
        }

        if (defcount > MAX_DEF_IN_MODULE) {
            parse_error(0); // TOO_MANY_DEF_IN_MODULE
        }

        for (int i = 0; i < defcount; ++i) {
            std::string sym = readSym().value_or("EOF");

            if (sym == "EOF") {
                parse_error(4); // SYM_EXPECTED
            }

            int val = readInt();

            if (val < 0) {
                parse_error(3); // NUM_EXPECTED
            }
            
            if (symbol_to_addr.find(sym) != symbol_to_addr.end()) {
                // RULE 2
                // If a symbol is defined multiple times,  
                // print an error message and use the first definition.  
                // The error message is to appear as part of printing the symbol table 
                // (following symbol=value printout on the same line).
                std::string msg = "Error: This variable is multiple times defined; first value used";
                first_pass_errors[sym] = msg;

                // RULE 5
                // If the symbol is a redefinition then print a warning message.
                std::cout << "Warning: Module " << module_num << ": " << sym << " redefinition ignored\n";
            }
            else {
                symbol_to_module[sym] = module_num;
                symbol_to_addr[sym] = module_base + val;
                symbols_defined_in_module.push_back({sym, val});
            }
        }

        int useCount = readInt();

        if (useCount < 0) {
            parse_error(3); // NUM_EXPECTED
        }

        if (useCount > MAX_USE_IN_MODULE) {
            parse_error(1); // TOO_MANY_USE_IN_MODULE
        }

        for (int i = 0; i < useCount; ++i) {
            std::string sym = readSym().value_or("EOF");

            if (sym == "EOF") {
                parse_error(4); // SYM_EXPECTED
            }
        }

        int instCount = readInt();
        
        if (instCount < 0) {
            parse_error(3); // NUM_EXPECTED
        }
        
        total_inst_count += instCount;

        if (total_inst_count > MACHINE_SIZE) {
            parse_error(2); // TOO_MANY_INSTR
        }

        for (int i = 0; i < instCount; ++i) {
            char addressMode = readMARIE().value_or('x');
            
            if (addressMode == 'x') {
                parse_error(5); // MARIE_EXPECTED
            }
            
            int operand = readInt();

            if (operand < 0) {
                parse_error(3); // NUM_EXPECTED
            }
        }

        for (auto [sym, val] : symbols_defined_in_module) {
            if (val > instCount) {
                // RULE 5
                // If an address appearing in a first definition of a symbol 
                // exceeds the size of the module, print a warning message 
                // in pass1 after processing the module and treat the address 
                // given as 0 (relative to the module).
                symbol_to_addr[sym] = module_base;

                std::cout << "Warning: Module " << module_num <<": " 
                          << sym << "=" << val << " valid=[0.." << instCount-1 << "] assume zero relative\n";
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

        auto it = first_pass_errors.find(sym);
        
        if (it != first_pass_errors.end()) {
            std::string err_msg = it->second;
            std::cout << " " << err_msg; 
        }

        std::cout << "\n";
    }
}

/* ========== SECOND PASS ========== */

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
    std::unordered_set<int>& symbols_actually_used_in_module, 
    int inst_num, 
    char addressMode, 
    int inst
)
{
    int opcode = inst / 1000;
    int operand = inst % 1000;
    std::string error_msg = "";
    
    if (opcode >= 10) {
        // RULE 11
        // If an illegal opcode is encountered (i.e. op >= 10), 
        // print an error and convert the <opcode,operand> to 9999.
        opcode = 9;
        operand = 999;
        error_msg = "Error: Illegal opcode; treated as 9999";
        printInst(inst_num, opcode, operand, error_msg);
        return;
    }

    switch (addressMode) {
        case 'M': {
            // Module addressing
            // Operand is the number of a valid module 
            // and is replaced with the base address of that module.
            if (module_to_base.find(operand) == module_to_base.end()) {
                // RULE 12
                // If a module operand is invalid, print an error message and assume module 0.
                operand = 0;
                error_msg = "Error: Illegal module operand ; treated as module=0";
            }
            
            operand = module_to_base[operand];
            printInst(inst_num, opcode, operand, error_msg);
            break;
        }
        case 'A': {
            // Absolute addressing
            // Operand is an absolute address which will never be changed in pass2; 
            // however it can’t be “>=” the machine size (512).
            if (operand >= MACHINE_SIZE) {
                // RULE 8
                // If an absolute address exceeds the size of the machine, 
                // print an error message and use the absolute value zero.
                operand = 0;
                error_msg = "Error: Absolute address exceeds machine size; zero used";
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
                error_msg = "Error: Relative address exceeds module size; relative zero used";
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
                error_msg = "Error: Illegal immediate operand; treated as 999";
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
                error_msg = "Error: External operand exceeds length of uselist; treated as relative=0";
            }
            else {
                std::string sym = symbols_in_use_list[operand];

                symbols_actually_used_in_module.insert(operand); // RULE 7

                auto it = symbol_to_addr.find(sym);

                if (it == symbol_to_addr.end()) {
                    // RULE 3
                    // If a symbol is used in an E-instruction but not defined anywhere, 
                    // print an error message and use the value absolute zero.
                    operand = 0;
                    error_msg = "Error: " + sym + " is not defined; zero used";
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
            // We are at valid EOF.

            // RULE 4
            // If a symbol is defined but never used globally by an E instruction, 
            // print a warning message.
            std::cout << "\n";
            
            for (auto [sym, addr] : symbol_to_addr) {
                auto it = symbols_used_global.find(sym);

                if (it == symbols_used_global.end()) {
                    std::cout << "Warning: Module " 
                            << symbol_to_module[sym] << ": " << sym
                            << " was defined but never used\n";
                }
            }

            return;
        }

        for (int i = 0; i < defcount; ++i) {
            std::string sym = readSym().value_or("EOF");
            int val = readInt();
        }

        int useCount = readInt();
        std::vector<std::string> symbols_in_use_list;

        for (int i = 0; i < useCount; ++i) {
            std::string sym = readSym().value_or("EOF");
            symbols_in_use_list.push_back(sym);
        }

        int instCount = readInt();

        // Indices of symbols used by an E instruction
        // within this module.
        // We need this information for RULE 7 to find out
        // which symbols in the use list weren't actually used.
        std::unordered_set<int> symbols_actually_used_in_module;

        for (int i = 0; i < instCount; ++i) {
            char addressMode = readMARIE().value_or('x');
            int inst = readInt();

            processMARIE(
                module_num,
                instCount, 
                symbols_in_use_list, 
                symbols_actually_used_in_module, 
                inst_num, 
                addressMode, 
                inst
            );
            
            ++inst_num;
        }

        for (int i = 0; i < (int) symbols_in_use_list.size(); ++i) {
            // RULE 7
            // If a symbol appears in a use list but is not actually used in the module 
            // (i.e., not referred to in any E-type address),
            // print a warning message and continue. 
            // If the same unused symbol appears multiple times in uselist, multiple
            // warnings are to be printed.
            if (symbols_actually_used_in_module.find(i) == symbols_actually_used_in_module.end()) {
                std::cout << "Warning: Module " << module_num 
                          << ": uselist[" << i << "]=" 
                          << symbols_in_use_list[i] << " was not used\n";
            }
        }

        ++module_num;
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