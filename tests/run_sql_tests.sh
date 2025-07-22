#!/bin/bash
# Run all SQL test files and check for errors

set -e  # Exit on error

echo "Running SQL tests..."
echo "==================="

# Counter for test results
TOTAL=0
PASSED=0
FAILED=0

# Run each SQL test file
for test_file in tests/sql/*.sql; do
    if [ -f "$test_file" ]; then
        TOTAL=$((TOTAL + 1))
        TEST_NAME=$(basename "$test_file")
        echo -n "Running $TEST_NAME... "
        
        # Run the test and capture output and exit code
        if sqlite3 :memory: < "$test_file" > /tmp/sql_test_output.txt 2>&1; then
            echo "PASSED"
            PASSED=$((PASSED + 1))
        else
            echo "FAILED"
            FAILED=$((FAILED + 1))
            echo "Error output:"
            cat /tmp/sql_test_output.txt
            echo "---"
        fi
    fi
done

echo
echo "SQL Test Summary:"
echo "================"
echo "Total:  $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"

# Clean up
rm -f /tmp/sql_test_output.txt

# Exit with error if any tests failed
if [ $FAILED -gt 0 ]; then
    exit 1
fi

exit 0