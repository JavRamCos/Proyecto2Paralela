//bruteforceNaive.c
//Tambien cifra un texto cualquiera con un key arbitrario.
//OJO: asegurarse que la palabra a buscar sea lo suficientemente grande
//  evitando falsas soluciones ya que sera muy improbable que tal palabra suceda de
//  forma pseudoaleatoria en el descifrado.
//>> mpicc bruteforceNaive.c -o desBrute -lcrypto
//>> mpirun -np <N> desBrute

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <mpi.h>
#include <unistd.h>
#include <openssl/des.h>

#define INFILE "Proyecto2Paralela/input_file.txt"

// Leer archivo y guardar caracteres
int ReadFile(char* dest) {
  FILE* file;
  long size;
  char* buffer;
  file = fopen(INFILE,"rb");
  if(file == NULL) {
    printf("Error reading text file ...\n");
    return 0;
  }
  // Obtener informacion del archivo de texto
  fseek(file,0L,SEEK_END);
  size = ftell(file);
  rewind(file);
  // Alojando memoria para lectura de archivo
  buffer = calloc(1,size+1);
  if(buffer == NULL) {
    fclose(file);
    printf("Error allocating memory ...\n");
    return 0;
  }
  // Copiar contenido del archivo de texto a buffer
  if(1 != fread(buffer,size,1,file)) {
    fclose(file);
    free(buffer);
    printf("Error parsing text ...\n");
    return 0;
  }
  // 
  memcpy(dest,buffer,strlen(buffer));
  fclose(file);
  free(buffer);
  return 1;
}

// Generar llave para encriptacion/desencriptacion
void GenerateKey(long key,DES_key_schedule sched) {
  DES_set_key_unchecked((DES_cblock*)&key, &sched);
}

//descifra un texto dado una llave
void decrypt(long key, char *ciph, int len){
  //set parity of key and do decrypt
  long k = 0;
  for(int i=0; i<8; ++i){
    key <<= 1;
    k += (key & (0xFE << i*8));
  }
  des_setparity((char *)&k); //el poder del casteo y &
  ecb_crypt((char *)&k, (char *) ciph, 16, DES_DECRYPT);
}

//cifra un texto dado una llave
void encrypt(long key, char *ciph){
  //set parity of key and do encrypt
  long k = 0;
  for(int i=0; i<8; ++i){
    key <<= 1;
    k += (key & (0xFE << i*8));
  }
  des_setparity((char *)&k); //el poder del casteo y &
  ecb_crypt((char *)&k, (char *) ciph, 16, DES_ENCRYPT);
}

//palabra clave a buscar en texto descifrado para determinar si se rompio el codigo
char search[] = "es una prueba de";
int tryKey(long key, char *ciph, int len){
  char temp[len+1]; //+1 por el caracter terminal
  memcpy(temp, ciph, len);
  temp[len]=0; //caracter terminal
  decrypt(key, temp, len);
  return strstr((char *)temp, search) != NULL;
}

char eltexto[INT_MAX];
long the_key = 123456L;
// 2^56 / 4 es exactamente 18014398509481983
// long the_key = 18014398509481983L;
// long the_key = 18014398509481983L + 1L;

int main(int argc, char *argv[]) {
  int N, id;
  long upper = (1L << 56); 
  long mylower, myupper;
  MPI_Status st;
  MPI_Request req;

  // Lectura de archivo
  if(ReadFile(eltexto) == 0) {
    return EXIT_FAILURE;
  }

  int ciphlen = strlen((char *)eltexto);
  MPI_Comm comm = MPI_COMM_WORLD;

  // Cifrar el texto
  char cipher[ciphlen + 1];
  memcpy(cipher, eltexto, ciphlen);
  cipher[ciphlen] = 0;
  
  // Generar llave
  DES_key_schedule sched;
  GenerateKey(the_key,sched);

  // Encriptar texto
  encrypt(the_key,cipher);

  // Inicializar MPI
  MPI_Init(NULL, NULL);
  MPI_Comm_size(comm, &N);
  MPI_Comm_rank(comm, &id);

  long found = 0L;
  int ready = 0;

  // Distribuir trabajo de forma naive
  long range_per_node = upper / N;
  mylower = range_per_node * id;
  myupper = range_per_node * (id + 1) - 1;
  if(id==N-1) {
    // Compensar residuo
    myupper = upper;
  }
  printf("Process %d lower %ld upper %ld\n", id, mylower, myupper);

  // Non blocking receive, revisar en el for si alguien ya encontró
  MPI_Irecv(&found, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

  for(long i = mylower; i<myupper; ++i) {
    MPI_Test(&req, &ready, MPI_STATUS_IGNORE);
    if(ready) {
        break; // Ya encontraron, salir
    }
    if(tryKey(i, cipher, ciphlen)) {
      found = i;
      printf("Process %d found the key\n", id);
      for (int node = 0; node < N; node++) {
          MPI_Send(&found, 1, MPI_LONG, node, 0, comm); // Avisar a otros
      }
      break;
    }
  }

  // Wait y luego imprimir el texto
  if(id == 0) {
    MPI_Wait(&req, &st);
    decrypt(found, cipher, ciphlen);
    printf("Key = %li\n\n", found);
    printf("Text: %s\n", cipher);
  }
  printf("Process %d exiting\n", id);

  // FIN entorno MPI
  MPI_Finalize();
  return EXIT_SUCCESS;
}