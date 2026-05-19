#include "StructuralRulesRule.h"

#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>

std::string StructuralRulesRule::name() const {
    return "STRUCTURAL_RULES";
}

std::string StructuralRulesRule::description() const {
    return "Проверяет структурные запреты: EXIT, RETURN, пустые ветки, IF-присваивания и магические числа";
}

int StructuralRulesRule::checkFile(const std::filesystem::path& file) const {
    std::vector<LineInfo> rawLines = readLines(file);
    std::vector<NormalizedLine> lines = normalizeMeaningfulLines(rawLines);

    // Переменные, которые уже имеют начальное значение в VAR-блоке.
    // Для них присваивание только в одной ветке IF не является проблемой:
    // если условие не выполнится, переменная всё равно не останется "не заданной".
    const std::set<std::string> initializedVariables = collectInitializedVariables(lines);

    int errors = 0;

    errors += checkExitInsideLoops(file, lines);
    errors += checkReturnInMiddle(file, lines);
    errors += checkEmptyElse(file, lines);
    errors += checkEmptyCaseBranch(file, lines);
    errors += checkAssignmentOnlyOneIfBranch(file, lines, initializedVariables);
    errors += checkMagicNumbersInIfConditions(file, lines);

    return errors;
}

std::vector<StructuralRulesRule::LineInfo> StructuralRulesRule::readLines(
    const std::filesystem::path& file
) {
    std::ifstream in(file);

    if (!in) {
        std::cerr << file.string() << ": error: cannot open file\n";
        return {};
    }

    std::vector<LineInfo> result;
    std::string originalLine;
    size_t lineNumber = 0;

    // Состояние многострочных комментариев между строками.
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

        result.push_back(info);
    }

    return result;
}

std::string StructuralRulesRule::removeStructuredTextCommentsFromLine(
    const std::string& line,
    bool& insideBlockComment,
    bool& insideBraceComment
) {
    std::string result;
    result.reserve(line.size());

    size_t i = 0;

    while (i < line.size()) {
        // Уже находимся внутри комментария вида (* ... *).
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

        // Уже находимся внутри комментария вида { ... }.
        if (insideBraceComment) {
            if (line[i] == '}') {
                insideBraceComment = false;
            }

            result += ' ';
            ++i;
            continue;
        }

        // Однострочный комментарий //.
        if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '/') {
            result.append(line.size() - i, ' ');
            break;
        }

        // Начало многострочного комментария (* ... *).
        if (i + 1 < line.size() && line[i] == '(' && line[i + 1] == '*') {
            insideBlockComment = true;
            result += "  ";
            i += 2;
            continue;
        }

        // Начало комментария { ... }.
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

std::string StructuralRulesRule::toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return s;
}

std::string StructuralRulesRule::trim(const std::string& s) {
    size_t first = 0;

    while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first]))) {
        ++first;
    }

    size_t last = s.size();

    while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1]))) {
        --last;
    }

    return s.substr(first, last - first);
}

bool StructuralRulesRule::isTokenChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

size_t StructuralRulesRule::findWord(
    const std::string& lowerLine,
    const std::string& lowerWord
) {
    size_t pos = lowerLine.find(lowerWord);

    while (pos != std::string::npos) {
        const bool leftOk = pos == 0 || !isTokenChar(lowerLine[pos - 1]);
        const bool rightOk = pos + lowerWord.size() >= lowerLine.size() ||
            !isTokenChar(lowerLine[pos + lowerWord.size()]);

        if (leftOk && rightOk) {
            return pos;
        }

        pos = lowerLine.find(lowerWord, pos + 1);
    }

    return std::string::npos;
}

bool StructuralRulesRule::containsWord(
    const std::string& lowerLine,
    const std::string& lowerWord
) {
    return findWord(lowerLine, lowerWord) != std::string::npos;
}

