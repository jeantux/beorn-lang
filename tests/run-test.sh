#!/bin/bash
set -e

export MODE=test

echo "----------------------------------------"
echo -e "\n\033[32mRunning tests!\033[0m"
echo "----------------------------------------"

echo -e "\n\033[43mExpecting errors .....\033[0m"
./bin/drax ./tests/expressions.dx
echo -e "\n\033[43m----------------------\033[0m"

./bin/drax ./tests/print.dx

./bin/drax ./tests/number.dx

./bin/drax ./tests/functions.dx

./bin/drax ./tests/string.dx

./bin/drax ./tests/frames.dx

./bin/drax ./tests/large_frames.dx

./bin/drax ./tests/os.dx

./bin/drax ./tests/list.dx

./bin/drax ./tests/tensor.dx

./bin/drax ./tests/import.dx

./bin/drax ./tests/math.dx

./bin/drax ./tests/lambdas.dx

./bin/drax ./tests/lambdas-nested.dx

./bin/drax ./tests/pipe.dx

./bin/drax ./tests/time.dx

./bin/drax ./tests/gc.dx

echo "----------------------------------------"

echo -e "\n\033[32mAll tests were executed successfully!\033[0m"

# ./bin/drax ./tests/http/http-only-response-text-plain.dx
