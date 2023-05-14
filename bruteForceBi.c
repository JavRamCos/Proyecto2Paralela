#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <mpi.h>
#include <unistd.h>
#include <time.h>
#include <openssl/des.h>

#define INFILE "input_file.txt"
// ... mantenemos las funciones ReadFile, decrypt, encrypt y tryKey

int ReadFile(char* dest) {
  FILE* file;
  long size;
  char* buffer;
  file = fopen(INFILE,"rb");
  if(file == NULL) {
    printf("Error reading text file ...\n");
    return 0;
  }
  fseek(file,0L,SEEK_END);
  size = ftell(file);
  rewind(file);
  buffer = calloc(1,size+1);
  if(buffer == NULL) {
    fclose(file);
    printf("Error allocating memory ...\n");
    return 0;
  }
  if(1 != fread(buffer,size,1,file)) {
    fclose(file);
    free(buffer);
    printf("Error parsing text ...\n");
    return 0;
  }
  memcpy(dest,buffer,strlen(buffer));
  fclose(file);
  free(buffer);
  return 1;
}

void decrypt(char* src,char* dest,DES_key_schedule sched) {
  for(int i  = 0; i < strlen(src); i += 8) {
    char temp[8] = { src[i], src[i+1], src[i+2], src[i+3],
                     src[i+4], src[i+5], src[i+6], src[i+7] };        
    char temp2[8] = {""};
    DES_ecb_encrypt((const_DES_cblock*)temp,(DES_cblock*)temp2,&sched,DES_DECRYPT);
    for(int k = 0; k<8; k++) {
      dest[i+k] = temp2[k];
    }
  }
}

int encrypt(char* src,char* dest,DES_key_schedule sched) {
  if(strlen(src)-1 % 8 == 0) {
    printf("Text does not have a length multiple of 8...\n");
    return 0;
  }

  for(int i  = 0; i < strlen(src); i += 8) {
    char temp[8] = { src[i], src[i+1], src[i+2], src[i+3],
                      src[i+4], src[i+5], src[i+6], src[i+7] };        
    char temp2[8] = {""};
    DES_ecb_encrypt((const_DES_cblock*)temp,(DES_cblock*)temp2,&sched,DES_ENCRYPT);
    for(int k = 0; k<8; k++) {
      dest[i+k] = temp2[k];
    }
  }
  return 1;
}

char eltexto[INT_MAX];

char search[] = "Lorem ipsum dolor sit";
long tryKeys(long num1,long num2,char* src) {
  // Primera llave
  char str1[256];
  sprintf(str1,"%ld",num1);
  DES_cblock tempkey1;
  DES_key_schedule tempsched1; 
  DES_string_to_key(str1,&tempkey1);
  DES_set_key((const_DES_cblock*)&tempkey1,&tempsched1); 
  char temp1[strlen(src)];
  decrypt(src,temp1,tempsched1);
  // Segunda llave
  char str2[256];
  sprintf(str2,"%ld",num2);
  DES_cblock tempkey2;
  DES_key_schedule tempsched2; 
  DES_string_to_key(str2,&tempkey2);
  DES_set_key((const_DES_cblock*)&tempkey2,&tempsched2); 
  char temp2[strlen(src)];
  decrypt(src,temp2,tempsched2);
  if(strstr((char *)temp1, search) != NULL) {
    return num1;
  } else if(strstr((char *)temp2, search) != NULL) {
    return num2;
  } else {
    return 0;
  }
}

int main(int argc, char *argv[]) {
  double starttime, endtime;
  int N, id;
  long upper = (1L << 56); 
  long mylower, myupper;
  starttime = MPI_Wtime();

    // Lectura de archivo
  if(ReadFile(eltexto) == 0) {
    return EXIT_FAILURE;
  }
  // char eltexto[] = "TestTestTestTest";

  // Generar llave
  char the_key[] = "123456789";
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
    // Inicializar MPI
  MPI_Status st;
  MPI_Request req;
  MPI_Comm comm = MPI_COMM_WORLD;
  MPI_Init(NULL,NULL);
  MPI_Comm_size(comm,&N);
  MPI_Comm_rank(comm,&id);

  long found = 0L;
  int ready = 0;

  // Distribuir trabajo de forma naive
  long range_per_node = upper / N;
  mylower = range_per_node * id;
  myupper = range_per_node * (id + 1) - 1;
  if(id ==N-1) {
    // Compensar residuo
    myupper = upper;
  }
  printf("Process %d lower %ld upper %ld\n", id, mylower, myupper);

  // Non blocking receive, revisar en el for si alguien ya encontró
  MPI_Irecv(&found, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

  for(long i = 0; i<(myupper-mylower)/2+1; i++) {
    MPI_Test(&req, &ready, MPI_STATUS_IGNORE);
    if(ready) {
      break; 
    }
    long res = tryKeys(mylower+i,myupper-i,ciphtext);
    if(res > 0) {
      found = res;
      printf("Process %d found the key\n", id);
      for (int node = 0; node < N; node++) {
          MPI_Send(&found, 1, MPI_LONG, node, 0, comm); // Avisar a otros
      }
      break;
    }
  }

  //Mantemenos el proceso de decrypt y results
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
  MPI_Finalize();
  return EXIT_SUCCESS;
}