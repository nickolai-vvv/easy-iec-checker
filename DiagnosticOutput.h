#pragma once

#include <filesystem>
#include <string>

namespace DiagnosticOutput {

void setDisplayPath(
    const std::filesystem::path& file,
    const std::filesystem::path& displayPath
);

void printCompilerStyleError(
    const std::filesystem::path& file,
    size_t lineNumber,
    size_t columnStart,
    size_t columnEnd,
    const std::string& message
);

}