std::vector<StructuralRulesRule::NormalizedLine> StructuralRulesRule::normalizeMeaningfulLines(
    const std::vector<LineInfo>& lines
) {
    std::vector<NormalizedLine> result;

    for (size_t i = 0; i < lines.size(); ++i) {
        const LineInfo& line = lines[i];
        const std::string trimmed = trim(line.codeOnly);

        // Пустые строки и строки, которые стали пустыми после удаления комментариев,
        // не участвуют в структурном анализе.
        if (trimmed.empty()) {
            continue;
        }

        NormalizedLine out;
        out.index = i;
        out.number = line.number;
        out.original = line.original;
        out.codeOnly = line.codeOnly;
        out.lowerTrimmed = toLower(trimmed);

        const size_t first = line.codeOnly.find_first_not_of(" \t\r\n");
        out.firstColumn = first == std::string::npos ? 1 : first + 1;

        result.push_back(out);
    }

    return result;
}

bool StructuralRulesRule::isCaseBranchLine(const std::string& lowerTrimmed) {
    const size_t colon = lowerTrimmed.find(':');

    if (colon == std::string::npos) {
        return false;
    }

    // ':=' — это присваивание, а не ветка CASE.
    if (colon + 1 < lowerTrimmed.size() && lowerTrimmed[colon + 1] == '=') {
        return false;
    }

    // Метки CASE идут до двоеточия. После двоеточия может сразу идти код,
    // например "1: x := 10;" — такая ветка не пустая.
    const std::string beforeColon = trim(lowerTrimmed.substr(0, colon));

    if (beforeColon.empty()) {
        return false;
    }

    // Отсекаем служебные строки вроде "program x:" или "var_input".
    // Внутри CASE обычно перед ':' стоят значения, перечисления или диапазоны.
    if (beforeColon == "if" || beforeColon == "case" || beforeColon == "for" ||
        beforeColon == "while" || beforeColon == "repeat" || beforeColon == "program" ||
        beforeColon == "function" || beforeColon == "function_block") {
        return false;
    }

    return true;
}

bool StructuralRulesRule::isFinalProgramTerminator(const std::string& lowerTrimmed) {
    return containsWord(lowerTrimmed, "end_program") ||
        containsWord(lowerTrimmed, "end_function") ||
        containsWord(lowerTrimmed, "end_function_block");
}


bool StructuralRulesRule::startsWithWord(
    const std::string& lowerTrimmed,
    const std::string& lowerWord
) {
    if (lowerTrimmed.size() < lowerWord.size()) {
        return false;
    }

    if (lowerTrimmed.compare(0, lowerWord.size(), lowerWord) != 0) {
        return false;
    }

    return lowerTrimmed.size() == lowerWord.size() ||
        !isTokenChar(lowerTrimmed[lowerWord.size()]);
}

bool StructuralRulesRule::tryParseAssignment(
    const NormalizedLine& line,
    AssignmentInfo& out
) {
    const size_t assignPos = line.lowerTrimmed.find(":=");

    if (assignPos == std::string::npos) {
        return false;
    }

    std::string left = trim(line.lowerTrimmed.substr(0, assignPos));

    if (left.empty()) {
        return false;
    }

    // Берём только простые имена переменных слева от :=.
    // Для первого варианта правила это лучше, чем пытаться разобрать весь ST.
    // Примеры, которые считаем простыми:
    //   x := 1;
    //   motor_state := TRUE;
    //   fb.value := 10;
    // Примеры, которые не считаем простыми:
    //   arr[i] := 1;
    //   SomeCall(x) := 1;
    for (char c : left) {
        const bool ok = std::isalnum(static_cast<unsigned char>(c)) ||
            c == '_' || c == '.';

        if (!ok) {
            return false;
        }
    }

    // Колонка указывает на начало имени переменной.
    // Так как lowerTrimmed начинается с firstColumn, ищем left внутри lowerTrimmed.
    const size_t leftPos = line.lowerTrimmed.find(left);
    if (leftPos == std::string::npos) {
        return false;
    }

    out.variable = left;
    out.lineNumber = line.number;
    out.columnStart = line.firstColumn + leftPos;
    out.columnEnd = out.columnStart + left.size() - 1;

    return true;
}

