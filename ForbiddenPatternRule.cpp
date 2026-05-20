#include "ForbiddenPatternRule.h"
#include "DiagnosticOutput.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <utility>

// Конструктор просто сохраняет список настроек.
// std::move нужен, чтобы не копировать весь vector лишний раз.
ForbiddenPatternRule::ForbiddenPatternRule(std::vector<ForbiddenPattern> patterns)
    : patterns_(std::move(patterns))
{
}

std::string ForbiddenPatternRule::name() const {
    return "FORBIDDEN_PATTERNS";
}

std::string ForbiddenPatternRule::description() const {
    return "Запрещает набор опасных конструкций по настраиваемому списку";
}

// Основной метод правила.
//
// Алгоритм простой:
// 1. Открываем файл.
// 2. Читаем его построчно.
// 3. Для каждой строки готовим две версии:
//    - originalLine: как в файле, вместе с комментариями;
//    - codeOnlyLine: та же строка, но комментарии заменены пробелами.
// 4. Для каждого паттерна выбираем, где искать:
//    - includeComments == true  -> originalLine;
//    - includeComments == false -> codeOnlyLine.
// 5. Печатаем все найденные ошибки.
int ForbiddenPatternRule::checkFile(const std::filesystem::path& file) const {
    std::ifstream in(file);

    if (!in) {
        std::cerr << file.string() << ": error: cannot open file\n";
        return 1;
    }

    int errors = 0;
    size_t lineNumber = 0;
    std::string originalLine;

    // Состояние многострочных комментариев.
    // Эти флаги нельзя объявлять внутри while, иначе анализатор забудет,
    // что предыдущая строка открыла комментарий (* ... или { ... .
    bool insideBlockComment = false;
    bool insideBraceComment = false;

    while (std::getline(in, originalLine)) {
        ++lineNumber;

        // Версия строки без комментариев Structured Text.
        // Длина строки сохраняется, чтобы позиции ошибок не съезжали.
        std::string codeOnlyLine = removeStructuredTextCommentsFromLine(
            originalLine,
            insideBlockComment,
            insideBraceComment
        );

        // Один проход по строке проверяет все запреты из patterns_.
        for (const ForbiddenPattern& pattern : patterns_) {
            // Для TODO/FIXME/HACK обычно includeComments == true.
            // Для REAL_TO_INT/WHILE/EXIT — false, чтобы комментарии не шумели.
            const std::string& searchLine =
                pattern.includeComments ? originalLine : codeOnlyLine;

            // Делаем поиск регистронезависимым.
            // Это полезно для ST, где стиль написания может отличаться.
            std::string haystack = toLower(searchLine);
            std::string needle = toLower(pattern.text);

            size_t pos = haystack.find(needle);

            // find() ищет только первое совпадение, поэтому идём циклом
            // и находим все совпадения в строке.
            while (pos != std::string::npos) {
                // Если wholeWord == true, дополнительно проверяем границы токена.
                // Иначе WHILE сработал бы внутри MY_WHILE_FLAG.
                bool ok = !pattern.wholeWord ||
                    isWholeWordMatch(haystack, pos, needle.size());

                if (ok) {
                    printCompilerStyleError(
                        file,
                        lineNumber,
                        pos + 1,       // пользователю колонки удобнее считать с 1
                        originalLine,  // печатаем настоящую строку, не очищенную
                        pos,           // а caret ставим в найденную позицию
                        pattern
                    );

                    ++errors;
                }

                // Сдвигаемся на 1 символ, чтобы найти следующее совпадение.
                // Это позволяет не пропускать соседние/пересекающиеся случаи.
                pos = haystack.find(needle, pos + 1);
            }
        }
    }

    return errors;
}

std::string ForbiddenPatternRule::removeStructuredTextCommentsFromLine(
    const std::string& line,
    bool& insideBlockComment,
    bool& insideBraceComment
) {
    std::string result;
    result.reserve(line.size());

    size_t i = 0;

    while (i < line.size()) {
        // Мы уже внутри комментария вида (* ... *).
        if (insideBlockComment) {
            // Конец многострочного комментария.
            if (i + 1 < line.size() && line[i] == '*' && line[i + 1] == ')') {
                insideBlockComment = false;

                // Добавляем два пробела вместо двух символов "*)".
                result += "  ";
                i += 2;
            }
            else {
                // Любой символ внутри комментария превращаем в пробел.
                result += ' ';
                ++i;
            }

            continue;
        }

        // Мы уже внутри комментария вида { ... }.
        if (insideBraceComment) {
            if (line[i] == '}') {
                insideBraceComment = false;
            }

            result += ' ';
            ++i;
            continue;
        }

        // Однострочный комментарий //.
        // Всё до конца строки заменяем пробелами и завершаем обработку строки.
        if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '/') {
            result.append(line.size() - i, ' ');
            break;
        }

        // Начало комментария вида (* ... *).
        if (i + 1 < line.size() && line[i] == '(' && line[i + 1] == '*') {
            insideBlockComment = true;
            result += "  ";
            i += 2;
            continue;
        }

        // Начало комментария вида { ... }.
        if (line[i] == '{') {
            insideBraceComment = true;
            result += ' ';
            ++i;
            continue;
        }

        // Обычный код переносим без изменений.
        result += line[i];
        ++i;
    }

    return result;
}

std::string ForbiddenPatternRule::toLower(std::string s) {
    for (char& c : s) {
        // static_cast<unsigned char> нужен для корректной работы std::tolower
        // с символами, у которых char может быть отрицательным.
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return s;
}

bool ForbiddenPatternRule::isTokenChar(char c) {
    unsigned char uc = static_cast<unsigned char>(c);

    // В токен входят буквы, цифры и подчёркивание.
    // Так обычно устроены имена переменных/функций.
    return std::isalnum(uc) || c == '_';
}

bool ForbiddenPatternRule::isWholeWordMatch(
    const std::string& text,
    size_t pos,
    size_t len
) {
    // Слева от совпадения либо начало строки, либо не-токенный символ.
    bool leftOk = pos == 0 || !isTokenChar(text[pos - 1]);

    // Справа от совпадения либо конец строки, либо не-токенный символ.
    bool rightOk = pos + len >= text.size() || !isTokenChar(text[pos + len]);

    return leftOk && rightOk;
}

void ForbiddenPatternRule::printCompilerStyleError(
    const std::filesystem::path& file,
    size_t lineNumber,
    size_t column,
    const std::string& line,
    size_t position,
    const ForbiddenPattern& pattern
) const {
    (void)line;
    (void)position;

    // Печатаем ошибку в стиле вашего ST-компилятора.
    //
    // Пример такого формата:
    // 04.05.2026 19:15:40  main.st:21.1-7 error syntax error, unexpected END_VAR, expecting :
    //
    // Формат состоит из:
    // - текущей даты и времени;
    // - имени файла;
    // - номера строки;
    // - диапазона колонок внутри строки;
    // - слова error;
    // - текста ошибки.
    //
    // Для наших правил диапазон колонок строится по длине запрещённого паттерна.
    // Например REAL_TO_INT начинается в колонке 10 и имеет длину 11 символов:
    // main.st:5.10-20 error forbidden conversion REAL_TO_INT

    const size_t matchLength = pattern.text.empty() ? 1 : pattern.text.size();
    const size_t columnEnd = column + matchLength - 1;

    DiagnosticOutput::printCompilerStyleError(
        file,
        lineNumber,
        column,
        columnEnd,
        pattern.message
    );
}
