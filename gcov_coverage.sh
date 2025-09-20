mkdir -p coverage
gcovr \
    --root . \
    --filter 'src/.*' \
    --exclude 'external/.*' \
    --exclude '/usr/include/.*' \
    --html \
    --html-details \
    -o coverage/coverage.html \
    --gcov-ignore-parse-errors=negative_hits.warn_once_per_file \
    --gcov-ignore-errors=no_working_dir_found
echo "Find coverage report at: $(realpath ./coverage/coverage.html)"