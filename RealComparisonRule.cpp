#include "RealComparisonRule.h"
#include "DiagnosticOutput.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

std::string RealComparisonRule::name() const {
    return "REAL_COMPARISON";
}

std::string RealComparisonRule::description() const {
    return "Запрещает сравнение REAL-переменных через = и <>";
}

int RealComparisonRule::checkFile(const std::filesystem::path& file) const {
    std::ifstream in(file);

    if (!in) {
        std::cerr << file.string() << ": error: cannot open file\n";
        return 1;
    }

    // Сначала читаем файл целиком.
    // Для этого правила так проще: сначала надо увидеть объявления REAL,
    // а уже потом проверять сравнения ниже по файлу.
    std::vector<LineInfo> lines;
    std::string originalLine;
    size_t lineNumber = 0;

    bool insideBlockComment = false;
    bool insideBraceComment = false;

    while (std::getline(in, originalLine)) {
        ++lineNumber;

        LineInfo info;
        info.number = lineNumber;
        info.original = originalLine;
        info.codeOnly = removeStructuredTextCommentsFromLine(
            originalLine,
            insideBlockComment,
            insideBraceComment
        );

        lines.push_back(info);
    }

    // Все имена храним в нижнем регистре.
    // Тогда rValue, RVALUE и rvalue считаются одной переменной.
    const std::set<std::string> realVariables = collectRealVariables(lines);

    // Если REAL-переменных в файле нет, правило точно ничего не найдёт.
    if (realVariables.empty()) {
        return 0;
    }

    int errors = 0;

    for (const LineInfo& line : lines) {
        const std::string& code = line.codeOnly;

        for (size_t i = 0; i < code.size(); ++i) {
            // Проверка оператора '<>'.
            if (i + 1 < code.size() && code[i] == '<' && code[i + 1] == '>') {
                std::string left = toLower(readIdentifierBefore(code, i));
                std::string right = toLower(readIdentifierAfter(code, i + 2));

                if (realVariables.count(left) || realVariables.count(right)) {
                    printCompilerStyleError(
                        file,
                        line.number,
                        i + 1,
                        i + 2,
                        "forbidden REAL comparison through <>"
                    );
                    ++errors;
                }

                ++i; // пропускаем второй символ оператора '<>'
                continue;
            }

            // Проверка оператора '='.
            // В ST есть ':=' — это присваивание, его блокировать нельзя.
            // Также не трогаем '<=' и '>='.
            if (code[i] == '=') {
                const bool isAssignment = i > 0 && code[i - 1] == ':';
                const bool isLessOrEqual = i > 0 && code[i - 1] == '<';
                const bool isGreaterOrEqual = i > 0 && code[i - 1] == '>';

                if (isAssignment || isLessOrEqual || isGreaterOrEqual) {
                    continue;
                }

                std::string left = toLower(readIdentifierBefore(code, i));
                std::string right = toLower(readIdentifierAfter(code, i + 1));

                if (realVariables.count(left) || realVariables.count(right)) {
                    printCompilerStyleError(
                        file,
                        line.number,
                        i + 1,
                        i + 1,
                        "forbidden REAL comparison through ="
                    );
                    ++errors;
                }
            }
        }
    }

    return errors;
}

std::string RealComparisonRule::removeStructuredTextCommentsFromLine(
    const std::string& line,
    bool& insideBlockComment,
    bool& insideBraceComment
) {
    std::string result;
    result.reserve(line.size());

    size_t i = 0;

    while (i < line.size()) {
        if (insideBlockComment) {
            if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == ')') {
                insideBlockComment = false;
                result += "  ";
                i += 2;
            }
            else {
                result += ' ';
                ++i;
            }

            continue;
        }

        if (insideBraceComment) {
            if (line[i] == '}') {
                insideBraceComment = false;
            }

            result += ' ';
            ++i;
            continue;
        }

        if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '/') {
            result.append(line.size() - i, ' ');
            break;
        }

        if (i + 1 < line.size() && line[i] == '(' && line[i + 1] == '*') {
            insideBlockComment = true;
            result += "  ";
            i += 2;
            continue;
        }

        if (line[i] == '{') {
            insideBraceComment = true;
            result += ' ';
            ++i;
            continue;
        }

        result += line[i];
        ++i;
    }

    return result;
}

std::string RealComparisonRule::toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return s;
}

bool RealComparisonRule::isTokenChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool RealComparisonRule::isIdentifierStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool RealComparisonRule::isIdentifierChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

