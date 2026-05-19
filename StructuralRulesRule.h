#pragma once

#include "_IRule.h"

#include <filesystem>
#include <string>
#include <vector>
#include <set>

// Структурные правила для Structured Text.
//
// Этот класс собрал несколько проверок, которым нужен контекст вокруг строки.
// Их неудобно реализовывать через ForbiddenPatternRule, потому что простой
// поиск слова не отвечает на вопросы вроде:
// - находится ли EXIT именно внутри цикла;
// - пустой ли ELSE;
// - пустая ли ветка CASE;
// - стоит ли RETURN в середине логики.
//
// Сейчас класс проверяет шесть методик:
// 1. Блокируем EXIT внутри циклов.
// 2. Блокируем RETURN в середине логики.
// 3. Блокируем пустой ELSE.
// 4. Блокируем пустую ветку CASE.
// 5. Блокируем присваивание в IF, когда переменная задаётся только в одной ветке.
// 6. Блокируем магические числа в условиях IF / ELSIF.
class StructuralRulesRule : public IRule {
public:
    std::string name() const override;
    std::string description() const override;
    int checkFile(const std::filesystem::path& file) const override;

private:
    // Одна строка исходника в двух видах:
    // original — как в файле;
    // codeOnly — без комментариев, но с сохранёнными позициями колонок.
    struct LineInfo {
        size_t number = 0;
        std::string original;
        std::string codeOnly;
    };

    // Нормализованная строка для более простого анализа.
    // lowerTrimmed — строка в нижнем регистре без пробелов по краям.
    // firstColumn — колонка первого непробельного символа в исходной строке.
    struct NormalizedLine {
        size_t index = 0;       // индекс в массиве lines, начиная с 0
        size_t number = 0;      // реальный номер строки в файле, начиная с 1
        size_t firstColumn = 1; // колонка первого содержательного символа
        std::string original;
        std::string codeOnly;
        std::string lowerTrimmed;
    };

    // Информация о первом присваивании конкретной переменной внутри ветки IF.
    // Храним строку/колонку, чтобы потом вывести ошибку именно на месте присваивания.
    struct AssignmentInfo {
        std::string variable;
        size_t lineNumber = 0;
        size_t columnStart = 1;
        size_t columnEnd = 1;
    };

private:
    // Читает файл и сразу удаляет комментарии Structured Text.
    static std::vector<LineInfo> readLines(const std::filesystem::path& file);

    // Удаляет комментарии ST из одной строки, сохраняя длину строки.
    // Поддерживаются //, (* ... *) и { ... }.
    static std::string removeStructuredTextCommentsFromLine(
        const std::string& line,
        bool& insideBlockComment,
        bool& insideBraceComment
    );

    // Вспомогательные функции для строк и токенов.
    static std::string toLower(std::string s);
    static std::string trim(const std::string& s);
    static bool isTokenChar(char c);
    static bool containsWord(const std::string& lowerLine, const std::string& lowerWord);
    static size_t findWord(const std::string& lowerLine, const std::string& lowerWord);
    static std::vector<NormalizedLine> normalizeMeaningfulLines(const std::vector<LineInfo>& lines);

    // Проверяет, похожа ли строка внутри CASE на начало ветки:
    // 1:
    // 1, 2, 3:
    // SomeEnumValue:
    // TRUE:
    //
    // При этом строка "x := y;" не должна считаться веткой CASE,
    // потому что там перед = стоит ':'.
    static bool isCaseBranchLine(const std::string& lowerTrimmed);

    // Проверяет, является ли строка закрывающим оператором программы/функции.
    // Это нужно, чтобы разрешить RETURN в самом конце логики.
    static bool isFinalProgramTerminator(const std::string& lowerTrimmed);

    // Проверяет, начинается ли строка с управляющего слова.
    // Например, "if x then" начинается с IF, а "my_if_flag := true" — нет.
    static bool startsWithWord(const std::string& lowerTrimmed, const std::string& lowerWord);

    // Пытается разобрать простое присваивание вида
    //     variable := expression;
    // Если строка действительно похожа на присваивание переменной, возвращает true
    // и заполняет out. Сложные конструкции вроде вызовов функций слева не считаем.
    static bool tryParseAssignment(const NormalizedLine& line, AssignmentInfo& out);

    // Собирает переменные с начальными значениями из VAR-блоков.
    // Например, строка "x : INT = 0;" означает, что x уже задана до IF.
    static std::set<std::string> collectInitializedVariables(
        const std::vector<NormalizedLine>& lines
    );

    // Печатает ошибку в стиле ST-компилятора.
    static void printCompilerStyleError(
        const std::filesystem::path& file,
        size_t lineNumber,
        size_t columnStart,
        size_t columnEnd,
        const std::string& message
    );

private:
    // Отдельные проверки. Каждая возвращает количество найденных ошибок.
    static int checkExitInsideLoops(
        const std::filesystem::path& file,
        const std::vector<NormalizedLine>& lines
    );

    static int checkReturnInMiddle(
        const std::filesystem::path& file,
        const std::vector<NormalizedLine>& lines
    );

    static int checkEmptyElse(
        const std::filesystem::path& file,
        const std::vector<NormalizedLine>& lines
    );

    static int checkEmptyCaseBranch(
        const std::filesystem::path& file,
        const std::vector<NormalizedLine>& lines
    );

    static int checkAssignmentOnlyOneIfBranch(
        const std::filesystem::path& file,
        const std::vector<NormalizedLine>& lines,
        const std::set<std::string>& initializedVariables
    );

    static int checkMagicNumbersInIfConditions(
        const std::filesystem::path& file,
        const std::vector<NormalizedLine>& lines
    );
};
