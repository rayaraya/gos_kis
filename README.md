To compile use: g++ -std=c++17 -g main.cpp 

To create diff file: ./a.out -d -f test1.txt -s test2.txt

To apply patch: ./a.out -u -f test1.txt -s patch 

Diff is saved to "patch" file by default, result in "result"
