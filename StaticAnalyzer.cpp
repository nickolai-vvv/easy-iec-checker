#include "StaticAnalyzer.h"
#include "DiagnosticOutput.h"
#include "ProjectAnalysis.h"
#include "SafetyRules.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

// Конструктор выбирает путь к make.mk.
//
// Поддерживаются два варианта запуска:
//
//     analyzer.exe C:/project/resource/_make/make.mk
//     analyzer.exe --makefile C:/project/resource/_make/make.mk
//
// Если путь не передали, используется DEFAULT_MAKEFILE_PATH.
StaticAnalyzer::StaticAnalyzer(int argc, char** argv) {
    makefile_ = DEFAULT_MAKEFILE_PATH;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if ((arg == "--makefile" || arg == "-m") && i + 1 < argc) {
            makefile_ = argv[i + 1];
            ++i;
        }
        else if (!arg.empty() && arg[0] != '-') {
            makefile_ = arg;
        }
    }
}

// Главный сценарий работы анализатора.
//
// Здесь намеренно нет деталей.
// Метод читается как пайплайн обработки проекта.
int StaticAnalyzer::run() {
    try {
        prepare();
        registerRules();
        collectSources();
        analyzeSources();
        return finish();
    }
    catch (const std::exception& e) {
        return fail(e);
    }
}

// Первый этап пайплайна.
//
// Проверяем make.mk и подготавливаем общий контекст.
void StaticAnalyzer::prepare() {
    if (!argumentError_.empty()) {
        throw std::runtime_error(argumentError_);
    }

    if (!std::filesystem::exists(makefile_)) {
        throw std::runtime_error(
            "makefile not found: " + makefile_.string()
        );
    }

    // Приводим путь к нормальному виду.
    //
    // Например:
    // resource/_make/make.mk
    //
    // может стать:
    // C:/Users/.../resource/_make/make.mk
    makefile_ = std::filesystem::weakly_canonical(makefile_);

}

// Регистрирует все правила анализа.
//
// Здесь единственное место, где StaticAnalyzer знает о конкретных классах правил.
// Дальше он работает только через интерфейс IRule.
void StaticAnalyzer::registerRules() {
    rules_.clear();
    rules_.push_back(std::make_unique<ArrayBoundsRule>());
    rules_.push_back(std::make_unique<ImplicitConversionRule>());
    rules_.push_back(std::make_unique<RealComparisonRule>());
    rules_.push_back(std::make_unique<UninitializedVariableRule>());
    rules_.push_back(std::make_unique<IntegerDivisionRule>());
}

#if 0 // Устаревшие наборы правил. В проекте используются только пять правил выше.
void StaticAnalyzer::registerSil3Rules() {

    // Универсальное правило для простых запретов по текстовому паттерну.
    // Здесь оставляем только те методики, которым не нужен контекст.
    // EXIT убран из этого списка специально: теперь он проверяется точнее
    // в StructuralRulesRule и запрещается только внутри циклов.
    rules_.push_back(std::make_unique<ForbiddenPatternRule>(
        std::vector<ForbiddenPattern>{
                // Запрещенные логические операции
			{ "XOR", "XOR", "forbidden logical operator XOR", true, false},
            //{ "OR" ,  "OR", "forbidden logical operator OR" , true, false },
            //{ "AND", "AND", "forbidden logical operator AND", true, false },
            { "NOT", "NOT", "forbidden logical operator NOT", true, false },

                // Запрещённые преобразования типов.
            { "REAL_TO_INT", "REAL_TO_INT", "forbidden conversion REAL_TO_INT", true, false},
            { "REAL_TO_DINT",  "REAL_TO_DINT",  "forbidden conversion REAL_TO_DINT",  true, false },
            { "LREAL_TO_INT",  "LREAL_TO_INT",  "forbidden conversion LREAL_TO_INT",  true, false },
            { "LREAL_TO_DINT", "LREAL_TO_DINT", "forbidden conversion LREAL_TO_DINT", true, false },
            { "REAL_TO_WORD",  "REAL_TO_WORD",  "forbidden conversion REAL_TO_WORD",  true, false },
            { "REAL_TO_DWORD", "REAL_TO_DWORD", "forbidden conversion REAL_TO_DWORD", true, false },
            { "DINT_TO_INT",   "DINT_TO_INT",   "forbidden conversion DINT_TO_INT",   true, false },
            { "DWORD_TO_INT",  "DWORD_TO_INT",  "forbidden conversion DWORD_TO_INT",  true, false },
            { "WORD_TO_INT",   "WORD_TO_INT",   "forbidden conversion WORD_TO_INT",   true, false },

                // Запрещённые конструкции языка, которые можно находить простым поиском.
            { "WHILE",  "WHILE",  "forbidden WHILE loop",  true, false },
            { "REPEAT", "REPEAT", "forbidden REPEAT loop", true, false },

                // Неструктурный поток управления.
            { "GOTO",     "GOTO",     "forbidden GOTO",     true, false },
            { "LABEL",    "LABEL",    "forbidden LABEL",    true, false },
            { "CONTINUE", "CONTINUE", "forbidden CONTINUE", true, false },

                // Адресация, указатели и нетипизированные интерфейсы.
            { "POINTER", "POINTER", "forbidden POINTER",                true, false },
            { "ADR",     "ADR",     "forbidden ADR",                    true, false },
            { "AT",      "AT",      "forbidden AT absolute addressing", true, false },
            { "ANY",     "ANY",     "forbidden ANY untyped parameter",  true, false },

                // Устаревшие PLC-типы и операции, нежелательные для SIL-кода.
            { "S5TIME",  "S5TIME",  "forbidden S5TIME legacy timer type", true, false },
            { "TIMER",   "TIMER",   "forbidden TIMER legacy type",        true, false },
            { "COUNTER", "COUNTER", "forbidden COUNTER legacy type",      true, false },

                // Технический долг ищем даже в комментариях.
            { "TODO",  "TODO",  "forbidden TODO",  true, true },
            { "FIXME", "FIXME", "forbidden FIXME", true, true },
            { "HACK",  "HACK",  "forbidden HACK",  true, true }
    }
    ));

    // Блокируем сравнение REAL-переменных через '=' и '<>'.
    rules_.push_back(std::make_unique<RealComparisonRule>());

    // Структурные правила:
    // - EXIT внутри циклов;
    // - RETURN в середине логики;
    // - пустой ELSE;
    // - пустая ветка CASE.
    rules_.push_back(std::make_unique<StructuralRulesRule>());
}

