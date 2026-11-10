"""Small, idempotent patch to the FreeInk SDK checkout (like CrossPoint's
scripts/patch_sdfat.py). Runs before every PlatformIO build (extra_scripts)
and from tools/reader-sim/build.sh.

FreeInkBook's Small (PSRAM-less) profile caps a page at 304 text runs. A
justified page at our 21 px body size has ~400 (every word is a run), so
dense pages were cut early and paragraphs continued on a near-empty page.
The patch makes the cap a build flag (FREEINK_BOOK_MAX_RUNS_PER_PAGE),
default unchanged.
"""
import os
import sys

OLD = "constexpr uint16_t kMaxRunsPerPage = 304;"
NEW = (
    "#ifndef FREEINK_BOOK_MAX_RUNS_PER_PAGE  // x3-firmware patch (tools/patch_sdk.py)\n"
    "#define FREEINK_BOOK_MAX_RUNS_PER_PAGE 304\n"
    "#endif\n"
    "constexpr uint16_t kMaxRunsPerPage = FREEINK_BOOK_MAX_RUNS_PER_PAGE;"
)


def patch(project_dir):
    path = os.path.join(project_dir, "freeink-sdk", "libs", "book", "FreeInkBook", "src", "layout", "ChapterLayout.cpp")
    if not os.path.exists(path):
        print("patch_sdk: %s not found (clone the SDK first)" % path)
        return
    text = open(path, encoding="utf-8").read()
    if "FREEINK_BOOK_MAX_RUNS_PER_PAGE" in text:
        return  # already patched
    if text.count(OLD) != 1:
        sys.exit("patch_sdk: expected one '%s' in %s; SDK version changed?" % (OLD, path))
    open(path, "w", encoding="utf-8").write(text.replace(OLD, NEW))
    print("patch_sdk: patched %s" % path)


try:
    Import("env")  # noqa: F821 - provided by PlatformIO/SCons
    patch(env["PROJECT_DIR"])  # noqa: F821
except NameError:
    patch(sys.argv[1] if len(sys.argv) > 1 else os.getcwd())
