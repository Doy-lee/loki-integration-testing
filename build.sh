ctags -R --c++-kinds=+p --fields=+iaS --extras=+q
mkdir -p bin
g++ main.cpp test_cases.cpp -std=c++11 -lrt -g -o bin/integration_test
