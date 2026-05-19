#pragma once

#include "_IRule.h"

#include <filesystem>
#include <set>
#include <string>
#include <vector>

// Правило: запрещает сравнение переменных типа REAL через операторы '=' и '<>'.
//
// Почему это отдельное правило, а не ForbiddenPatternRule:
// простой поиск символа '=' дал бы слишком много ложных срабатываний.
// Нам нужно понять, что в сравнении участвует именно переменная типа REAL.
// Поэтому правило работает в два этапа:
// 1. Сначала собирает имена переменных, объявленных как REAL в VAR ... END_VAR.
// 2. Потом ищет операторы '=' и '<>' и проверяет операнды рядом с ними.
//
// Пример, который будет найден:
//
// VAR
//     rValue : REAL;
// END_VAR
//
// IF rValue = 0.0 THEN
// END_IF;
//
// Пример, который не должен быть найден:
//
// IF intValue = 10 THEN
// END_IF;
class RealComparisonRule : public IRule {
public:
    std::string name() const override;
    std::string description() const override;
    int checkFile(const std::filesystem::path& file) const override;

private:
    struct LineInfo {
        size_t number = 0;
        std::string original;
        std::string codeOnly;
    };

private:
    // Удаляет комментарии Structured Text из одной строки, сохраняя длину строки.
    // Сохранение длины важно, чтобы колонка ошибки совпадала с исходным файлом.
    static std::string removeStructuredTextCommentsFromLine(
        const std::string& line,
        bool& insideBlockComment,
        bool& insideBraceComment
    );

    // Переводит строку в нижний регистр для регистронезависимого анализа.
    static std::string toLower(std::string s);

    // Символ является частью имени переменной/токена.
    static bool isTokenChar(char c);

    // Символ может быть началом имени переменной.
    static bool isIdentifierStart(char c);

    // Символ может быть продолжением имени переменной.
    static bool isIdentifierChar(char c);

    // Достаёт идентификатор слева от позиции оператора.
    // Например: "IF rValue = 0 THEN" и position указывает на '=' -> rValue.
    static std::string readIdentifierBefore(const std::string& line, size_t position);

    // Достаёт идентификатор справа от позиции оператора.
    // Например: "IF 0 = rValue THEN" и position указывает после '=' -> rValue.
    static std::string readIdentifierAfter(const std::string& line, size_t position);

    // Разбивает строку с объявлением вида "a, b : REAL;" на имена a и b.
    static std::vector<std::string> splitDeclaredNames(const std::string& leftPart);

    // Собирает все переменные типа REAL, объявленные внутри VAR ... END_VAR.
    static std::set<std::string> collectRealVariables(const std::vector<LineInfo>& lines);

    // Печатает ошибку в стиле ST-компилятора:
    // 04.05.2026 19:15:40  main.st:21.1-7 error message
    static void printCompilerStyleError(
        const std::filesystem::path& file,
        size_t lineNumber,
        size_t columnStart,
        size_t columnEnd,
        const std::string& message
    );
};
