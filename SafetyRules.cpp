#include "SafetyRules.h"
#include "ProjectAnalysis.h"

#include <regex>
#include <set>

#pragma region CommonHelpers

static const std::regex assignmentPattern(
    R"(^(.*?)\s*:=\s*(.*?)\s*;\s*$)"
);

static bool parseAssignment(const std::string& code, std::smatch& match) {
    return std::regex_match(code, match, assignmentPattern);
}

static std::string assignmentTarget(const std::string& lhs) {
    const size_t bracket = lhs.find('[');
    return ProjectAnalysis::trim(lhs.substr(0, bracket));
}

#pragma endregion

#pragma region ArrayBoundsRule

std::string ArrayBoundsRule::name() const {
    return "ARRAY_BOUNDS";
}

std::string ArrayBoundsRule::description() const {
    return "Explicit constant array indexes must be inside declared bounds";
}

int ArrayBoundsRule::check(const ProjectAnalysis& project) const {
    int errors = 0;
    const std::regex arrayAccess(
        R"(\b([A-Za-z_][A-Za-z0-9_]*)\s*\[\s*([+-]?\d+)\s*\])"
    );

    for (const auto& source : project.sources()) {
        for (const auto& line : source.lines) {
            for (std::sregex_iterator it(
                    line.code.begin(), line.code.end(), arrayAccess
                ), end; it != end; ++it) {
                const auto* variable = project.findVariable(source, (*it)[1]);

                if (!variable || !variable->isArray) {
                    continue;
                }

                const int index = std::stoi((*it)[2]);
                if (index >= variable->lowerBound && index <= variable->upperBound) {
                    continue;
                }

                const size_t column = static_cast<size_t>((*it).position(2));
                ProjectAnalysis::report(
                    source.file,
                    line.number,
                    column + 1,
                    column + (*it)[2].length(),
                    9001,
                    "array index is outside declared bounds ARRAY [" +
                        std::to_string(variable->lowerBound) + ".." +
                        std::to_string(variable->upperBound) + "]"
                );
                ++errors;
            }
        }
    }

    return errors;
}

#pragma endregion

#pragma region ImplicitConversionRule

std::string ImplicitConversionRule::name() const {
    return "IMPLICIT_CONVERSION";
}

std::string ImplicitConversionRule::description() const {
    return "Assignments between different scalar data types require an explicit conversion";
}

int ImplicitConversionRule::check(const ProjectAnalysis& project) const {
    int errors = 0;

    for (const auto& source : project.sources()) {
        for (const auto& line : source.lines) {
            std::smatch assignment;
            if (!parseAssignment(line.code, assignment)) {
                continue;
            }

            const auto* destination = project.findVariable(
                source,
                assignmentTarget(assignment[1])
            );
            const auto* sourceValue = ProjectAnalysis::expressionType(
                source,
                assignment[2],
                project
            );

            if (!destination || destination->isArray || !sourceValue ||
                !ProjectAnalysis::isNumericType(destination->type) ||
                !ProjectAnalysis::isNumericType(sourceValue->type) ||
                ProjectAnalysis::lower(destination->type) ==
                    ProjectAnalysis::lower(sourceValue->type)) {
                continue;
            }

            const size_t column = static_cast<size_t>(assignment.position(2));
            ProjectAnalysis::report(
                source.file,
                line.number,
                column + 1,
                column + assignment.length(2),
                9002,
                "implicit conversion from " + ProjectAnalysis::lower(sourceValue->type) +
                    " to " + ProjectAnalysis::lower(destination->type) +
                    " is forbidden; use an explicit conversion"
            );
            ++errors;
        }
    }

    return errors;
}

#pragma endregion

#pragma region RealComparisonRule

std::string RealComparisonRule::name() const {
    return "REAL_COMPARISON";
}

std::string RealComparisonRule::description() const {
    return "REAL and LREAL values must not be compared with = or <>";
}

int RealComparisonRule::check(const ProjectAnalysis& project) const {
    int errors = 0;
    const std::regex comparison(
        R"(([A-Za-z_][A-Za-z0-9_]*|[+-]?(?:\d+\.\d*|\d*\.\d+))\s*(=|<>)\s*([A-Za-z_][A-Za-z0-9_]*|[+-]?(?:\d+\.\d*|\d*\.\d+)))"
    );

    for (const auto& source : project.sources()) {
        for (const auto& line : source.lines) {
            for (std::sregex_iterator it(
                    line.code.begin(), line.code.end(), comparison
                ), end; it != end; ++it) {
                const auto* left = ProjectAnalysis::expressionType(
                    source,
                    (*it)[1],
                    project
                );
                const auto* right = ProjectAnalysis::expressionType(
                    source,
                    (*it)[3],
                    project
                );

                const bool hasRealOperand =
                    (left && ProjectAnalysis::isRealType(left->type)) ||
                    (right && ProjectAnalysis::isRealType(right->type));

                if (!hasRealOperand) {
                    continue;
                }

                const size_t column = static_cast<size_t>((*it).position(2));
                ProjectAnalysis::report(
                    source.file,
                    line.number,
                    column + 1,
                    column + (*it)[2].length(),
                    9003,
                    "comparison of floating-point values using '" +
                        (*it)[2].str() + "' is forbidden"
                );
                ++errors;
            }
        }
    }

    return errors;
}

