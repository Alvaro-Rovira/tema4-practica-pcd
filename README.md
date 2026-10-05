# practica tema 4 - race condition

programa en c++17 con cmake que provoca una race condition de verdad y la analiza. 4 hilos
suman 1 a una variable global `contador` 1.000.000 de veces cada uno, sin proteger nada.
tendria que dar 4.000.000 y no lo da. despues el mismo programa repite todo con
`std::atomic<long long>` para comparar.

## compilar y ejecutar

    cmake -S . -B build
    cmake --build build
    ./build/practica4

si no le dices el tipo de build compila en debug, que es donde se ve el problema. para
release:

    cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
    cmake --build build-release
    ./build-release/practica4

se le pueden pasar dos numeros, las iteraciones por hilo y cuantas veces repetir. por defecto
son 1000000 y 10. saca una linea por ejecucion y al final cuantas han salido bien y cual ha sido
la peor. si repites muchas veces, con `grep` te quedas solo con el resumen:

    ./build/practica4 100 1000 | grep "salen bien"

desde clion basta con abrir la carpeta, el perfil debug viene por defecto.

## el codigo

todo esta en `src/main.cpp`:

- `contador` es la variable del enunciado, un `long long` global sin proteccion.
- `incrementar` es lo que hace cada hilo: un bucle que hace `contador++` tantas veces como
  iteraciones le pasen.
- `ejecutar` pone el contador a 0, lanza los 4 hilos, hace `join` y devuelve lo que ha salido.
- `probar` repite `ejecutar` varias veces, compara cada resultado con lo que tendria que dar y
  se guarda cuantas salen bien y la peor.
- `contador_atomico` e `incrementar_atomico` son lo mismo pero con `std::atomic<long long>`,
  para la extension.

las salidas de abajo son de ejecuciones reales en mi mac, un m4 con 10 nucleos. en otro
ordenador saldran otros numeros, y en el mismo tambien si lo vuelves a ejecutar.

## la race condition

en debug, con 1.000.000 de iteraciones y 10 repeticiones:

    4 hilos, 1000000 iteraciones cada uno, tendria que dar 4000000

    sin proteccion
    ejecucion 1: 1052304, se pierden 2947696
    ejecucion 2: 1028984, se pierden 2971016
    ejecucion 3: 1047518, se pierden 2952482
    ejecucion 4: 1042454, se pierden 2957546
    ejecucion 5: 1018721, se pierden 2981279
    ejecucion 6: 1014573, se pierden 2985427
    ejecucion 7: 1017907, se pierden 2982093
    ejecucion 8: 1026481, se pierden 2973519
    ejecucion 9: 1014085, se pierden 2985915
    ejecucion 10: 1031927, se pierden 2968073
    salen bien 0 de 10, el peor da 1014085

ninguna llega a 4.000.000, y ni se acercan: salen alrededor de 1.000.000, o sea que se pierden
casi 3 de cada 4 incrementos. yo esperaba que se perdieran unos pocos, no tantos. los 4 hilos
van de verdad a la vez, cada uno en un nucleo, y se estan pisando todo el rato.

lo otro es que cada ejecucion da un numero distinto, de 1.014.085 a 1.052.304. el codigo es el
mismo y la entrada tambien, lo unico que cambia es como reparte el sistema operativo los hilos.
un programa correcto daria siempre 4.000.000, asi que solo con ver que el resultado baila ya
sabes que algo va mal.

## por que pasa: load, add y store

`contador++` en c++ parece una sola operacion, pero el procesador no lo hace de una vez. con
`otool -tvV build/practica4` se ve lo que ha generado el compilador para el bucle de
`incrementar`. la parte que toca `contador` es esta:

    ldr   x8, [x9]
    add   x8, x8, #0x1
    str   x8, [x9]

`ldr` es el load, lee `contador` de memoria y lo mete en el registro `x8`. `add` le suma 1
dentro del registro. `str` es el store, escribe el registro otra vez en memoria. el bucle tiene
mas loads y stores, pero son de `i`, que esta en la pila de cada hilo y no lo comparte nadie.

el registro es de cada hilo, pero `contador` es la misma posicion de memoria para todos. si dos
hilos se intercalan asi:

    paso   hilo a                hilo b                contador en memoria
    1      ldr: x8 = 100                               100
    2                            ldr: x8 = 100         100
    3      add: x8 = 101                               100
    4                            add: x8 = 101         100
    5      str: escribe 101                            101
    6                            str: escribe 101      101

