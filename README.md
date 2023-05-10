# Proyecto #2 - Programación Paralela con MPI

# Cracking DES Encryption with MPI

Este proyecto explora tres enfoques diferentes para romper el cifrado DES utilizando el paso de mensajes de la Interfaz de Paso de Mensajes (MPI) en C.

### Objetivos
- Implementa y diseña programas para la paralelización de procesos con memoria distribuida usando OpenMPI.

- Optimizar el uso de recursos distribuidos y mejorar el speedup de un programa paralelo.

- Descubrir la llave privada usada para cifrar un texto, usando el método de fuerza bruta (brute force).

- Comprender y Analizar el comportamiento del speedup de forma estadística.
## Versión Naive

La versión naive divide el espacio de búsqueda de claves por igual entre los procesos disponibles. Cada proceso busca a través de su sección de claves asignada de manera secuencial hasta que se encuentra la clave correcta.

## Versión con Colas

La versión con colas utiliza una estrategia de asignación dinámica de tareas. Cuando un proceso termina de buscar a través de su sección de claves asignada, solicita una nueva sección de claves al proceso maestro. Esto puede resultar en una distribución más equilibrada de la carga de trabajo, especialmente si el tiempo requerido para buscar a través de una sección de claves varía.

## Versión con Búsqueda Bidireccional

La versión con búsqueda bidireccional implementa una estrategia de búsqueda de encuentro en el medio. Los procesos con identificadores pares buscan de forma ascendente desde el inicio de su sección asignada, mientras que los procesos con identificadores impares buscan de forma descendente desde el final de su sección asignada. Esto puede reducir a la mitad el tiempo necesario para encontrar la clave correcta en el peor de los casos.

## Compilación y Ejecución

Para compilar cualquiera de los programas, puedes usar el compilador de MPI. Aquí te dejo un ejemplo de cómo hacerlo:

```bash
mpicc bruteForceBi.c -o bruteforceBi -lcrypto -lssl
mpicc bruteforceNaive.c -o bruteForceNaive -lcrypto -lssl
mpicc bruteForceColas.c -o bruteforceColas -lcrypto -lssl
```

Para ejecutarlos:

```bash
mpirun -np 4 ./bruteforceBi
mpirun -np 4 ./bruteForceNaive
mpirun -np 4 ./bruteforceColas
```
_En estos ejemplos, el número 4 indica que se deben usar 4 procesos. Puedes reemplazar este número por la cantidad de procesos que desees utilizar._

### Authors

- [@JavRamCos](https://github.com/JavRamCos)
- [@cesarvin](https://github.com/cesarvin)
- [@AndresQuinto5](https://github.com/AndresQuinto5)