std::set<std::string> StructuralRulesRule::collectInitializedVariables(
    const std::vector<NormalizedLine>& lines
) {
    std::set<std::string> result;
    bool insideVarBlock = false;

    for (const NormalizedLine& line : lines) {
        // В ST переменные обычно объявляются между VAR и END_VAR.
        // Нас интересуют только объявления, а не обычный код программы.
        if (startsWithWord(line.lowerTrimmed, "var") ||
            startsWithWord(line.lowerTrimmed, "var_input") ||
            startsWithWord(line.lowerTrimmed, "var_output") ||
            startsWithWord(line.lowerTrimmed, "var_in_out") ||
            startsWithWord(line.lowerTrimmed, "var_temp") ||
            startsWithWord(line.lowerTrimmed, "var_global")) {
            insideVarBlock = true;
            continue;
        }

        if (startsWithWord(line.lowerTrimmed, "end_var")) {
            insideVarBlock = false;
            continue;
        }

        if (!insideVarBlock) {
            continue;
        }

        // Ищем простое объявление вида:
        //     x : INT = 0;
        //     y : INT := 0;
        //     flag : BOOL := FALSE;
        // Если после типа есть '=' или ':=', считаем переменную инициализированной.
        const size_t colon = line.lowerTrimmed.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        std::string variable = trim(line.lowerTrimmed.substr(0, colon));
        if (variable.empty()) {
            continue;
        }

        // Берём только простые имена. Групповые объявления вроде
        // "x, y : INT := 0;" пока специально не разбираем, чтобы не делать
        // ложных допущений. Их можно добавить позже отдельной маленькой доработкой.
        bool simpleName = true;
        for (char c : variable) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') {
                simpleName = false;
                break;
            }
        }

        if (!simpleName) {
            continue;
        }

        const std::string afterColon = line.lowerTrimmed.substr(colon + 1);
        if (afterColon.find(":=") != std::string::npos ||
            afterColon.find('=') != std::string::npos) {
            result.insert(variable);
        }
    }

    return result;
}


void StructuralRulesRule::printCompilerStyleError(
    const std::filesystem::path& file,
    size_t lineNumber,
    size_t columnStart,
    size_t columnEnd,
    const std::string& message
) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};

#if defined(_WIN32)
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif

    std::ostringstream timestamp;
    timestamp << std::put_time(&localTime, "%d.%m.%Y %H:%M:%S");

    std::cerr << timestamp.str()
        << "  "
        << file.filename().string()
        << ":"
        << lineNumber
        << "."
        << columnStart
        << "-"
        << columnEnd
        << " : error C9001: "
        << message
        << "\n";
}

int StructuralRulesRule::checkExitInsideLoops(
    const std::filesystem::path& file,
    const std::vector<NormalizedLine>& lines
) {
    int errors = 0;
    int loopDepth = 0;

    for (const NormalizedLine& line : lines) {
        // Сначала проверяем EXIT на текущей глубине.
        // Если мы внутри WHILE/REPEAT, EXIT запрещён.
        const size_t exitPos = findWord(line.lowerTrimmed, "exit");
        if (exitPos != std::string::npos && loopDepth > 0) {
            const size_t column = line.firstColumn + exitPos;
            printCompilerStyleError(
                file,
                line.number,
                column,
                column + 3,
                "forbidden EXIT inside loop"
            );
            ++errors;
        }

        // Обновляем глубину циклов после проверки строки.
        // Так EXIT на той же строке, что и WHILE, не считается ещё находящимся внутри цикла,
        // но обычный стиль ST всё равно пишет тело цикла на следующих строках.
        if (containsWord(line.lowerTrimmed, "while")) {
            ++loopDepth;
        }

        if (containsWord(line.lowerTrimmed, "repeat")) {
            ++loopDepth;
        }

        if (containsWord(line.lowerTrimmed, "end_while") ||
            containsWord(line.lowerTrimmed, "until") ||
            containsWord(line.lowerTrimmed, "end_repeat")) {
            if (loopDepth > 0) {
                --loopDepth;
            }
        }
    }

    return errors;
}

int StructuralRulesRule::checkReturnInMiddle(
    const std::filesystem::path& file,
    const std::vector<NormalizedLine>& lines
) {
    int errors = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        const NormalizedLine& line = lines[i];
        const size_t returnPos = findWord(line.lowerTrimmed, "return");

        if (returnPos == std::string::npos) {
            continue;
        }

        // Ищем следующую содержательную строку после RETURN.
        // Если дальше сразу END_PROGRAM/END_FUNCTION/END_FUNCTION_BLOCK,
        // считаем RETURN финальным и не ругаемся.
        // Во всех остальных случаях RETURN прерывает логику посередине.
        bool hasCodeAfterReturn = false;

        for (size_t j = i + 1; j < lines.size(); ++j) {
            if (isFinalProgramTerminator(lines[j].lowerTrimmed)) {
                break;
            }

            hasCodeAfterReturn = true;
            break;
        }

        if (hasCodeAfterReturn) {
            const size_t column = line.firstColumn + returnPos;
            printCompilerStyleError(
                file,
                line.number,
                column,
                column + 5,
                "forbidden RETURN in the middle of logic"
            );
            ++errors;
        }
    }

    return errors;
}

