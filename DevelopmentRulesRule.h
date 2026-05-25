#pragma once

#include "_IRule.h"

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

class DevelopmentRulesRule : public IRule {
public:
    std::string name() const override;
    std::string description() const override;
    int checkFile(const std::filesystem::path& file) const override;

private:
    enum class VarBlockKind {
        None,
        Var,
        VarInput,
        VarOutput,
        VarInOut,
        VarGlobal,
        VarTemp
    };

    struct LineInfo {
        size_t number = 0;
        std::string original;
        std::string codeOnly;
    };

    struct VarInfo {
        std::string name;
        std::string type;
        bool initialized = false;
        bool isArray = false;
        int arrayLower = 0;
        int arrayUpper = 0;
        VarBlockKind blockKind = VarBlockKind::None;
    };

    struct Assignment {
        std::string lhs;
        std::string rhs;
        size_t assignPos = std::string::npos;
        bool lhsIsSimpleVariable = false;
    };

private:
    static std::vector<LineInfo> readLines(const std::filesystem::path& file);
    static std::string removeStructuredTextCommentsFromLine(
        const std::string& line,
        bool& insideBlockComment,
        bool& insideBraceComment
    );

    static std::string toLower(std::string s);
    static std::string trim(const std::string& s);
    static bool isIdentifierStart(char c);
    static bool isIdentifierChar(char c);
    static bool isTokenChar(char c);
    static bool startsWithWord(const std::string& lowerTrimmed, const std::string& lowerWord);
    static bool containsWord(const std::string& lowerLine, const std::string& lowerWord);
    static bool isIntegerType(const std::string& type);
    static bool isRealType(const std::string& type);
    static bool isIntegerLiteral(const std::string& text);
    static bool isRealLiteral(const std::string& text);
    static std::vector<std::string> splitDeclaredNames(const std::string& leftPart);

    static bool tryEnterVarBlock(const std::string& lowerTrimmed, VarBlockKind& kind);
    static bool tryParseArrayBounds(const std::string& declarationRight, int& lower, int& upper);
    static std::string parseDeclaredType(const std::string& declarationRight);
    static std::map<std::string, VarInfo> collectVariables(const std::vector<LineInfo>& lines);

    static bool tryParseAssignment(const std::string& code, Assignment& assignment);
    static std::string readOperandBefore(const std::string& code, size_t position);
    static std::string readOperandAfter(const std::string& code, size_t position);
    static bool operandIsInteger(
        const std::string& operand,
        const std::map<std::string, VarInfo>& variables
    );

    static void report(
        const std::filesystem::path& file,
        size_t lineNumber,
        size_t columnStart,
        size_t columnEnd,
        const std::string& message
    );

    static int checkExplicitArrayBounds(
        const std::filesystem::path& file,
        const std::vector<LineInfo>& lines,
        const std::map<std::string, VarInfo>& variables
    );

    static int checkUseBeforeInitialization(
        const std::filesystem::path& file,
        const std::vector<LineInfo>& lines,
        const std::map<std::string, VarInfo>& variables
    );

    static int checkIntegerDivisionAssignedToReal(
        const std::filesystem::path& file,
        const std::vector<LineInfo>& lines,
        const std::map<std::string, VarInfo>& variables
    );
};
