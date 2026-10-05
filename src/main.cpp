#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
const int num_hilos = 4;
long long contador = 0;
std::atomic<long long> contador_atomico = 0;
void incrementar(long long iteraciones) {
    for (long long i = 0; i < iteraciones; i++) {
        contador++;
    }
}
void incrementar_atomico(long long iteraciones) {
    for (long long i = 0; i < iteraciones; i++) {
        contador_atomico++;
    }
}
long long ejecutar(bool atomico, long long iteraciones) {
    contador = 0;
    contador_atomico = 0;
    std::vector<std::thread> hilos;
    for (int i = 0; i < num_hilos; i++) {
        if (atomico) {
            hilos.push_back(std::thread(incrementar_atomico, iteraciones));
        } else {
            hilos.push_back(std::thread(incrementar, iteraciones));
        }
    }
    for (int i = 0; i < num_hilos; i++) {
        hilos[i].join();
    }
    if (atomico) {
        return contador_atomico;
    }
    return contador;
}
void probar(bool atomico, long long iteraciones, int repeticiones) {
    long long esperado = num_hilos * iteraciones;
    int bien = 0;
    long long peor = esperado;
    for (int r = 1; r <= repeticiones; r++) {
        long long res = ejecutar(atomico, iteraciones);
        std::cout << "ejecucion " << r << ": " << res << ", se pierden " << esperado - res << "\n";
        if (res == esperado) {
            bien++;
        }
        if (res < peor) {
            peor = res;
        }
    }
    std::cout << "salen bien " << bien << " de " << repeticiones << ", el peor da " << peor << "\n";
}
int main(int argc, char* argv[])
{
    long long iteraciones = 1000000;
    int repeticiones = 10;
    if (argc > 1) {
        iteraciones = atoll(argv[1]);
    }
    if (argc > 2) {
        repeticiones = atoi(argv[2]);
    }
    std::cout << num_hilos << " hilos, " << iteraciones << " iteraciones cada uno, tendria que dar "
        << num_hilos * iteraciones << "\n";
    std::cout << "\nsin proteccion\n";
    probar(false, iteraciones, repeticiones);
    std::cout << "\ncon atomico\n";
    probar(true, iteraciones, repeticiones);
    return 0;
}