int StructuralRulesRule::checkEmptyElse(
    const std::filesystem::path& file,
    const std::vector<NormalizedLine>& lines
) {
    int errors = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        const NormalizedLine& line = lines[i];
        const size_t elsePos = findWord(line.lowerTrimmed, "else");

        if (elsePos == std::string::npos) {
            continue;
        }

        // Если после ELSE на той же строке есть код, ветка точно не пустая.
        const size_t afterElse = elsePos + 4;
        if (afterElse < line.lowerTrimmed.size() && !trim(line.lowerTrimmed.substr(afterElse)).empty()) {
            continue;
        }

        // Пустой ELSE — это когда следующая содержательная строка сразу закрывает блок.
        // Для IF это END_IF, для CASE это END_CASE.
        if (i + 1 < lines.size() &&
            (containsWord(lines[i + 1].lowerTrimmed, "end_if") ||
             containsWord(lines[i + 1].lowerTrimmed, "end_case"))) {
            const size_t column = line.firstColumn + elsePos;
            printCompilerStyleError(
                file,
                line.number,
                column,
                column + 3,
                "forbidden empty ELSE branch"
            );
            ++errors;
        }
    }

    return errors;
}

int StructuralRulesRule::checkEmptyCaseBranch(
    const std::filesystem::path& file,
    const std::vector<NormalizedLine>& lines
) {
    int errors = 0;
    int caseDepth = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        const NormalizedLine& line = lines[i];

        if (containsWord(line.lowerTrimmed, "case")) {
            ++caseDepth;
            continue;
        }

        if (containsWord(line.lowerTrimmed, "end_case")) {
            if (caseDepth > 0) {
                --caseDepth;
            }
            continue;
        }

        if (caseDepth <= 0 || !isCaseBranchLine(line.lowerTrimmed)) {
            continue;
        }

        const size_t colon = line.lowerTrimmed.find(':');
        const std::string afterColon = colon == std::string::npos
            ? std::string{}
            : trim(line.lowerTrimmed.substr(colon + 1));

        // Если после двоеточия есть код на той же строке, ветка не пустая.
        if (!afterColon.empty()) {
            continue;
        }

        // Ветка CASE пустая, если следующая содержательная строка — это:
        // - новая ветка CASE;
        // - ELSE;
        // - END_CASE.
        if (i + 1 < lines.size() &&
            (isCaseBranchLine(lines[i + 1].lowerTrimmed) ||
             containsWord(lines[i + 1].lowerTrimmed, "else") ||
             containsWord(lines[i + 1].lowerTrimmed, "end_case"))) {
            const size_t columnStart = line.firstColumn;
            const size_t columnEnd = line.firstColumn + colon;

            printCompilerStyleError(
                file,
                line.number,
                columnStart,
                columnEnd,
                "forbidden empty CASE branch"
            );
            ++errors;
        }
    }

    return errors;
}

