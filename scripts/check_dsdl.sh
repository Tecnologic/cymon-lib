#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
#
# Validates dsdl/cymon, checks that the C serialization Nunavut generates from
# it (the way UNIMOC generates it) compiles, and compiles the service handler
# example of docs/integration.md against it. Needs nunavut
# (pip install nunavut), a C compiler and a C++20 compiler.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="$(mktemp -d)"
trap 'rm -rf "${out}"' EXIT

python3 -m nunavut -O "${out}" -l c --target-endianness little "${root}/dsdl/cymon"

for header in "${out}"/cymon/*.h; do
  printf '#include "%s"\n' "${header#"${out}/"}" |
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"${out}" -x c -fsyntax-only -
done
# The first C++ block of the integration guide uses the generated types.
python3 - "${root}/docs/integration.md" > "${out}/example.cpp" <<'PY'
import re, sys
text = open(sys.argv[1]).read()
print("#include <algorithm>\n#include <cstddef>\n#include <span>")
print(re.findall(r"```cpp\n(.*?)```", text, re.S)[0])
PY
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -I"${root}/include" -I"${out}" -fsyntax-only "${out}/example.cpp"

echo "DSDL ok: $(find "${out}/cymon" -name '*.h' | wc -l) types"
