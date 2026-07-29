#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct SourceLine {
    size_t number{};
    std::string code;
};

struct VariableInfo {
    std::string name;
    std::string type;
    bool hasInitializer{};
    bool isArray{};
    int lowerBound{};
    int upperBound{};
};

struct SourceUnit {
    std::filesystem::path file;
    std::vector<SourceLine> lines;
    std::map<std::string, VariableInfo> variables;
};

// Общий разбор поддерживаемого подмножества ST. Здесь намеренно собраны
// комментарии, лексика и декларации, чтобы правила оставались короткими.
class ProjectAnalysis {
public:
    static ProjectAnalysis load(const std::vector<std::filesystem::path>& files);

    const std::vector<SourceUnit>& sources() const { return sources_; }
    const VariableInfo* findVariable(const SourceUnit& source, const std::string& name) const;

    static std::string lower(std::string value);
    static std::string trim(const std::string& value);
    static bool isIntegerType(const std::string& type);
    static bool isRealType(const std::string& type);
    static bool isNumericType(const std::string& type);
    static bool isIntegerExpression(const SourceUnit& source, const std::string& text, const ProjectAnalysis& project);
    static const VariableInfo* expressionType(const SourceUnit& source, const std::string& text, const ProjectAnalysis& project);
    static void report(const std::filesystem::path& file, size_t line, size_t first, size_t last, int errorCode, const std::string& message);

private:
    std::vector<SourceUnit> sources_;
    std::map<std::string, VariableInfo> projectVariables_;
};