int StructuralRulesRule::checkAssignmentOnlyOneIfBranch(
    const std::filesystem::path& file,
    const std::vector<NormalizedLine>& lines,
    const std::set<std::string>& initializedVariables
) {
    struct IfFrame {
        bool hasElse = false;

        // Переменные, которые уже были точно заданы ДО входа в этот IF.
        // Если такая переменная присваивается только в THEN-ветке, это нормально:
        // при невыполнении условия у неё всё равно уже есть значение.
        std::set<std::string> assignedBeforeIf;

        // Для каждой ветки IF / ELSIF / ELSE храним переменные,
        // которые присваиваются внутри этой ветки.
        std::vector<std::map<std::string, AssignmentInfo>> branches;
    };

    int errors = 0;
    std::vector<IfFrame> stack;

    // Глубина CASE нужна именно для правила про "переменная уже была задана до IF".
    // БАГ, который был раньше:
    //     CASE mode OF
    //         1:
    //             z := 100;
    //         2:
    //         3:
    //             z := 300;
    //     ELSE
    //         z := 0;
    //     END_CASE;
    //
    // Старый код видел "z := 100" и ошибочно считал z точно заданной до следующего IF.
    // Но присваивание внутри CASE условное: оно выполнится только для конкретной ветки.
    // Поэтому присваивания внутри CASE нельзя добавлять в definitelyAssigned как обычные.
    int caseDepth = 0;

    // Переменные, которые точно получили значение в обычной линейной логике
    // до текущего места. Сюда входят:
    // - переменные с инициализатором в VAR-блоке: x : INT := 0;
    // - простые присваивания вне IF и вне CASE: x := 0;
    //
    // Важный пример, который НЕ должен считаться ошибкой:
    //     x := 0;
    //     IF flag THEN
    //         x := 10;
    //     END_IF;
    //
    // Если flag = FALSE, x всё равно останется равным 0, то есть переменная
    // не останется неопределённой.
    std::set<std::string> definitelyAssigned = initializedVariables;

    for (const NormalizedLine& line : lines) {
        // Обновляем состояние CASE до обработки присваиваний на строке.
        // Ветка вида "1:" сама по себе не должна ничего менять.
        if (startsWithWord(line.lowerTrimmed, "case")) {
            ++caseDepth;
            continue;
        }

        if (startsWithWord(line.lowerTrimmed, "end_case")) {
            if (caseDepth > 0) {
                --caseDepth;
            }
            continue;
        }

        if (startsWithWord(line.lowerTrimmed, "if")) {
            IfFrame frame;
            frame.assignedBeforeIf = definitelyAssigned;
            frame.branches.push_back({});
            stack.push_back(frame);
            continue;
        }

        if (startsWithWord(line.lowerTrimmed, "elsif")) {
            if (!stack.empty()) {
                stack.back().branches.push_back({});
            }
            continue;
        }

        if (startsWithWord(line.lowerTrimmed, "else")) {
            if (!stack.empty()) {
                stack.back().hasElse = true;
                stack.back().branches.push_back({});
            }
            continue;
        }

        if (startsWithWord(line.lowerTrimmed, "end_if")) {
            if (stack.empty()) {
                continue;
            }

            IfFrame frame = stack.back();
            stack.pop_back();

            // Методика: запрещаем ситуацию, когда у IF нет ELSE,
            // и переменная задаётся только в одной ветке IF/ELSIF.
            // Но НЕ ругаемся, если переменная уже была задана до IF.
            if (!frame.hasElse) {
                std::map<std::string, AssignmentInfo> firstAssignment;
                std::map<std::string, size_t> assignmentBranchCount;

                for (const auto& branch : frame.branches) {
                    for (const auto& item : branch) {
                        const std::string& variable = item.first;

                        if (firstAssignment.find(variable) == firstAssignment.end()) {
                            firstAssignment[variable] = item.second;
                        }

                        ++assignmentBranchCount[variable];
                    }
                }

                for (const auto& item : assignmentBranchCount) {
                    const std::string& variable = item.first;
                    const size_t count = item.second;

                    if (count == 1) {
                        // Если переменная уже была задана до IF, то отсутствие ELSE
                        // не опасно: при ложном условии сохранится предыдущее значение.
                        if (frame.assignedBeforeIf.find(variable) != frame.assignedBeforeIf.end()) {
                            continue;
                        }

                        const AssignmentInfo& where = firstAssignment[variable];
                        printCompilerStyleError(
                            file,
                            where.lineNumber,
                            where.columnStart,
                            where.columnEnd,
                            "forbidden assignment without ELSE: variable is assigned only in one IF branch"
                        );
                        ++errors;
                    }
                }
            }

            // Если IF имеет ELSE и переменная задаётся во всех ветках, то после END_IF
            // она считается точно заданной. Но только если сам IF не находится внутри CASE.
            // Иначе это всё равно условное присваивание по ветке CASE.
            if (caseDepth == 0 && frame.hasElse && !frame.branches.empty()) {
                std::set<std::string> assignedInAllBranches;
                bool firstBranch = true;

                for (const auto& branch : frame.branches) {
                    std::set<std::string> branchVariables;
                    for (const auto& item : branch) {
                        branchVariables.insert(item.first);
                    }

                    if (firstBranch) {
                        assignedInAllBranches = branchVariables;
                        firstBranch = false;
                    }
                    else {
                        std::set<std::string> intersection;
                        for (const std::string& variable : assignedInAllBranches) {
                            if (branchVariables.find(variable) != branchVariables.end()) {
                                intersection.insert(variable);
                            }
                        }
                        assignedInAllBranches = intersection;
                    }
                }

                for (const std::string& variable : assignedInAllBranches) {
                    definitelyAssigned.insert(variable);
                }
            }

            continue;
        }

        AssignmentInfo assignment;
        if (tryParseAssignment(line, assignment)) {
            if (!stack.empty() && !stack.back().branches.empty()) {
                // Присваивание внутри IF условное, поэтому не добавляем его сразу
                // в definitelyAssigned. Сначала нужно дождаться END_IF и понять,
                // есть ли ELSE и присвоена ли переменная во всех ветках.
                stack.back().branches.back().insert({ assignment.variable, assignment });
            }
            else if (caseDepth == 0) {
                // Простое присваивание вне IF и вне CASE считается безопасной
                // инициализацией для последующих IF-блоков.
                definitelyAssigned.insert(assignment.variable);
            }
            else {
                // Присваивание внутри CASE НЕ является точно выполненным.
                // Например, "z := 100" в ветке "1:" не гарантирует, что z будет
                // задана при mode = 2 или mode = 3.
            }
        }
    }

    return errors;
}

