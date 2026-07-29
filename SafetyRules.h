#pragma once
#include "_IRule.h"

#pragma region ArrayBoundsRule

class ArrayBoundsRule final : public IRule {
public:
    std::string name() const override;
    std::string description() const override;
    int check(const ProjectAnalysis& project) const override;
};

#pragma endregion

#pragma region ImplicitConversionRule

class ImplicitConversionRule final : public IRule {
public:
    std::string name() const override;
    std::string description() const override;
    int check(const ProjectAnalysis& project) const override;
};

#pragma endregion

#pragma region RealComparisonRule

class RealComparisonRule final : public IRule {
public:
    std::string name() const override;
    std::string description() const override;
    int check(const ProjectAnalysis& project) const override;
};

#pragma endregion

#pragma region UninitializedVariableRule

class UninitializedVariableRule final : public IRule {
public:
    std::string name() const override;
    std::string description() const override;
    int check(const ProjectAnalysis& project) const override;
};

#pragma endregion

#pragma region IntegerDivisionRule

class IntegerDivisionRule final : public IRule {
public:
    std::string name() const override;
    std::string description() const override;
    int check(const ProjectAnalysis& project) const override;
};

#pragma endregion
