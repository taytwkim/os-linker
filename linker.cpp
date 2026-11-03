#include <cstdlib>
#include <iostream>
#include <optional>
#include <string.h>
#include <string>
#include <unordered_map>

FILE *fp;
int linenum;
int curr_line_offset;
int end_of_line_offset;

// Map module # to module base
std::unordered_map<int, int> module_base_table;

// Map symbol to absolute address
std::unordered_map<std::string, int> absolute_addr_table;

char *getToken()
{
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

    pch = std::strtok(nullptr, delim);

    if (pch == nullptr) {
        return getToken();
    }

    curr_line_offset = pch - line + 1;

    return pch;
}

int readInt()
{
    char *tok = getToken();

    if (tok == nullptr) {
        // EOF
        return -1;
    }

    return std::stoi(tok);
}

std::optional<std::string> readSym()
{
    char *tok = getToken();

    if (tok == nullptr) {
        return std::nullopt;
    }

    std::string s = tok;
    return s;
}

std::optional<char> readMARIE()
{
    char *tok = getToken();

    if (tok == nullptr) {
        // EOF
        return std::nullopt;
    }

    return *tok;
}

void pass1()
{
    int module_num = 0;
    int module_base = 0;

    while (true) {
        int defcount = readInt();

        if (defcount < 0) {
            exit(2);
        }

        for (int i = 0; i < defcount; ++i) {
            std::string sym = readSym().value_or("EOF");

            if (sym == "EOF") {
                exit(2);
            }

            int val = readInt();
            
            absolute_addr_table[sym] = module_base + val;
        }

        int useCount = readInt();

        for (int i = 0; i < useCount; ++i) {
            std::string sym = readSym().value_or("EOF");

            if (sym == "EOF") {
                exit(2);
            }
        }

        int instCount = readInt();

        for (int i = 0; i < instCount; ++i) {
            char addressMode = readMARIE().value_or('F');
            
            if (addressMode == 'F') {
                exit(2);
            }
            
            int operand = readInt();
        }

        module_base_table[module_num] = module_base;
        ++module_num;
        module_base += instCount;
    }
}

void print_pass1_results()
{
    std::cout << "Hello World!" << "\n";
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
    print_pass1_results();

    return EXIT_SUCCESS;
}