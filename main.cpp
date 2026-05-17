#include <iostream>
#include "fabrica.hpp"
 
int main() {
    int n, m;
    std::cin >> n >> m;
    std::cin.ignore();
 
    SistemaCentral central(n, m);
    central.executar();
 
    return 0;
}