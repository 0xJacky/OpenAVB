from pathlib import Path
import re


root = Path(__file__).resolve().parent.parent
owner_roots = {"crypto", "devices", "image", "platform", "protocol", "runtime"}
source_suffixes = {".c", ".S"}


def require_owner_path(path: Path, tree: str) -> None:
    relative = path.relative_to(root / tree)
    assert len(relative.parts) >= 2, f"{tree} file lacks owner directory: {relative}"
    assert relative.parts[0] in owner_roots, (
        f"unexpected {tree} owner {relative.parts[0]}: {relative}"
    )


source_files = sorted(
    path for path in (root / "src").rglob("*")
    if path.is_file() and path.suffix in source_suffixes
)
test_files = sorted((root / "tests").rglob("*_tests.c"))
primary_headers = sorted(
    path for path in (root / "include" / "avp").rglob("*.h")
    if len(path.relative_to(root / "include" / "avp").parts) >= 2
)
forwarding_headers = sorted((root / "include" / "avp").glob("*.h"))

assert source_files, "OpenAVB source tree is empty"
assert test_files, "OpenAVB test tree is empty"
assert primary_headers, "OpenAVB primary header tree is empty"
assert forwarding_headers, "OpenAVB forwarding header set is empty"

for source in source_files:
    require_owner_path(source, "src")
for test in test_files:
    require_owner_path(test, "tests")
for header in primary_headers:
    require_owner_path(header, "include/avp")

source_names: dict[str, Path] = {}
for source in source_files:
    previous = source_names.setdefault(source.name, source)
    assert previous == source, (
        f"duplicate source basename {source.name}: "
        f"{previous.relative_to(root)}, {source.relative_to(root)}"
    )

forward_pattern = re.compile(r'^#include "(avp/[^\"]+/[^\"]+)"$', re.MULTILINE)
for header in forwarding_headers:
    matches = forward_pattern.findall(header.read_text())
    assert len(matches) == 1, (
        f"compatibility header must forward to one primary header: "
        f"{header.relative_to(root)}"
    )
    target = root / "include" / matches[0]
    assert target.is_file(), (
        f"forwarding target is missing: {header.relative_to(root)} -> "
        f"{target.relative_to(root)}"
    )
    assert target.parent.name in owner_roots or (
        target.relative_to(root / "include" / "avp").parts[0] in owner_roots
    ), f"forwarding target lacks subsystem ownership: {target.relative_to(root)}"

print(
    "OpenAVB layout verified: "
    f"{len(source_files)} sources, {len(test_files)} tests, "
    f"{len(primary_headers)} primary headers, "
    f"{len(forwarding_headers)} forwarding headers"
)