int StructuralRulesRule::checkMagicNumbersInIfConditions(
    const std::filesystem::path& file,
    const std::vector<NormalizedLine>& lines
) {
    int errors = 0;

    for (const NormalizedLine& line : lines) {
        bool isIfLike = startsWithWord(line.lowerTrimmed, "if");
        size_t keywordLength = 2;

        if (!isIfLike && startsWithWord(line.lowerTrimmed, "elsif")) {
            isIfLike = true;
            keywordLength = 5;
        }

        if (!isIfLike) {
            continue;
        }

        const size_t thenPos = findWord(line.lowerTrimmed, "then");
        const size_t conditionStart = keywordLength;
        const size_t conditionEnd = thenPos == std::string::npos
            ? line.lowerTrimmed.size()
            : thenPos;

        if (conditionStart >= conditionEnd) {
            continue;
        }

        const std::string condition = line.lowerTrimmed.substr(
            conditionStart,
            conditionEnd - conditionStart
        );

        size_t i = 0;
        while (i < condition.size()) {
            if (!std::isdigit(static_cast<unsigned char>(condition[i]))) {
                ++i;
                continue;
            }

            const size_t numberStart = i;

            while (i < condition.size() && std::isdigit(static_cast<unsigned char>(condition[i]))) {
                ++i;
            }

            if (i < condition.size() && condition[i] == '.') {
                ++i;
                while (i < condition.size() && std::isdigit(static_cast<unsigned char>(condition[i]))) {
                    ++i;
                }
            }

            const size_t numberEnd = i;

            // Не считаем числом часть имени переменной вроде sensor1 или value_2.
            const bool leftIsName = numberStart > 0 && isTokenChar(condition[numberStart - 1]);
            const bool rightIsName = numberEnd < condition.size() && isTokenChar(condition[numberEnd]);

            if (leftIsName || rightIsName) {
                continue;
            }

            const std::string numberText = condition.substr(numberStart, numberEnd - numberStart);

            // 0 и 1 часто используются как очевидные граничные значения.
            // Если нужно запрещать вообще все числа, можно удалить этот if.
            if (numberText == "0" || numberText == "1" ||
                numberText == "0.0" || numberText == "1.0") {
                continue;
            }

            const size_t columnStart = line.firstColumn + conditionStart + numberStart;
            const size_t columnEnd = columnStart + numberText.size() - 1;

            printCompilerStyleError(
                file,
                line.number,
                columnStart,
                columnEnd,
                "forbidden magic number in IF condition"
            );
            ++errors;
        }
    }

    return errors;
}
