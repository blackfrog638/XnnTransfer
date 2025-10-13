# Compiler Requirements

## Overview

XnnTransfer requires **full C++20 support**, including:
- Concepts (`requires` clauses)
- Coroutines (`co_await`, `co_return`)
- Standard library features (`std::jthread`, `std::format`, etc.)

## Supported Compilers

### ✅ Windows
- **MSVC 2022** (v17.0+) - Full C++20 support
- **Clang-cl** (LLVM 14+)
- **MinGW GCC** (12+)

### ✅ Linux
- **GCC** 11+ (Recommended: GCC 12 or 13)
- **Clang** 14+ with libc++

### ✅ macOS
**⚠️ Important:** Apple Clang does NOT fully support C++20 concepts!

#### Recommended Solution: Homebrew GCC
```bash
brew install gcc@13
export CC=$(brew --prefix gcc@13)/bin/gcc-13
export CXX=$(brew --prefix gcc@13)/bin/g++-13
xmake f -c
xmake build
```

## CI/CD Configuration

Our GitHub Actions automatically installs GCC 13 on macOS runners:

```yaml
- name: Install GCC on macOS
  if: runner.os == 'macOS'
  run: |
    brew install gcc@13
    echo "CC=$(brew --prefix gcc@13)/bin/gcc-13" >> $GITHUB_ENV
    echo "CXX=$(brew --prefix gcc@13)/bin/g++-13" >> $GITHUB_ENV
```

## Why Not Apple Clang?

Apple Clang (based on LLVM but modified by Apple) lags behind mainstream LLVM:

| Feature | Apple Clang 15 | GCC 13 | LLVM 17 |
|---------|----------------|--------|---------|
| Concepts (`requires`) | ❌ Partial | ✅ Full | ✅ Full |
| Coroutines | ✅ Yes | ✅ Yes | ✅ Yes |
| `std::jthread` | ❌ No | ✅ Yes | ✅ Yes |
| `std::format` | ❌ No | ✅ Yes | ✅ Yes |

## Build Tips

### Force Specific Compiler (XMake)
```bash
# Use GCC
xmake f --toolchain=gcc
xmake build

# Use LLVM Clang
xmake f --toolchain=clang
xmake build
```

### Check Current Compiler
```bash
xmake f --verbose
```

## Troubleshooting

### macOS: "concepts not supported"
```
error: 'requires' keyword not supported
```
**Solution:** Install and use GCC/LLVM via Homebrew (see above).

### Linux: Missing `<concepts>` header
```
fatal error: 'concepts' file not found
```
**Solution:** Update GCC to 11+ or Clang to 14+.

### Windows: MSVC older than 2022
```
error C7555: use of designated initializers requires '/std:c++20'
```
**Solution:** Update to Visual Studio 2022 or use `/std:c++20` flag.

## References

- [C++20 Compiler Support](https://en.cppreference.com/w/cpp/compiler_support/20)
- [XMake Toolchains](https://xmake.io/#/guide/configuration?id=toolchains)
- [ASIO C++20 Coroutines](https://think-async.com/Asio/asio-1.28.0/doc/asio/overview/cpp2011/concepts.html)
