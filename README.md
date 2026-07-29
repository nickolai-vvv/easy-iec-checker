# easy-iec-checker

A static analyzer for IEC 61131-3 Structured Text. It reads `make.mk`, finds
the `.st` files listed there, and prints diagnostics suitable for a terminal,
IDE, or CI job.

## Checks

- Explicit constant array index outside declared bounds.
- Implicit numeric conversion in an assignment.
- `REAL` / `LREAL` comparison using `=` or `<>`.
- Reading a local variable before an initializer or an assignment.
- Integer division whose result is assigned to `REAL` / `LREAL`.

For example, `Values[3]` is invalid for `ARRAY [0..2] OF INT`. An assignment
such as `R := A`, where `R` is `REAL` and `A` is `INT`, requires an explicit
conversion. `Values[3] := A` is not a type conversion when both values are
`INT`, but it still violates the array bounds.

## Architecture

```text
make.mk -> .st source list -> ProjectAnalysis -> rules -> diagnostics
```

`StaticAnalyzer` is an orchestrator. It finds source files, builds one
`ProjectAnalysis`, then runs the registered rules.

`ProjectAnalysis` is the shared project model: comment-free source lines,
declarations, variable types, initializers, and array bounds. A rule can
therefore resolve declarations from another source file listed in the same
`make.mk`.

Before analysis, `//` comments and `(* ... *)` block comments are removed from
source lines. Rule diagnostics are therefore never emitted for text inside a
comment.

Each rule implements one policy and never reads source files itself.

## Build

Open `easy-iec-checker.slnx` in Visual Studio and build the project. Or run the
following command from Developer PowerShell for Visual Studio:

```powershell
msbuild .\easy-iec-checker.vcxproj /p:Configuration=Release /p:Platform=x64
```

The executable for this configuration is usually located at:

```text
.\x64\Release\easy-iec-checker.exe
```

## Usage

From the repository root:

```powershell
.\x64\Release\easy-iec-checker.exe
```

By default, the analyzer uses `resource/_make/make.mk`. You can pass another
makefile explicitly:

```powershell
.\x64\Release\easy-iec-checker.exe .\resource\_make\make.mk
.\x64\Release\easy-iec-checker.exe --makefile .\resource\_make\make.mk
.\x64\Release\easy-iec-checker.exe -m .\resource\_make\make.mk
```

## make.mk Format

The analyzer extracts `.st` source paths from the makefile. Line continuations
using `\` and relative paths are supported.

```makefile
FILES = \
    LogicalProgram.st \
    Regul/RegulProgram.st
```

## Diagnostics and Exit Codes

Every rule uses the same diagnostic format:

```text
LogicalProgram3.st:25.8-8 : error C9003: comparison of floating-point values using '=' is forbidden
easy-iec-checker: found 1 error(s)
```

| Diagnostic | Rule |
| --- | --- |
| `C9001` | Array index outside declared bounds. |
| `C9002` | Forbidden implicit numeric conversion. |
| `C9003` | Forbidden `REAL` / `LREAL` comparison with `=` or `<>`. |
| `C9004` | Use of an uninitialized variable is forbidden. |
| `C9005` | Forbidden integer division before assignment to `REAL` / `LREAL`. |

| Code | Meaning |
| --- | --- |
| `0` | Analysis completed successfully; no issues found. |
| `1` | Analysis completed and rule violations were found. |
| `2` | Startup error, for example a missing `make.mk`. |

## Adding a Rule

Create a class that implements `IRule`:

```cpp
class IRule {
public:
    virtual ~IRule() = default;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual int check(const ProjectAnalysis& project) const = 0;
};
```

The rule traverses `project.sources()`, uses `ProjectAnalysis::findVariable`
and the common type helpers, then emits diagnostics through
`ProjectAnalysis::report`. Finally, register it in
`StaticAnalyzer::registerRules()`. No makefile parsing, file reading, or
declaration parsing needs to be duplicated.