han hecho dos incrementos y `contador` solo ha subido uno. el hilo b leyo antes de que a
escribiera, trabajo con un valor viejo y al escribir machaco lo que habia hecho a. basta con que
un hilo lea entre el load y el store de otro.

hay un caso todavia peor. un hilo hace el load y el sistema lo para justo ahi. mientras tanto
los otros hacen miles de incrementos. cuando vuelve, suma 1 a su valor viejo, lo escribe, y
todos esos miles desaparecen de golpe. esto se ve muy claro en las pruebas que vienen ahora.

con 4 hilos y 4.000.000 de incrementos estos cruces pasan millones de veces, y por eso el
resultado se queda tan lejos.

## y con pocas iteraciones

lo he probado con distintos numeros de iteraciones, 1000 repeticiones cada uno, en debug:

    iteraciones   tendria que dar   salen bien   el peor da
              1                 4    1000/1000            4
             10                40     999/1000           30
            100               400     994/1000          300
           1000              4000     874/1000         3000
          10000             40000       0/1000        10000
         100000            400000       0/1000       101984
        1000000           4000000       0/1000       976218

con pocas iteraciones casi siempre sale bien. crear un hilo cuesta bastante mas que hacer 100
sumas, asi que cuando main lanza el segundo hilo el primero ya ha terminado. acaban yendo uno
detras de otro, casi nunca coinciden y no hay nada que pisar. cuantas mas iteraciones, mas rato
estan los hilos a la vez y mas falla, hasta que a partir de 10.000 ya no sale bien casi ninguna.
lo repeti tres veces mas con 1 y con 10.000 iteraciones y sale parecido: con 1 siempre bien, con
10.000 como mucho 2 de 1000.

aun asi, con pocas iteraciones falla alguna vez. con 10 falla 1 de 1000 y con 100 fallan 6. y si
te fijas en el peor resultado: 30 de 40, 300 de 400, 3000 de 4000. se pierde justo lo que hace un
hilo entero, que es el caso peor de antes: un hilo lee, otro hace todas sus sumas en medio y el
primero escribe encima. con 10.000 el peor da 10.000 justos, o sea que de los 4 hilos solo ha
contado el trabajo de uno.

para mi esto es lo mas importante de la practica. si solo hubiera probado con 10 o 100
iteraciones habria dicho que el codigo esta bien. el fallo esta ahi igual, solo que sale poco, y
eso es peor, porque pasa las pruebas y luego falla cuando no te lo esperas.

## extension: std::atomic<long long>

atomico quiere decir que no se puede partir: la operacion se hace entera de una vez y ningun otro
hilo puede meterse a mitad. `std::atomic` es el tipo que tiene c++ para eso y es lo que pide el
enunciado. la unica diferencia con lo de antes es el tipo de la variable: `contador_atomico` es un
`std::atomic<long long>` y el bucle es igual, `contador_atomico++`. en debug:

    con atomico
    ejecucion 1: 4000000, se pierden 0
    ejecucion 2: 4000000, se pierden 0
    ...
    ejecucion 10: 4000000, se pierden 0
    salen bien 10 de 10, el peor da 4000000

sale 4.000.000 siempre. en todas las pruebas de la tabla de antes la version atomica salio bien
1000 de 1000, y en release tambien, incluso repitiendo 10.000 veces.

funciona porque el `++` de un atomico ya no son tres instrucciones sueltas, es una sola. en el
ensamblador de release se ve dentro del bucle, en debug esta dentro de una funcion de la
libreria estandar a la que se llama en cada vuelta:

    ldaddal  x9, x10, [x8]

`ldaddal` lee, suma y escribe en memoria en una sola instruccion, y ningun otro nucleo puede
meterse en medio. la `a` y la `l` del final son acquire y release, que es como cumple arm el
orden por defecto de los atomicos en c++.

lo malo es que va bastante mas lento. lo cronometre con 100 repeticiones en debug: la parte sin
proteccion tarda 0.2 s y la del atomico 6.2 s, unas 30 veces mas. los 4 nucleos quieren la misma
variable a la vez y cada incremento tiene que esperar su turno. la version sin proteccion es mucho
mas rapida, pero da un resultado que esta mal, asi que tampoco sirve de mucho.

## extension 2: release

