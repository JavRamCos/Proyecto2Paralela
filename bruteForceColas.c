#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <mpi.h>
#include <unistd.h>
#include <time.h>
#include <openssl/des.h>

#define INFILE "input_file.txt"
#define TASK_SIZE 1000000L // Tamaño de la tarea que se asignará a cada proceso

// Las funciones ReadFile, decrypt, encrypt y tryKey son las mismas que en tu código anterior

char eltexto[INT_MAX];

// palabra clave a buscar en texto descifrado para determinar si se rompio el codigo
char search[] = "Lorem ipsum dolor sit";
int tryKey(long num,char* src) {
  char str[256];
  sprintf(str,"%ld",num);
  DES_cblock tempkey;
  DES_key_schedule tempsched; 
  DES_string_to_key(str,&tempkey);
  DES_set_key((const_DES_cblock*)&tempkey,&tempsched); 
  char temp[strlen(src)];
  decrypt(src,temp,tempsched);
  return strstr((char *)temp, search) != NULL;
}

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

// descifra un texto dado una llave
void decrypt(char* src,char* dest,DES_key_schedule sched) {
  // Desencriptar texto completo en cadenas de 8 bytes
  for(int i  = 0; i < strlen(src); i += 8) {
    char temp[8] = { src[i], src[i+1], src[i+2], src[i+3],
                     src[i+4], src[i+5], src[i+6], src[i+7] };        
    char temp2[8] = {""};
    DES_ecb_encrypt((const_DES_cblock*)temp,(DES_cblock*)temp2,&sched,DES_DECRYPT);
    // Agregar resultado al final de la lista dest
    for(int k = 0; k<8; k++) {
      dest[i+k] = temp2[k];
    }
  }
}

// cifa un texto dado una llave
int encrypt(char* src,char* dest,DES_key_schedule sched) {
  // Verificar que la cadena tenga longitud multiplo de 8
  if(strlen(src)-1 % 8 == 0) {
    printf("Text does not have a length multiple of 8...\n");
    return 0;
  }
  // Encriptar el texto completo en pedazos de 8 bytes
  for(int i  = 0; i < strlen(src); i += 8) {
    char temp[8] = { src[i], src[i+1], src[i+2], src[i+3],
                     src[i+4], src[i+5], src[i+6], src[i+7] };        
    char temp2[8] = {""};
    DES_ecb_encrypt((const_DES_cblock*)temp,(DES_cblock*)temp2,&sched,DES_ENCRYPT);
    // Agregar resultado al final de la lista dest
    for(int k = 0; k<8; k++) {
      dest[i+k] = temp2[k];
    }
  }
  return 1;
}


int main(int argc, char *argv[]) {
    double starttime, endtime;
    int N, id;
    long upper = (1L << 56); 
    long next_task = 0L;
    starttime = MPI_Wtime();

    // Inicialización de MPI y obtención del tamaño y rango
    MPI_Status st;
    MPI_Request req;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Init(NULL,NULL);
    MPI_Comm_size(comm,&N);
    MPI_Comm_rank(comm,&id);

        // Lectura de archivo
    if(ReadFile(eltexto) == 0) {
        return EXIT_FAILURE;
    }
    // char eltexto[] = "TestTestTestTest";

    // Generar llave
    char the_key[] = "123456";
    // 2^56 / 4 es exactamente 18014398509481983
    // long the_key = 18014398509481983L;
    // long the_key = 18014398509481983L + 1L;
    DES_cblock key;
    DES_key_schedule sched; 
    DES_string_to_key(the_key,&key);
    DES_set_key((const_DES_cblock*)&key,&sched); 

      // Cifrar el texto
    char ciphtext[strlen(eltexto)];
    char finaltext[strlen(eltexto)];
    encrypt(eltexto,ciphtext,sched);

    long found = 0L;
    int ready = 0;

        // Iniciar la recepción no bloqueante, para verificar en el bucle si alguien ya encontró la clave
    MPI_Irecv(&found, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

    while (next_task < upper) {
        long mylower = next_task;
        long myupper = mylower + TASK_SIZE - 1;
        if (myupper > upper) {
        myupper = upper;
        }
        next_task += TASK_SIZE;

        for(long i = mylower; i<=myupper; ++i) {
        MPI_Test(&req, &ready, MPI_STATUS_IGNORE);
        if(ready) {
            break; 
        }
        if(tryKey(i,ciphtext)) {
            found = i;
            printf("Process %d found the key\n", id);
            for (int node = 0; node < N; node++) {
                MPI_Send(&found, 1, MPI_LONG, node, 0, comm); // Avisar a otros
            }
            break;
        }
        }
        if (ready) {
        break;
        }
    }

    // Espera y luego imprime el texto
    if(id == 0) {
        MPI_Wait(&req, &st);
        DES_key_schedule found_schedule; // Cambiado a DES_key_schedule
        DES_set_key_unchecked((DES_cblock *)&found, &found_schedule); // Utiliza la clave encontrada (found) en lugar de the_key
        decrypt(ciphtext,finaltext,sched);
        printf("Key = %li\n\n", found);
        printf("Texto original: %s\n",eltexto);
        printf("Texto cifrado: %s\n",ciphtext);
        printf("Texto descifrado: %s\n",finaltext);
        endtime = MPI_Wtime();
        printf("Tiempo de ejecucion: %.3lf s\n",endtime-starttime);
    }
    printf("Process %d exiting\n", id);
    // Finalizar entorno MPI
    MPI_Finalize();
    return EXIT_SUCCESS;
}