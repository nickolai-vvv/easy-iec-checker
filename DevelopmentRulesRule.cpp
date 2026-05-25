#include "DevelopmentRulesRule.h"
#include "DiagnosticOutput.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

std::string DevelopmentRulesRule::name() const {
    return "DEVELOPMENT_RULES";
}

std::string DevelopmentRulesRule::description() const {
    return "Development-only checks for array bounds, initialization, and precision loss";
}

int DevelopmentRulesRule::checkFile(const std::filesystem::path& file) const {
    const std::vector<LineInfo> lines = readLines(file);
    const std::map<std::string, VarInfo> variables = collectVariables(lines);

    int errors = 0;
    errors += checkExplicitArrayBounds(file, lines, variables);
    errors += checkUseBeforeInitialization(file, lines, variables);
    errors += checkIntegerDivisionAssignedToReal(file, lines, variables);
    return errors;
}

std::vector<DevelopmentRulesRule::LineInfo> DevelopmentRulesRule::readLines(
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

std::string DevelopmentRulesRule::removeStructuredTextCommentsFromLine(
    const std::string& line,
    bool& insideBlockComment,
    bool& insideBraceComment
) {
    std::string result;
    result.reserve(line.size());

    size_t i = 0;

    while (i < line.size()) {
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

        if (insideBraceComment) {
            if (line[i] == '}') {
                insideBraceComment = false;
            }
            result += ' ';
            ++i;
            continue;
        }

        if (i + 1 < line.size() && line[i] == '/' && line[i + 1] == '/') {
            result.append(line.size() - i, ' ');
            break;
        }

        if (i + 1 < line.size() && line[i] == '(' && line[i + 1] == '*') {
            insideBlockComment = true;
            result += "  ";
            i += 2;
            continue;
        }

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

std::string DevelopmentRulesRule::toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    return s;
}

std::string DevelopmentRulesRule::trim(const std::string& s) {
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

bool DevelopmentRulesRule::isIdentifierStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool DevelopmentRulesRule::isIdentifierChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool DevelopmentRulesRule::isTokenChar(char c) {
    return isIdentifierChar(c);
}

bool DevelopmentRulesRule::startsWithWord(
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

bool DevelopmentRulesRule::containsWord(
    const std::string& lowerLine,
    const std::string& lowerWord
) {
    size_t pos = lowerLine.find(lowerWord);

    while (pos != std::string::npos) {
        const bool leftOk = pos == 0 || !isTokenChar(lowerLine[pos - 1]);
        const bool rightOk = pos + lowerWord.size() >= lowerLine.size() ||
            !isTokenChar(lowerLine[pos + lowerWord.size()]);

        if (leftOk && rightOk) {
            return true;
        }

        pos = lowerLine.find(lowerWord, pos + 1);
    }

    return false;
}

bool DevelopmentRulesRule::isIntegerType(const std::string& type) {
    const std::string lower = toLower(type);
    return lower == "sint" || lower == "int" || lower == "dint" || lower == "lint" ||
        lower == "usint" || lower == "uint" || lower == "udint" || lower == "ulint" ||
        lower == "byte" || lower == "word" || lower == "dword" || lower == "lword";
}

bool DevelopmentRulesRule::isRealType(const std::string& type) {
    const std::string lower = toLower(type);
    return lower == "real" || lower == "lreal";
}

bool DevelopmentRulesRule::isIntegerLiteral(const std::string& text) {
    if (text.empty()) {
        return false;
    }

    size_t i = 0;
    if (text[i] == '+' || text[i] == '-') {
        ++i;
    }

    if (i >= text.size()) {
        return false;
    }

    for (; i < text.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
            return false;
        }
    }

    return true;
}

bool DevelopmentRulesRule::isRealLiteral(const std::string& text) {
    return text.find('.') != std::string::npos ||
        text.find('e') != std::string::npos ||
        text.find('E') != std::string::npos;
}

std::vector<std::string> DevelopmentRulesRule::splitDeclaredNames(
    const std::string& leftPart
) {
    std::vector<std::string> result;
    std::string current;

    for (char c : leftPart) {
        if (c == ',') {
            current = trim(toLower(current));
            if (!current.empty()) {
                result.push_back(current);
            }
            current.clear();
        }
        else {
            current += c;
        }
    }

    current = trim(toLower(current));
    if (!current.empty()) {
        result.push_back(current);
    }

    return result;
}

bool DevelopmentRulesRule::tryEnterVarBlock(
    const std::string& lowerTrimmed,
    VarBlockKind& kind
) {
    if (startsWithWord(lowerTrimmed, "var_input")) {
        kind = VarBlockKind::VarInput;
        return true;
    }
    if (startsWithWord(lowerTrimmed, "var_output")) {
        kind = VarBlockKind::VarOutput;
        return true;
    }
    if (startsWithWord(lowerTrimmed, "var_in_out")) {
        kind = VarBlockKind::VarInOut;
        return true;
    }
    if (startsWithWord(lowerTrimmed, "var_global")) {
        kind = VarBlockKind::VarGlobal;
        return true;
    }
    if (startsWithWord(lowerTrimmed, "var_temp")) {
        kind = VarBlockKind::VarTemp;
        return true;
    }
    if (startsWithWord(lowerTrimmed, "var")) {
        kind = VarBlockKind::Var;
        return true;
    }

    return false;
}

bool DevelopmentRulesRule::tryParseArrayBounds(
    const std::string& declarationRight,
    int& lower,
    int& upper
) {
    static const std::regex arrayRegex(
        R"(array\s*\[\s*([+-]?\d+)\s*\.\.\s*([+-]?\d+)\s*\])",
        std::regex_constants::icase
    );

    std::smatch match;
    if (!std::regex_search(declarationRight, match, arrayRegex)) {
        return false;
    }

    lower = std::stoi(match[1].str());
    upper = std::stoi(match[2].str());

    if (lower > upper) {
        std::swap(lower, upper);
    }

    return true;
}

std::string DevelopmentRulesRule::parseDeclaredType(
    const std::string& declarationRight
) {
    std::string right = toLower(declarationRight);
    const size_t assignPos = right.find(":=");
    if (assignPos != std::string::npos) {
        right = right.substr(0, assignPos);
    }

    const size_t semiPos = right.find(';');
    if (semiPos != std::string::npos) {
        right = right.substr(0, semiPos);
    }

    const size_t ofPos = right.find(" of ");
    if (ofPos != std::string::npos) {
        right = right.substr(ofPos + 4);
    }

    right = trim(right);
    std::istringstream iss(right);
    std::string type;
    iss >> type;
    return type;
}

std::map<std::string, DevelopmentRulesRule::VarInfo> DevelopmentRulesRule::collectVariables(
    const std::vector<LineInfo>& lines
) {
    std::map<std::string, VarInfo> result;
    VarBlockKind currentBlock = VarBlockKind::None;

    for (const LineInfo& line : lines) {
        const std::string lowerTrimmed = trim(toLower(line.codeOnly));

        if (lowerTrimmed.empty()) {
            continue;
        }

        if (tryEnterVarBlock(lowerTrimmed, currentBlock)) {
            continue;
        }

        if (startsWithWord(lowerTrimmed, "end_var")) {
            currentBlock = VarBlockKind::None;
            continue;
        }

        if (currentBlock == VarBlockKind::None) {
            continue;
        }

        const size_t colon = lowerTrimmed.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        const std::string left = lowerTrimmed.substr(0, colon);
        const std::string right = lowerTrimmed.substr(colon + 1);
        const std::vector<std::string> names = splitDeclaredNames(left);
        const bool hasInitializer = right.find(":=") != std::string::npos;
        const bool externallyInitialized =
            currentBlock == VarBlockKind::VarInput ||
            currentBlock == VarBlockKind::VarInOut;

        int arrayLower = 0;
        int arrayUpper = 0;
        const bool isArray = tryParseArrayBounds(right, arrayLower, arrayUpper);
        const std::string type = parseDeclaredType(right);

        for (const std::string& name : names) {
            if (name.empty() || !isIdentifierStart(name[0])) {
                continue;
            }

            bool simpleName = true;
            for (char c : name) {
                if (!isIdentifierChar(c)) {
                    simpleName = false;
                    break;
                }
            }

            if (!simpleName) {
                continue;
            }

            VarInfo info;
            info.name = name;
            info.type = type;
            info.initialized = hasInitializer || externallyInitialized;
            info.isArray = isArray;
            info.arrayLower = arrayLower;
            info.arrayUpper = arrayUpper;
            info.blockKind = currentBlock;
            result[name] = info;
        }
    }

    return result;
}

bool DevelopmentRulesRule::tryParseAssignment(
    const std::string& code,
    Assignment& assignment
) {
    const size_t assignPos = code.find(":=");
    if (assignPos == std::string::npos) {
        return false;
    }

    assignment.assignPos = assignPos;
    assignment.lhs = trim(toLower(code.substr(0, assignPos)));
    assignment.rhs = code.substr(assignPos + 2);

    if (assignment.lhs.empty() || !isIdentifierStart(assignment.lhs[0])) {
        assignment.lhsIsSimpleVariable = false;
        return true;
    }

    assignment.lhsIsSimpleVariable = true;
    for (char c : assignment.lhs) {
        if (!isIdentifierChar(c)) {
            assignment.lhsIsSimpleVariable = false;
            break;
        }
    }

    return true;
}

std::string DevelopmentRulesRule::readOperandBefore(
    const std::string& code,
    size_t position
) {
    if (position == 0) {
        return {};
    }

    size_t end = position;
    while (end > 0 && std::isspace(static_cast<unsigned char>(code[end - 1]))) {
        --end;
    }

    if (end == 0) {
        return {};
    }

    size_t start = end;

    if (std::isdigit(static_cast<unsigned char>(code[start - 1]))) {
        while (start > 0 &&
            (std::isdigit(static_cast<unsigned char>(code[start - 1])) ||
             code[start - 1] == '.')) {
            --start;
        }
    }
    else if (isIdentifierChar(code[start - 1])) {
        while (start > 0 && isIdentifierChar(code[start - 1])) {
            --start;
        }
    }
    else {
        return {};
    }

    return trim(code.substr(start, end - start));
}

std::string DevelopmentRulesRule::readOperandAfter(
    const std::string& code,
    size_t position
) {
    size_t start = position;
    while (start < code.size() && std::isspace(static_cast<unsigned char>(code[start]))) {
        ++start;
    }

    if (start >= code.size()) {
        return {};
    }

    size_t end = start;

    if (code[end] == '+' || code[end] == '-') {
        ++end;
    }

    if (end < code.size() && std::isdigit(static_cast<unsigned char>(code[end]))) {
        while (end < code.size() &&
            (std::isdigit(static_cast<unsigned char>(code[end])) || code[end] == '.')) {
            ++end;
        }
    }
    else if (end < code.size() && isIdentifierStart(code[end])) {
        ++end;
        while (end < code.size() && isIdentifierChar(code[end])) {
            ++end;
        }
    }
    else {
        return {};
    }

    return trim(code.substr(start, end - start));
}

bool DevelopmentRulesRule::operandIsInteger(
    const std::string& operand,
    const std::map<std::string, VarInfo>& variables
) {
    const std::string lower = toLower(trim(operand));

    if (isIntegerLiteral(lower)) {
        return true;
    }

    if (isRealLiteral(lower)) {
        return false;
    }

    auto it = variables.find(lower);
    if (it == variables.end()) {
        return false;
    }

    return isIntegerType(it->second.type);
}

void DevelopmentRulesRule::report(
    const std::filesystem::path& file,
    size_t lineNumber,
    size_t columnStart,
    size_t columnEnd,
    const std::string& message
) {
    DiagnosticOutput::printCompilerStyleError(
        file,
        lineNumber,
        columnStart,
        columnEnd,
        message
    );
}

int DevelopmentRulesRule::checkExplicitArrayBounds(
    const std::filesystem::path& file,
    const std::vector<LineInfo>& lines,
    const std::map<std::string, VarInfo>& variables
) {
    int errors = 0;

    for (const LineInfo& line : lines) {
        const std::string lower = toLower(line.codeOnly);

        for (const auto& item : variables) {
            const VarInfo& info = item.second;
            if (!info.isArray) {
                continue;
            }

            size_t pos = lower.find(info.name);
            while (pos != std::string::npos) {
                const bool leftOk = pos == 0 || !isTokenChar(lower[pos - 1]);
                size_t afterName = pos + info.name.size();
                const bool rightOk = afterName >= lower.size() || !isTokenChar(lower[afterName]);

                if (leftOk && rightOk) {
                    while (afterName < lower.size() &&
                        std::isspace(static_cast<unsigned char>(lower[afterName]))) {
                        ++afterName;
                    }

                    if (afterName < lower.size() && lower[afterName] == '[') {
                        const size_t close = lower.find(']', afterName + 1);
                        if (close != std::string::npos) {
                            const std::string indexText = trim(
                                lower.substr(afterName + 1, close - afterName - 1)
                            );

                            if (isIntegerLiteral(indexText)) {
                                const int index = std::stoi(indexText);
                                if (index < info.arrayLower || index > info.arrayUpper) {
                                    report(
                                        file,
                                        line.number,
                                        afterName + 2,
                                        close,
                                        "forbidden explicit array index outside declared bounds"
                                    );
                                    ++errors;
                                }
                            }
                        }
                    }
                }

                pos = lower.find(info.name, pos + 1);
            }
        }
    }

    return errors;
}

int DevelopmentRulesRule::checkUseBeforeInitialization(
    const std::filesystem::path& file,
    const std::vector<LineInfo>& lines,
    const std::map<std::string, VarInfo>& variables
) {
    int errors = 0;
    std::set<std::string> initialized;
    std::set<std::string> reportedOnLine;
    VarBlockKind currentBlock = VarBlockKind::None;

    for (const auto& item : variables) {
        if (item.second.initialized) {
            initialized.insert(item.first);
        }
    }

    for (const LineInfo& line : lines) {
        const std::string lower = toLower(line.codeOnly);
        const std::string lowerTrimmed = trim(lower);

        if (tryEnterVarBlock(lowerTrimmed, currentBlock)) {
            continue;
        }

        if (startsWithWord(lowerTrimmed, "end_var")) {
            currentBlock = VarBlockKind::None;
            continue;
        }

        if (currentBlock != VarBlockKind::None || lowerTrimmed.empty()) {
            continue;
        }

        Assignment assignment;
        const bool hasAssignment = tryParseAssignment(line.codeOnly, assignment);
        const size_t scanStart = hasAssignment ? assignment.assignPos + 2 : 0;
        reportedOnLine.clear();

        size_t pos = scanStart;
        while (pos < lower.size()) {
            if (!isIdentifierStart(lower[pos])) {
                ++pos;
                continue;
            }

            const size_t start = pos;
            ++pos;
            while (pos < lower.size() && isIdentifierChar(lower[pos])) {
                ++pos;
            }

            const std::string name = lower.substr(start, pos - start);
            auto varIt = variables.find(name);
            if (varIt == variables.end()) {
                continue;
            }

            if (initialized.find(name) != initialized.end()) {
                continue;
            }

            if (reportedOnLine.insert(name).second) {
                report(
                    file,
                    line.number,
                    start + 1,
                    pos,
                    "forbidden use of variable before initialization"
                );
                ++errors;
            }
        }

        if (!hasAssignment) {
            continue;
        }

        std::string lhsName = assignment.lhs;
        if (assignment.lhsIsSimpleVariable) {
            initialized.insert(lhsName);
            continue;
        }

        const size_t bracket = lhsName.find('[');
        if (bracket != std::string::npos) {
            lhsName = trim(lhsName.substr(0, bracket));
            if (!lhsName.empty()) {
                initialized.insert(lhsName);
            }
        }
    }

    return errors;
}

int DevelopmentRulesRule::checkIntegerDivisionAssignedToReal(
    const std::filesystem::path& file,
    const std::vector<LineInfo>& lines,
    const std::map<std::string, VarInfo>& variables
) {
    int errors = 0;

    for (const LineInfo& line : lines) {
        Assignment assignment;
        if (!tryParseAssignment(line.codeOnly, assignment) ||
            !assignment.lhsIsSimpleVariable) {
            continue;
        }

        const std::string lhs = toLower(assignment.lhs);
        auto lhsIt = variables.find(lhs);
        if (lhsIt == variables.end() || !isRealType(lhsIt->second.type)) {
            continue;
        }

        size_t slash = assignment.rhs.find('/');
        while (slash != std::string::npos) {
            const std::string left = readOperandBefore(assignment.rhs, slash);
            const std::string right = readOperandAfter(assignment.rhs, slash + 1);

            if (operandIsInteger(left, variables) &&
                operandIsInteger(right, variables)) {
                const size_t column = assignment.assignPos + 3 + slash;
                report(
                    file,
                    line.number,
                    column,
                    column,
                    "forbidden integer division assigned to REAL; convert operands to REAL/LREAL first"
                );
                ++errors;
            }

            slash = assignment.rhs.find('/', slash + 1);
        }
    }

    return errors;
}
