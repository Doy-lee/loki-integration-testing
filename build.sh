ctags -R --c++-kinds=+p --fields=+iaS --extras=+q
mkdir -p bin/output
g++ loki_integration_tests.cpp loki_test_cases.cpp -std=c++11 -Wall -lpthread -lrt -g -o bin/integration_test
