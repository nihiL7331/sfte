#!/bin/bash
# This script is used for CI GitHub action.
#
# It's main point is to confirm that there's no:
# - dead code in macro branches after variable renames,
# - macro-specific code not wrapped in the specific macro.
#
# It works by stripping the section from 
# '// >>>CONFIGURATION'
# line to
# '// >>modifiers and shortcuts macros'
# line and grabbing all '_SFTE_ENSURE_RANGE' calls.
# Then, it runs the compilation for every option with their minimum and maximum values.
#
# WARN:
# This might not catch any more complex statements, e.g. when one macro is dependent on another.

set -e

cc nob.c -o nob

# _SFTE_ENSURE_RANGE(macro, min, max)    => RANGE|macro|min |max
# _SFTE_ENSURE_DEPS(macro, dep1 && dep2) =>  DEP |macro|dep1|dep2
EXTRACTED=$(awk '
    /\/\/ >>>CONFIGURATION/,/\/\/ >>>PUBLIC API/ {
        if ($0 ~ /^_SFTE_ENSURE_RANGE/) {
            line = $0
            sub(/^[^(]+\(/, "", line) # strip to first (
            sub(/\).*$/, "", line)    # strip from last ) to end
            split(line, p, ",")

            for(i=1; i<=3; i++) { # strip from whitespaces
                sub(/^[ \t]+/, "", p[i])
                sub(/[ \t]+$/, "", p[i])
            }

            if (p[1] != "" && p[2] != "" && p[3] != "") {
                print "RANGE|" p[1] "|" p[2] "|" p[3]
            }
        }
        
        if ($0 ~ /^_SFTE_ENSURE_DEPS/) {
            line = $0
            sub(/^[^(]+\(/, "", line) # strip to first (
            sub(/\).*$/, "", line)    # strip from last ) to end
            split(line, p, ",")
            macro = p[1]              # extract macro
            sub(/^[ \t]+/, "", macro)
            sub(/[ \t]+$/, "", macro)
        
            sub(/^[^,]+,/, "", line)  # strip macro
            deps = ""
            while (match(line, /SFTE_[A-Z0-9_]+/)) {
                deps = deps " " substr(line, RSTART, RLENGTH)
                line = substr(line, RSTART + RLENGTH)
            }
            print "DEP|" macro "|" deps
        }
    }' sfte_dev.h)

declare -a RANGES
declare -A FORWARD_DEPS # dependencies required to enable a macro
declare -A REVERSE_DEPS # macros that break if a dependency is disabled

while IFS='|' read -r TYPE COL1 COL2 COL3; do
    if [[ "$TYPE" == "RANGE" ]]; then
        RANGES+=("$COL1|$COL2|$COL3")
    elif [[ "$TYPE" == "DEP" ]]; then
        MACRO="$COL1"
        DEPS="$COL2"
        FORWARD_DEPS["$MACRO"]="$DEPS"
        for DEP in $DEPS; do
            REVERSE_DEPS["$DEP"]+="$MACRO "
        done
    fi
done <<< "$EXTRACTED"

cp sfte_dev.h sfte_dev.h.bak
cp sfte.h sfte.h.bak
trap 'mv sfte_dev.h.bak sfte_dev.h; mv sfte.h.bak sfte.h; rm -f fuzz_macros.log' EXIT

for ENTRY in "${RANGES[@]}"; do
    IFS='|' read -r MACRO MIN MAX <<< "$ENTRY"

    for VAL in "$MIN" "$MAX"; do
        echo "$MACRO = $VAL"
        sed -i -E "s/^(#define[[:space:]]+$MACRO[[:space:]]+).*/\1$VAL/" sfte_dev.h
        if [[ "$VAL" == "$MAX" && -n "${FORWARD_DEPS[$MACRO]}" ]]; then
            # if turning on, turn on dependencies
            for DEP in ${FORWARD_DEPS[$MACRO]}; do
                echo "enabling dependency: $DEP = 1"
                sed -i -E "s/^(#define[[:space:]]+$DEP[[:space:]]+).*/\11/" sfte_dev.h
            done
        elif [[ "$VAL" == "$MIN" && -n "${REVERSE_DEPS[$MACRO]}" ]]; then
            # if turning off, turn off anything that relies on it
            for REV in ${REVERSE_DEPS[$MACRO]}; do
                echo "disabling dependency: $REV = 0"
                sed -i -E "s/^(#define[[:space:]]+$REV[[:space:]]+).*/\10/" sfte_dev.h
            done
        fi

        if ! ./nob dev &> fuzz_macros.log; then
            echo "build failed: $MACRO=$VAL"
            cat fuzz_macros.log
            exit 1
        fi

        cp sfte_dev.h.bak sfte_dev.h
    done
done
