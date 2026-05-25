#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "_IRule.h"

// Путь к make.mk по умолчанию.
// Используется, если пользователь не передал путь через аргументы командной строки.
#define DEFAULT_MAKEFILE_PATH "resource/_make/make.mk"

// Главный класс статического анализатора.
//
// Сейчас он:
// 1. Читает make.mk.
// 2. Достаёт из него список .sts-файлов.
// 3. Регистрирует набор правил анализа.
// 4. Открывает каждый найденный файл.
// 5. Запускает каждое правило для каждого файла.
// 6. Печатает общий итог.
//
// Важно:
// StaticAnalyzer не должен знать детали конкретных проверок.
// Он работает только с базовым интерфейсом IRule.
class StaticAnalyzer {
public:
    // Конструктор получает argc/argv из main().
    //
    // Если пользователь передал путь к make.mk, используем его.
    //
    // Поддерживаются варианты:
    // analyzer.exe resource/_make/make.mk
    // analyzer.exe --makefile resource/_make/make.mk
    // analyzer.exe -m resource/_make/make.mk
    //
    // Иначе используем DEFAULT_MAKEFILE_PATH.
    StaticAnalyzer(int argc, char** argv);

    // Главная точка запуска анализатора.
    //
    // Метод специально сделан как пайплайн:
    //
    // prepare()
    //   -> registerRules()
    //   -> collectSources()
    //   -> analyzeSources()
    //   -> finish()
    //
    // Возвращает:
    // 0 — ошибок нет;
    // 1 — найдены ошибки анализа;
    // 2 — критическая ошибка запуска.
    int run();

private:
    // Путь к make.mk.
    enum class RuleConfig {
        Sil3,
        Development
    };

    std::filesystem::path makefile_;
    RuleConfig ruleConfig_ = RuleConfig::Sil3;
    std::string argumentError_;

    // Список .sts-файлов, найденных в make.mk.
    //
    // Здесь хранятся пути ровно в том виде,
    // в каком они были извлечены из make.mk.
    std::vector<std::filesystem::path> sourceFiles_;

    // Список правил анализа.
    //
    // Храним указатели на базовый интерфейс IRule,
    // чтобы StaticAnalyzer мог одинаково запускать любые правила:
    // REAL_TO_INT, FOR, WHILE и т.д.
    std::vector<std::unique_ptr<IRule>> rules_;

    // Общее количество найденных ошибок.
    int totalErrors_ = 0;

private:
    // Первый этап пайплайна.
    //
    // Проверяет наличие make.mk, приводит путь к нормальному виду
    // и печатает базовую информацию о режиме работы.
    void prepare();

    // Регистрирует все правила анализа.
    //
    // Новые проверки добавляются здесь:
    // rules_.push_back(std::make_unique<NewRule>());
    void registerRules();
    void registerSil3Rules();
    void registerDevelopmentRules();

    // Второй этап пайплайна.
    //
    // Извлекает список .sts-файлов из make.mk.
    void collectSources();

    // Третий этап пайплайна.
    //
    // Проходит по найденным файлам и запускает все зарегистрированные правила.
    void analyzeSources();

    // Финальный этап пайплайна.
    //
    // Печатает итог и возвращает код завершения.
    int finish() const;

    // Обработка критических ошибок.
    //
    // Например:
    // - make.mk не найден;
    // - файл не удалось открыть;
    // - ошибка std::filesystem.
    int fail(const std::exception& e) const;

private:
    // Удаляет пробелы и табы по краям строки.
    static std::string trim(const std::string& s);

    // Проверяет, заканчивается ли строка на suffix без учёта регистра.
    //
    // Нужно, чтобы одинаково находить:
    // .st, .ST, .St.
    static bool endsWithCaseInsensitive(
        const std::string& s,
        const std::string& suffix
    );

    // Удаляет неактивную часть строки make.mk.
    //
    // Поддерживает комментарии через:
    // #
    // //
    //
    // Это нужно, чтобы закомментированные файлы не попадали
    // в список анализируемых файлов.
    static std::string removeMakeComment(const std::string& line);

    // Очищает токен из make.mk от лишних символов по краям.
    //
    // Например:
    // "main.st" -> main.st
    // main.st,  -> main.st
    // main.st:  -> main.st
    static std::string cleanToken(std::string token);
    static bool tryParseRuleConfig(
        const std::string& value,
        RuleConfig& config
    );

    // Читает make.mk и склеивает строки, которые заканчиваются на '\'.
    //
    // Make-файлы часто пишут так:
    //
    // FILES = <строка продолжается обратным слэшем>
    //     main.st <строка продолжается обратным слэшем>
    //     val.st
    //
    // Для анализатора это должна быть одна логическая строка.
    std::vector<std::string> readMakeLogicalLines() const;

    // Извлекает из make.mk список .sts-файлов.
    std::vector<std::filesystem::path> extractStsFilesFromMakefile() const;

    // Превращает путь из make.mk в реальный путь к файлу на диске.
    //
    // Например:
    // main.st
    //
    // превращается в:
    // resource/main.st
    std::filesystem::path resolveFilePath(
        const std::filesystem::path& fileFromMake
    ) const;
};
