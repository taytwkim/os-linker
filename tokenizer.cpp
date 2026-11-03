#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>

FILE *fp;
int linenum;
int curr_line_offset;
int end_of_line_offset;

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

int main(int argc, char *argv[])
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

    char *tok;

    while ((tok = getToken()) != nullptr) {
        std::cout << "token = " << std::left << std::setw(20) << tok
                  << "position = " << linenum << ":" << curr_line_offset << '\n';
    }

    std::cout << "EOF position " << linenum << ":" << curr_line_offset << std::endl;

    fclose(fp);

    return EXIT_SUCCESS;
}