#pragma endregion

#pragma region UninitializedVariableRule

std::string UninitializedVariableRule::name() const {
    return "UNINITIALIZED_VARIABLE";
}

std::string UninitializedVariableRule::description() const {
    return "Variables must be assigned before their first read";
}

int UninitializedVariableRule::check(const ProjectAnalysis& project) const {
    int errors = 0;
    const std::regex identifier(R"([A-Za-z_][A-Za-z0-9_]*)");

    for (const auto& source : project.sources()) {
        std::set<std::string> initialized;
        for (const auto& [name, variable] : source.variables) {
            if (variable.hasInitializer) {
                initialized.insert(name);
            }
        }

        bool inVarBlock = false;
        for (const auto& line : source.lines) {
            const std::string trimmed = ProjectAnalysis::lower(
                ProjectAnalysis::trim(line.code)
            );

            if (trimmed.rfind("var", 0) == 0) {
                inVarBlock = true;
                continue;
            }

            if (trimmed.rfind("end_var", 0) == 0) {
                inVarBlock = false;
                continue;
            }

            if (inVarBlock) {
                continue;
            }

            std::smatch assignment;
            const bool hasAssignment = parseAssignment(line.code, assignment);
            const std::string readExpression = hasAssignment
                ? assignment[2].str()
                : line.code;
            std::set<std::string> reportedOnLine;

            for (std::sregex_iterator it(
                    readExpression.begin(), readExpression.end(), identifier
                ), end; it != end; ++it) {
                const std::string name = ProjectAnalysis::lower((*it).str());
                const auto variable = source.variables.find(name);

                if (variable == source.variables.end() ||
                    initialized.contains(name) ||
                    !reportedOnLine.insert(name).second) {
                    continue;
                }

                const size_t column = hasAssignment
                    ? static_cast<size_t>(assignment.position(2) + (*it).position())
                    : static_cast<size_t>((*it).position());
                ProjectAnalysis::report(
                    source.file,
                    line.number,
                    column + 1,
                    column + name.size(),
                    9004,
                    "use of uninitialized variable '" + name + "' is forbidden"
                );
                ++errors;
            }

            if (hasAssignment) {
                const std::string target = ProjectAnalysis::lower(
                    assignmentTarget(assignment[1])
                );

                if (source.variables.contains(target)) {
                    initialized.insert(target);
                }
            }
        }
    }

    return errors;
}

#pragma endregion

#pragma region IntegerDivisionRule

std::string IntegerDivisionRule::name() const {
    return "INTEGER_DIVISION_TO_REAL";
}

std::string IntegerDivisionRule::description() const {
    return "Integer division must not be assigned to REAL or LREAL";
}

int IntegerDivisionRule::check(const ProjectAnalysis& project) const {
    int errors = 0;
    const std::regex division(R"(([^/\s]+)\s*/\s*([^/\s;]+))");

    for (const auto& source : project.sources()) {
        for (const auto& line : source.lines) {
            std::smatch assignment;
            if (!parseAssignment(line.code, assignment)) {
                continue;
            }

            const auto* destination = project.findVariable(
                source,
                assignmentTarget(assignment[1])
            );

            if (!destination || !ProjectAnalysis::isRealType(destination->type)) {
                continue;
            }

            const std::string rhs = assignment[2];
            for (std::sregex_iterator it(rhs.begin(), rhs.end(), division), end;
                it != end;
                ++it) {
                if (!ProjectAnalysis::isIntegerExpression(source, (*it)[1], project) ||
                    !ProjectAnalysis::isIntegerExpression(source, (*it)[2], project)) {
                    continue;
                }

                const size_t column = static_cast<size_t>(
                    assignment.position(2) + (*it).position()
                );
                ProjectAnalysis::report(
                    source.file,
                    line.number,
                    column + 1,
                    column + (*it).length(),
                    9005,
                    "integer division before assignment to REAL is forbidden; "
                    "convert operands to REAL/LREAL first"
                );
                ++errors;
            }
        }
    }

    return errors;
}

#pragma endregion
