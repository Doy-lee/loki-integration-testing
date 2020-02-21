ctags -R --c++-kinds=+p --fields=+iaS
mkdir -p bin/output
g++ loki_integration_tests.cpp -std=c++17 -Wall -lpthread -lrt -g -o bin/integration_test