std::string RealComparisonRule::readIdentifierBefore(
    const std::string& line,
    size_t position
) {
    if (position == 0) {
        return {};
    }

    size_t end = position;

    // Пропускаем пробелы перед оператором.
    while (end > 0 && std::isspace(static_cast<unsigned char>(line[end - 1]))) {
        --end;
    }

    size_t start = end;

    // Идём влево по символам имени.
    while (start > 0 && isIdentifierChar(line[start - 1])) {
        --start;
    }

    if (start == end || !isIdentifierStart(line[start])) {
        return {};
    }

    return line.substr(start, end - start);
}

std::string RealComparisonRule::readIdentifierAfter(
    const std::string& line,
    size_t position
) {
    size_t start = position;

    // Пропускаем пробелы после оператора.
    while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
        ++start;
    }

    if (start >= line.size() || !isIdentifierStart(line[start])) {
        return {};
    }

    size_t end = start + 1;

    while (end < line.size() && isIdentifierChar(line[end])) {
        ++end;
    }

    return line.substr(start, end - start);
}

std::vector<std::string> RealComparisonRule::splitDeclaredNames(
    const std::string& leftPart
) {
    std::vector<std::string> result;
    std::string current;

    // В объявлении ST несколько переменных могут идти через запятую:
    // a, b, c : REAL;
    for (char c : leftPart) {
        if (c == ',') {
            current = toLower(current);

            // Убираем пробелы вокруг имени.
            size_t first = current.find_first_not_of(" \t\r\n");
            size_t last = current.find_last_not_of(" \t\r\n");

            if (first != std::string::npos) {
                std::string name = current.substr(first, last - first + 1);

                if (!name.empty()) {
                    result.push_back(name);
                }
            }

            current.clear();
        }
        else {
            current += c;
        }
    }

    current = toLower(current);
    size_t first = current.find_first_not_of(" \t\r\n");
    size_t last = current.find_last_not_of(" \t\r\n");

    if (first != std::string::npos) {
        std::string name = current.substr(first, last - first + 1);

        if (!name.empty()) {
            result.push_back(name);
        }
    }

    return result;
}

std::set<std::string> RealComparisonRule::collectRealVariables(
    const std::vector<LineInfo>& lines
) {
    std::set<std::string> result;
    bool insideVarBlock = false;

    for (const LineInfo& line : lines) {
        std::string lower = toLower(line.codeOnly);

        // Начало блока объявлений.
        // Ищем именно токен VAR, чтобы не перепутать с именем переменной.
        size_t varPos = lower.find("var");
        if (varPos != std::string::npos) {
            const bool leftOk = varPos == 0 || !isTokenChar(lower[varPos - 1]);
            const bool rightOk = varPos + 3 >= lower.size() || !isTokenChar(lower[varPos + 3]);

            if (leftOk && rightOk) {
                insideVarBlock = true;
            }
        }

        if (!insideVarBlock) {
            continue;
        }

        // Конец блока объявлений.
        size_t endVarPos = lower.find("end_var");
        if (endVarPos != std::string::npos) {
            const bool leftOk = endVarPos == 0 || !isTokenChar(lower[endVarPos - 1]);
            const bool rightOk = endVarPos + 7 >= lower.size() || !isTokenChar(lower[endVarPos + 7]);

            if (leftOk && rightOk) {
                insideVarBlock = false;
                continue;
            }
        }

        // Нас интересует простое объявление вида:
        // rValue : REAL;
        // a, b   : REAL := 0.0;
        //
        // Сложные случаи ARRAY/STRUCT специально не разбираем,
        // чтобы не усложнять правило и не плодить ложные срабатывания.
        size_t colon = lower.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        std::string left = lower.substr(0, colon);
        std::string right = lower.substr(colon + 1);

        // Проверяем, что после ':' тип именно REAL как отдельный токен.
        // LREAL здесь не считается REAL, потому что пользовательская методика
        // сформулирована именно как "сравнение REAL".
        size_t realPos = right.find("real");
        if (realPos == std::string::npos) {
            continue;
        }

        const bool leftOk = realPos == 0 || !isTokenChar(right[realPos - 1]);
        const bool rightOk = realPos + 4 >= right.size() || !isTokenChar(right[realPos + 4]);

        if (!leftOk || !rightOk) {
            continue;
        }

        std::vector<std::string> names = splitDeclaredNames(left);

        for (const std::string& name : names) {
            if (!name.empty()) {
                result.insert(name);
            }
        }
    }

    return result;
}

void RealComparisonRule::printCompilerStyleError(
    const std::filesystem::path& file,
    size_t lineNumber,
    size_t columnStart,
    size_t columnEnd,
    const std::string& message
) {
    DiagnosticOutput::printCompilerStyleError(
        file,
        lineNumber,
        columnStart,
        columnEnd,
        message
    );
}
