#include "StaticAnalyzer.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv) {

#ifdef _WIN32
    SetConsoleOutputCP(1251);
    SetConsoleCP(1251);
#endif

    StaticAnalyzer analyzer(argc, argv);
    return analyzer.run();
}