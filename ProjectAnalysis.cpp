#include "ProjectAnalysis.h"
#include "DiagnosticOutput.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>

static std::string stripComments(
    const std::string& input,
    bool& block,
    bool& brace
) {
    std::string out;

    for (size_t i = 0; i < input.size(); ++i) {
        if (block) {
            if (i + 1 < input.size() &&
                input[i] == '*' &&
                input[i + 1] == ')') {
                block = false;
                ++i;
            }

            continue;
        }

        if (brace) {
            if (input[i] == '}') {
                brace = false;
            }

            continue;
        }

        // Однострочный комментарий: остаток строки не является кодом.
        if (i + 1 < input.size() && input[i] == '/' && input[i + 1] == '/') {
            break;
        }

        // Блочный комментарий может продолжаться на следующих строках.
        if (i + 1 < input.size() && input[i] == '(' && input[i + 1] == '*') {
            block = true;
            ++i;
            continue;
        }

        if (input[i] == '{') {
            brace = true;
            continue;
        }

        out += input[i];
    }

    return out;
}
static bool literal(const std::string& text, bool& real) {
    static const std::regex integer(R"(^[+-]?\d+$)");
    static const std::regex floating(R"(^[+-]?(\d+\.\d*|\d*\.\d+)([eE][+-]?\d+)?$)");
    real = std::regex_match(text, floating); return real || std::regex_match(text, integer);
}

ProjectAnalysis ProjectAnalysis::load(const std::vector<std::filesystem::path>& files) {
    ProjectAnalysis result;
    const std::regex declaration(R"(^\s*([A-Za-z_][A-Za-z0-9_]*(\s*,\s*[A-Za-z_][A-Za-z0-9_]*)*)\s*:\s*(.*?)\s*;\s*$)", std::regex::icase);
    const std::regex bounds(R"(ARRAY\s*\[\s*([+-]?\d+)\s*\.\.\s*([+-]?\d+)\s*\]\s*OF\s*([A-Za-z_][A-Za-z0-9_]*))", std::regex::icase);
    for (const auto& file : files) {
        std::ifstream in(file);
        if (!in) continue;
        SourceUnit source; source.file = file;
        bool block = false, brace = false, inVar = false;
        std::string raw; size_t number = 0;
        while (std::getline(in, raw)) {
            ++number; std::string code = stripComments(raw, block, brace);
            source.lines.push_back({ number, code });
            const std::string lowered = lower(trim(code));
            if (std::regex_match(lowered, std::regex(R"(^var(_input|_output|_in_out|_global|_temp)?\b.*)", std::regex::icase))) { inVar = true; continue; }
            if (lowered.rfind("end_var", 0) == 0) { inVar = false; continue; }
            if (!inVar) continue;
            std::smatch match;
            if (!std::regex_match(code, match, declaration)) continue;
            std::string names = match[1], right = match[3];
            std::smatch arrayMatch; VariableInfo base;
            base.hasInitializer = right.find(":=") != std::string::npos;
            if (std::regex_search(right, arrayMatch, bounds)) {
                base.isArray = true; base.lowerBound = std::stoi(arrayMatch[1]); base.upperBound = std::stoi(arrayMatch[2]); base.type = lower(arrayMatch[3]);
            } else {
                const size_t init = right.find(":="); base.type = lower(trim(right.substr(0, init)));
            }
            size_t start = 0;
            while (start < names.size()) { size_t comma = names.find(',', start); std::string name = lower(trim(names.substr(start, comma - start))); VariableInfo info = base; info.name = name; source.variables[name] = info; result.projectVariables_.try_emplace(name, info); if (comma == std::string::npos) break; start = comma + 1; }
        }
        result.sources_.push_back(std::move(source));
    }
    return result;
}

const VariableInfo* ProjectAnalysis::findVariable(const SourceUnit& source, const std::string& name) const {
    const std::string key = lower(trim(name));
    if (auto it = source.variables.find(key); it != source.variables.end()) return &it->second;
    if (auto it = projectVariables_.find(key); it != projectVariables_.end()) return &it->second;
    return nullptr;
}
std::string ProjectAnalysis::lower(std::string value) { std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); }); return value; }
std::string ProjectAnalysis::trim(const std::string& value) { const auto first = value.find_first_not_of(" \t\r\n"); if (first == std::string::npos) return {}; const auto last = value.find_last_not_of(" \t\r\n"); return value.substr(first, last - first + 1); }
bool ProjectAnalysis::isIntegerType(const std::string& type) { const auto t = lower(type); return t == "sint" || t == "int" || t == "dint" || t == "lint" || t == "usint" || t == "uint" || t == "udint" || t == "ulint" || t == "byte" || t == "word" || t == "dword" || t == "lword"; }
bool ProjectAnalysis::isRealType(const std::string& type) { const auto t = lower(type); return t == "real" || t == "lreal"; }
bool ProjectAnalysis::isNumericType(const std::string& type) { return isIntegerType(type) || isRealType(type); }
const VariableInfo* ProjectAnalysis::expressionType(const SourceUnit& source, const std::string& text, const ProjectAnalysis& project) { bool real = false; const auto value = trim(text); if (literal(value, real)) { static VariableInfo integer{"", "int"}; static VariableInfo floating{"", "real"}; return real ? &floating : &integer; } static const std::regex id(R"(^[A-Za-z_][A-Za-z0-9_]*$)"); return std::regex_match(value, id) ? project.findVariable(source, value) : nullptr; }
bool ProjectAnalysis::isIntegerExpression(const SourceUnit& source, const std::string& text, const ProjectAnalysis& project) { const auto* info = expressionType(source, text, project); return info && isIntegerType(info->type); }
void ProjectAnalysis::report(const std::filesystem::path& file, size_t line, size_t first, size_t last, int errorCode, const std::string& message) { DiagnosticOutput::printCompilerStyleError(file, line, first, last, errorCode, message); }
