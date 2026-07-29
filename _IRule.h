#pragma once

#include <string>

class ProjectAnalysis;

// Правило не читает файлы самостоятельно. StaticAnalyzer один раз строит
// ProjectAnalysis и запускает все правила на общей модели проекта.
class IRule {
public:
    virtual ~IRule() = default;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual int check(const ProjectAnalysis& project) const = 0;
};
