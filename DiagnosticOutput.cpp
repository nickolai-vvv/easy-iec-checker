#include "DiagnosticOutput.h"

#include <iostream>
#include <unordered_map>

namespace {

std::unordered_map<std::string, std::filesystem::path> displayPaths;

std::string keyForPath(const std::filesystem::path& file) {
    return file.lexically_normal().generic_string();
}

std::filesystem::path cleanDisplayPath(std::filesystem::path path) {
    path = path.lexically_normal();

    std::filesystem::path cleaned;
    for (const std::filesystem::path& part : path) {
        if (part == ".") {
            continue;
        }

        cleaned /= part;
    }

    return cleaned.empty() ? path : cleaned;
}

std::string formatPathForDiagnostic(const std::filesystem::path& file) {
    const auto found = displayPaths.find(keyForPath(file));
    if (found != displayPaths.end()) {
        return found->second.generic_string();
    }

    return cleanDisplayPath(file).generic_string();
}

}

namespace DiagnosticOutput {

void setDisplayPath(
    const std::filesystem::path& file,
    const std::filesystem::path& displayPath
) {
    displayPaths[keyForPath(file)] = cleanDisplayPath(displayPath);
}

void printCompilerStyleError(
    const std::filesystem::path& file,
    size_t lineNumber,
    size_t columnStart,
    size_t columnEnd,
    int errorCode,
    const std::string& message
) {
    std::cerr
        << formatPathForDiagnostic(file)
        << ":"
        << lineNumber
        << "."
        << columnStart
        << "-"
        << columnEnd
        << " : error C"
        << errorCode
        << ": "
        << message
        << "\n";
}

}
