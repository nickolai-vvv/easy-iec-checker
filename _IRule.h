#pragma once

#include <filesystem>
#include <string>

// Базовый интерфейс для всех правил статического анализатора.
//
// Идея такая:
// StaticAnalyzer не должен знать про конкретные проверки вроде REAL_TO_INT,
// FOR, WHILE и т.д. Он должен знать только то, что у любого правила есть
// общий набор методов: имя, описание и проверка одного файла.
class IRule {
public:
    // Виртуальный деструктор обязателен для базового класса,
    // если мы будем удалять дочерние правила через указатель IRule*.
    virtual ~IRule() = default;

    // Короткое имя правила.
    //
    // Например:
    // REAL_TO_INT
    // FOR_RULE
    // MAGIC_NUMBER
    virtual std::string name() const = 0;

    // Человекочитаемое описание правила.
    //
    // Позже это можно использовать для режима --list-rules,
    // справки или подробного логирования.
    virtual std::string description() const = 0;

    // Проверяет один файл.
    //
    // Возвращает количество найденных ошибок.
    virtual int checkFile(const std::filesystem::path& file) const = 0;
};
