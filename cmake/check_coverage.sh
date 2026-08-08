#!/bin/sh
# Check coverage against threshold
THRESHOLD="${1:-80}"
INFO_FILE="coverage.info"

if [ ! -f "$INFO_FILE" ]; then
    echo "ERROR: coverage.info not found. Run coverage target first."
    exit 1
fi

# Use lcov --summary to get coverage percentage
SUMMARY=$(lcov --summary "$INFO_FILE" 2>/dev/null)
# Parse "lines.......: 84.5% (6408 of 7579 lines)"
PCT=$(echo "$SUMMARY" | grep 'lines' | grep -oE '[0-9]+\.[0-9]+%' | head -1 | tr -d '%')

if [ -z "$PCT" ]; then
    echo "ERROR: Could not parse coverage percentage"
    echo "$SUMMARY"
    exit 1
fi

echo "Coverage: ${PCT}% (threshold: ${THRESHOLD}%)"

BELOW=$(awk -v pct="$PCT" -v th="$THRESHOLD" 'BEGIN { print (pct+0 < th+0) ? "1" : "0" }')
if [ "$BELOW" = "1" ]; then
    echo "FAIL: Coverage ${PCT}% is below threshold ${THRESHOLD}%"
    exit 1
fi

echo "PASS: Coverage ${PCT}% meets threshold ${THRESHOLD}%"
exit 0