void StaticAnalyzer::registerDevelopmentRules() {
    rules_.push_back(std::make_unique<RealComparisonRule>());
    rules_.push_back(std::make_unique<DevelopmentRulesRule>());
}
#endif

// Второй этап пайплайна.
//
// Получаем список исходников из make.mk.
void StaticAnalyzer::collectSources() {
    sourceFiles_ = extractStsFilesFromMakefile();


    if (sourceFiles_.empty()) {
        std::cerr << makefile_.string()
            << ": warning: no .st files found in makefile\n";
    }
}

// Третий этап пайплайна.
//
// Запускаем все зарегистрированные правила по каждому найденному файлу.
void StaticAnalyzer::analyzeSources() {
    totalErrors_ = 0;

    if (rules_.empty()) {
        std::cerr << "easy-iec-checker: warning: no rules registered\n";
        return;
    }

    std::vector<std::filesystem::path> resolvedFiles;
    for (const std::filesystem::path& fileFromMake : sourceFiles_) {
        // make.mk может содержать относительные пути.
        // Поэтому сначала ищем реальный файл на диске.
        std::filesystem::path resolved = resolveFilePath(fileFromMake);

        if (resolved.empty()) {
            std::cerr << makefile_.string()
                << ": warning: file listed in makefile not found: "
                << fileFromMake.string()
                << "\n";

            continue;
        }

        // Главное архитектурное место:
        // StaticAnalyzer не знает, какое именно правило запускается.
        // Он просто проходит по массиву IRule и вызывает общий метод checkFile().
        DiagnosticOutput::setDisplayPath(resolved, fileFromMake);
        resolvedFiles.push_back(resolved);
    }

    const ProjectAnalysis project = ProjectAnalysis::load(resolvedFiles);
    for (const std::unique_ptr<IRule>& rule : rules_) {
        totalErrors_ += rule->check(project);
    }
}

// Финальный этап пайплайна.
//
// Печатает итог и возвращает код завершения.
int StaticAnalyzer::finish() const {
    if (totalErrors_ > 0) {
        std::cerr << "easy-iec-checker: found "
            << totalErrors_
            << " error(s)\n";

        return 1;
    }

    return 0;
}

// Обработка критических ошибок пайплайна.
int StaticAnalyzer::fail(const std::exception& e) const {
    std::cerr << "fatal error: " << e.what() << "\n";
    return 2;
}

// Удаляет пробелы по краям строки.
std::string StaticAnalyzer::trim(const std::string& s) {
    size_t start = 0;

    while (start < s.size() &&
        std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }

    size_t end = s.size();

    while (end > start &&
        std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }

    return s.substr(start, end - start);
}