debug y release son dos formas de compilar el mismo codigo. debug es para cuando estas
programando: no optimiza nada y mete la informacion que necesita el depurador, asi que el codigo
maquina se parece mucho a lo que has escrito. release es para entregar el programa: cmake le
pasa `-O3 -DNDEBUG` al compilador, que optimiza todo lo que puede y quita los `assert`. en debug
solo le pasa `-g`.

lo mismo de antes compilado en release:

    sin proteccion
    ejecucion 1: 4000000, se pierden 0
    ejecucion 2: 4000000, se pierden 0
    ejecucion 3: 4000000, se pierden 0
    ejecucion 4: 4000000, se pierden 0
    ejecucion 5: 4000000, se pierden 0
    ejecucion 6: 4000000, se pierden 0
    ejecucion 7: 4000000, se pierden 0
    ejecucion 8: 4000000, se pierden 0
    ejecucion 9: 4000000, se pierden 0
    ejecucion 10: 4000000, se pierden 0
    salen bien 10 de 10, el peor da 4000000

la version sin proteccion da 4.000.000 en las 10, y sale al instante. a primera vista parece que
release arregla la race condition. mirando el ensamblador se ve lo que ha pasado de verdad:

    cmp   x0, #0x1
    b.lt  0x100000d78
    adrp  x8, 8
    ldr   x9, [x8]
    add   x9, x9, x0
    str   x9, [x8]
    ret

el bucle ya no esta. `x0` es el numero de iteraciones, y el compilador lo suma de una vez en
lugar de sumar 1 un millon de veces. puede hacerlo porque en c++ una data race es comportamiento
indefinido, asi que el compilador da por hecho que ningun otro hilo toca `contador` mientras
tanto. ahora cada hilo hace un solo load, un solo add y un solo store. la race sigue ahi, pero
dura tres instrucciones en lugar de un millon de vueltas, y como lanzar cada hilo cuesta mucho
mas que eso, casi nunca coinciden.

casi nunca no es nunca. lo deje repitiendo 10.000 veces y me quede solo con las que fallan:

    ./build-release/practica4 1000000 10000 | grep -v "se pierden 0$"

    4 hilos, 1000000 iteraciones cada uno, tendria que dar 4000000

    sin proteccion
    ejecucion 1198: 3000000, se pierden 1000000
    ejecucion 3097: 3000000, se pierden 1000000
    ejecucion 3778: 3000000, se pierden 1000000
    ejecucion 5603: 3000000, se pierden 1000000
    ejecucion 6553: 3000000, se pierden 1000000
    ejecucion 6757: 3000000, se pierden 1000000
    ejecucion 8529: 3000000, se pierden 1000000
    ejecucion 9111: 3000000, se pierden 1000000
    ejecucion 9766: 3000000, se pierden 1000000
    ejecucion 9993: 3000000, se pierden 1000000
    salen bien 9990 de 10000, el peor da 3000000

    con atomico
    salen bien 10000 de 10000, el peor da 4000000

10 fallos de 10.000, y todos de 3.000.000 exactos. dos hilos hicieron su load a la vez, leyeron
el mismo valor y uno machaco al otro. es la misma tabla de antes, pero ahora cada store que se
pierde se lleva un millon de incrementos de golpe.

el atomico en release sigue saliendo bien. el bucle se queda y hay un `ldaddal` por vuelta,
porque el compilador no puede juntar operaciones atomicas en una. va mas rapido que en debug,
2.5 s en vez de 6.2 con 100 repeticiones, porque en debug cada `++` era una llamada a una
funcion y ahora la instruccion esta metida directamente en el bucle.

## lo que saco de todo esto

el resultado de una race condition no depende del codigo, depende de como se cruzan los hilos en
esa ejecucion concreta. por eso cambia cada vez, y por eso que salga bien una vez no demuestra
nada, igual que pasaba en el tema 3.

el fallo se puede esconder sin estar arreglado. con pocas iteraciones o compilando en release el
programa da bien casi siempre, pero el bug es el mismo. y fallar una vez de cada 1000 me parece
peor que fallar siempre, porque no lo ves venir.

release no arregla nada. el compilador ha cambiado el bucle porque el codigo tiene una data race
y eso le deja hacer lo que quiera. con otro compilador, otras opciones o un bucle un poco
distinto podria volver a fallar como en debug.

lo que lo arregla de verdad es `std::atomic`, o un `std::mutex` alrededor del incremento. con el
atomico sale 4.000.000 siempre, en debug y en release, con muchas o pocas iteraciones. es
bastante mas lento, pero si varios hilos escriben en la misma variable es lo que toca pagar.
