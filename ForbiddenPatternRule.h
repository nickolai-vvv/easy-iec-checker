#pragma once

#include "_IRule.h"

#include <filesystem>
#include <string>
#include <vector>

// Описание одного запрещённого паттерна.
//
// Почему это отдельная структура, а не отдельный класс под каждое правило:
// многие методики отличаются только словом, которое нужно найти.
// Например REAL_TO_INT, REAL_TO_DINT, TODO, FIXME, WHILE — это один и тот же
// алгоритм поиска, но разные настройки.
//
// Поэтому мы храним настройки в ForbiddenPattern, а общий алгоритм —
// в классе ForbiddenPatternRule.
struct ForbiddenPattern {
    // Короткое техническое имя методики.
    // Используется для понимания, какая именно проверка сработала.
    std::string name;

    // Текст, который нужно искать в исходнике.
    // Поиск выполняется без учёта регистра:
    // REAL_TO_INT, real_to_int и Real_To_Int будут найдены одинаково.
    std::string text;

    // Сообщение, которое будет напечатано пользователю при ошибке.
    std::string message;

    // true  — искать только как отдельное слово/токен.
    //         Например, WHILE найдётся в строке "WHILE x DO",
    //         но не найдётся внутри "MY_WHILE_FLAG".
    //
    // false — искать как обычную подстроку.
    //         Такой режим нужен редко, но оставлен на будущее.
    bool wholeWord = true;

    // true  — искать в исходной строке целиком, включая комментарии.
    //         Это удобно для TODO/FIXME/HACK, потому что они обычно
    //         как раз находятся в комментариях.
    //
    // false — перед поиском удалить комментарии Structured Text.
    //         Так запрещённые конструкции не будут срабатывать на примерах
    //         или пояснениях в комментариях.
    bool includeComments = false;
};

// Универсальное правило для запрета простых текстовых паттернов.
//
// Этим одним классом можно закрывать много методик вида:
// - запрещаем вызов REAL_TO_INT(...);
// - запрещаем WHILE;
// - запрещаем EXIT;
// - запрещаем TODO/FIXME/HACK.
//
// Чтобы добавить новую такую методику, обычно НЕ нужно создавать новый .h/.cpp.
// Достаточно добавить ещё один элемент ForbiddenPattern в StaticAnalyzer::registerRules().
class ForbiddenPatternRule : public IRule {
public:
    // Принимаем список паттернов снаружи, чтобы класс был универсальным.
    // Один объект ForbiddenPatternRule может проверять сразу много методик.
    explicit ForbiddenPatternRule(std::vector<ForbiddenPattern> patterns);

    // Имя общего правила.
    // Это имя группы, а не конкретной методики REAL_TO_INT или TODO.
    std::string name() const override;

    // Описание общего правила для логов/справки.
    std::string description() const override;

    // Проверяет один файл и возвращает количество найденных ошибок.
    int checkFile(const std::filesystem::path& file) const override;

private:
    // Все запрещённые паттерны, которые проверяет этот объект.
    std::vector<ForbiddenPattern> patterns_;

private:
    // Удаляет комментарии Structured Text из одной строки, сохраняя длину строки.
    //
    // Важно: функция заменяет символы комментариев пробелами, а не просто вырезает их.
    // Благодаря этому позиция найденной ошибки остаётся правильной:
    // номер колонки совпадает с колонкой в исходном файле.
    //
    // Structured Text поддерживает несколько видов комментариев:
    // - // однострочный комментарий;
    // - (* многострочный комментарий *);
    // - { комментарий в фигурных скобках }.
    //
    // Флаги insideBlockComment и insideBraceComment передаются по ссылке,
    // потому что многострочный комментарий может начаться на одной строке,
    // а закончиться на другой.
    static std::string removeStructuredTextCommentsFromLine(
        const std::string& line,
        bool& insideBlockComment,
        bool& insideBraceComment
    );

    // Переводит строку в нижний регистр.
    // Используется для поиска без учёта регистра.
    static std::string toLower(std::string s);

    // Проверяет, является ли символ частью токена.
    //
    // Токенами считаем буквы, цифры и подчёркивание.
    // Это нужно, чтобы WHILE не находился внутри SOME_WHILE_FLAG.
    static bool isTokenChar(char c);

    // Проверяет, что найденное совпадение является отдельным словом/токеном.
    //
    // Например:
    // text = "WHILE x DO", pos указывает на WHILE  -> true
    // text = "MY_WHILE_FLAG", pos указывает на WHILE -> false
    static bool isWholeWordMatch(
        const std::string& text,
        size_t pos,
        size_t len
    );

    // Печатает ошибку в стиле компилятора:
    //
    // file.st:10:5: error: forbidden conversion REAL_TO_INT
    //     x := REAL_TO_INT(y);
    //          ^
    //
    // Такой формат удобно читать и он часто кликается в IDE/терминалах.
    void printCompilerStyleError(
        const std::filesystem::path& file,
        size_t lineNumber,
        size_t column,
        const std::string& line,
        size_t position,
        const ForbiddenPattern& pattern
    ) const;
};