// Проверяет окончание строки без учёта регистра.
bool StaticAnalyzer::endsWithCaseInsensitive(
    const std::string& s,
    const std::string& suffix
) {
    if (s.size() < suffix.size()) {
        return false;
    }

    auto it = s.end() - static_cast<std::ptrdiff_t>(suffix.size());

    for (size_t i = 0; i < suffix.size(); ++i) {
        char a = static_cast<char>(
            std::tolower(static_cast<unsigned char>(it[i]))
        );

        char b = static_cast<char>(
            std::tolower(static_cast<unsigned char>(suffix[i]))
        );

        if (a != b) {
            return false;
        }
    }

    return true;
}

// Удаляет неактивную часть строки make.mk.
//
// Если в make.mk есть:
//
// main.st # комментарий
//
// или:
//
// //old_file.st
//
// то анализатор не должен воспринимать комментарий как активный файл.
std::string StaticAnalyzer::removeMakeComment(const std::string& line) {
    bool escaped = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];

        // Комментарий make-файла.
        if (c == '#' && !escaped) {
            return line.substr(0, i);
        }

        // Комментарий в стиле //.
        if (i + 1 < line.size() &&
            line[i] == '/' &&
            line[i + 1] == '/') {
            return line.substr(0, i);
        }

        if (c == '\\' && !escaped) {
            escaped = true;
            continue;
        }

        escaped = false;
    }

    return line;
}

// Чистит токен из make.mk.
std::string StaticAnalyzer::cleanToken(std::string token) {
    // Убираем лишние символы слева.
    while (!token.empty()) {
        char c = token.front();

        if (c == '"' || c == '\'' || c == '(' || c == '[' || c == '{') {
            token.erase(token.begin());
        }
        else {
            break;
        }
    }

    // Убираем лишние символы справа.
    while (!token.empty()) {
        char c = token.back();

        if (c == '"' || c == '\'' || c == ')' || c == ']' || c == '}' ||
            c == ';' || c == ',' || c == ':') {
            token.pop_back();
        }
        else {
            break;
        }
    }

    // Нормализуем разделители путей.
    std::replace(token.begin(), token.end(), '\\', '/');

    return token;
}

// Читает make.mk как набор логических строк.
std::vector<std::string> StaticAnalyzer::readMakeLogicalLines() const {
    std::ifstream in(makefile_);

    if (!in) {
        throw std::runtime_error(
            "cannot open makefile: " + makefile_.string()
        );
    }

    std::vector<std::string> result;
    std::string current;
    std::string line;

    while (std::getline(in, line)) {
        // Сначала убираем неактивную часть строки.
        line = removeMakeComment(line);

        // Убираем пробелы по краям.
        std::string t = trim(line);

        // Если строка заканчивается на '\',
        // значит следующая строка продолжает текущую.
        if (!t.empty() && t.back() == '\\') {
            t.pop_back();
            current += t;
            current += ' ';
        }
        else {
            current += t;

            if (!trim(current).empty()) {
                result.push_back(current);
            }

            current.clear();
        }
    }

    // На случай, если файл закончился,
    // а в current ещё осталась строка.
    if (!trim(current).empty()) {
        result.push_back(current);
    }

    return result;
}

// Достаёт из make.mk все .st-файлы.
std::vector<std::filesystem::path> StaticAnalyzer::extractStsFilesFromMakefile() const {
    std::vector<std::string> lines = readMakeLogicalLines();

    // set нужен, чтобы не проверять один и тот же файл дважды.
    std::set<std::filesystem::path> uniqueFiles;

    for (const std::string& line : lines) {
        std::istringstream iss(line);
        std::string token;

        // Разбиваем строку по пробелам.
        while (iss >> token) {
            token = cleanToken(token);

            if (endsWithCaseInsensitive(token, ".st")) {
                uniqueFiles.insert(std::filesystem::path(token));
            }
        }
    }

    return std::vector<std::filesystem::path>(
        uniqueFiles.begin(),
        uniqueFiles.end()
    );
}

// Ищет реальный файл на диске.
std::filesystem::path StaticAnalyzer::resolveFilePath(
    const std::filesystem::path& fileFromMake
) const {
    // Если путь уже абсолютный и файл существует,
    // сразу возвращаем его.
    if (fileFromMake.is_absolute() &&
        std::filesystem::exists(fileFromMake)) {
        return std::filesystem::weakly_canonical(fileFromMake);
    }

    // Папка _make.
    std::filesystem::path makeDir = makefile_.parent_path();

    // Папка resource.
    std::filesystem::path resourceDir = makeDir.parent_path();

    // Пробуем несколько возможных вариантов расположения файла.
    std::vector<std::filesystem::path> candidates = {
        makeDir / fileFromMake,
        resourceDir / fileFromMake,
        std::filesystem::current_path() / fileFromMake
    };

    for (const std::filesystem::path& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return std::filesystem::weakly_canonical(candidate);
        }
    }

    // Пустой путь означает: файл не найден.
    return {};
}
