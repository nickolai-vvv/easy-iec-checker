# easy-iec-checker

Lightweight static analyzer for IEC 61131-3 Structured Text code.

`easy-iec-checker` reads a project `make.mk`, finds listed `.sts` source files, and checks them for unsafe or forbidden PLC code patterns. Diagnostics are printed with file names, line numbers, columns, and short error messages, so the output can be used from a terminal, IDE, or CI job.

## What It Checks

- Forbidden logical operators: `XOR`, `OR`, `AND`, `NOT`
- Forbidden numeric conversions, for example `REAL_TO_INT`, `REAL_TO_DINT`, `LREAL_TO_INT`, `DINT_TO_INT`
- Forbidden loop and control-flow constructs: `WHILE`, `REPEAT`, `GOTO`, `LABEL`, `CONTINUE`
- Pointer-like or unsafe access patterns: `POINTER`, `ADR`, `AT`, `ANY`
- Legacy PLC types: `S5TIME`, `TIMER`, `COUNTER`
- Technical-debt markers: `TODO`, `FIXME`, `HACK`
- Direct `REAL` comparisons using `=` or `<>`
- Structural issues:
  - `EXIT` inside loops
  - `RETURN` in the middle of logic
  - empty `ELSE` branches
  - empty `CASE` branches
  - assignments made only in one `IF` branch
  - magic numbers in `IF` / `ELSIF` conditions

## Build

The project is a Visual Studio C++ project.

Open `easy-iec-checker.slnx` in Visual Studio and build the project.

To build from the command line, open **Developer PowerShell for Visual Studio** in the repository folder and run:

```powershell
msbuild .\easy-iec-checker.vcxproj /p:Configuration=Release /p:Platform=x64
```

Do not run this command from a regular PowerShell unless `msbuild` is already in `PATH`.

After a `Release|x64` build, the executable is usually created as:

```text
.\x64\Release\easy-iec-checker.exe
```

If you build another configuration, the path may be different. You can find the executable with:

```powershell
Get-ChildItem -Recurse -Filter easy-iec-checker.exe
```

## Usage

Run the checker from the repository root:

```powershell
.\x64\Release\easy-iec-checker.exe
```

By default, it uses:

```text
.\resource\_make\make.mk
```

You can also pass the makefile path explicitly:

```powershell
.\x64\Release\easy-iec-checker.exe .\resource\_make\make.mk
```

or:

```powershell
.\x64\Release\easy-iec-checker.exe --makefile .\resource\_make\make.mk
.\x64\Release\easy-iec-checker.exe -m .\resource\_make\make.mk
```

## Makefile Format

The checker scans the provided `make.mk` and extracts `.st` / `.sts` file paths from it. Paths may be written across multiple lines with `\`.

Example:

```makefile
FILES = \
    main.st \
    LogicalProgram.st \
    Regul/RegulProgram.st
```

Relative paths are resolved against the project/resource location used by the analyzer.

## Example Output

```text
resource/main.st:42:12: error: forbidden conversion REAL_TO_INT
    value := REAL_TO_INT(sensorValue);
             ^
easy-iec-checker: found 1 error(s)
```

## Exit Codes

| Code | Meaning |
| --- | --- |
| `0` | Analysis completed successfully, no issues found |
| `1` | Analysis completed, issues found |
| `2` | Fatal runtime error, for example missing makefile |

## Adding New Rules

Rules implement the `IRule` interface:

```cpp
class IRule {
public:
    virtual ~IRule() = default;
    virtual std::string name() const = 0;
    virtual std::string description() const = 0;
    virtual int checkFile(const std::filesystem::path& file) const = 0;
};
```

Simple text-based bans can be added to the `ForbiddenPatternRule` configuration in `StaticAnalyzer::registerRules()`. More complex checks should be implemented as a separate rule class and registered in the same method.
