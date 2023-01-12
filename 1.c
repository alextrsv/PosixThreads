#define _GNU_SOURCE

#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <linux/futex.h>
#include <syscall.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <inttypes.h>


//C=malloc;D=19;E=63;F=nocache;G=48;H=random;I=133;J=avg;K=flock
#define A 97
#define MEMSIZE_B A*1024*1024
#define B 0x1C57CB42
#define D 19
#define E 63
#define FILESIZE_B E*1024*1024
#define I 133
#define FILES_NUMBER (A/E)+1;


 void * start_address;


typedef struct fillMemoryArgs {
    void *memory_pointer;
    size_t memory_size;
    FILE *urandom;
} fillArgs;



void * fill_memory_from_thread(void * args){
    fillArgs *fill_args = (fillArgs *) args;
    int c = fread((void *) fill_args->memory_pointer, sizeof(int), (fill_args->memory_size)/sizeof(int), fill_args->urandom);
    if(c <= 0){
        perror("Память не заполняется");
        _exit(1);
    }

    int * num = fill_args->memory_pointer;
    printf("%p:   %d\n", fill_args->memory_pointer,  *num );

    return NULL;
}


void fill_memory(size_t memory_size){  
    FILE *urandom = fopen("/dev/urandom", "r");
    pthread_t threads[D];
    size_t block_size = memory_size/D;

    fillArgs args = {start_address, block_size, urandom};

    for(int i = 0; i < D-1; i++){
        pthread_create(&threads[i], NULL, fill_memory_from_thread, (void *) &args);
        args.memory_pointer += (memory_size/D);
    }

    printf("Ждём завершения потоков...\n");
        
    for (int i = 0; i < D-1; i++){
        pthread_join(threads[i], NULL); 
    }
    printf("Область памяти заполнена\n");

    // int * num = start_address;
    // for (int i = 25427000; i < 25427968; i++){
    //     printf("[%d]:  %d  \n",i,  num[i]);
    //     // if (num[i] == 0 ) break;
    // }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////FILES_WRITE///////////////////////////////////////////////////////////////////////////
void write_from_memory_to_file(int file, void* memory_pointer, uint64_t size){
    write(file, memory_pointer, size);
    close(file);
}

//Названия файлов
const char* make_filename(uint8_t name){
    switch (name) {
    case 0:
        return "./first.txt";
    case 1:
        return "./second.txt";
    case 2:
        return "./third.txt";
    case 3:
        return "./forth.txt";
    case 4:
        return "./fiveth.txt";
    default:
        return "./over.txt";
    }
}

void fill_files(void* memory_pointer){
    printf("Начало работы с файлами...\n");
    uint64_t files_count = (A / E) +1;
    size_t size_to_write = FILESIZE_B;
    size_t size_left = MEMSIZE_B;

    for (uint8_t i = 0; i < files_count; i++){
        const char* filename = make_filename(i);
        int out = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
        printf("Файл открыт, начинаем запись...\n");
        if (size_left < FILESIZE_B) {
            size_to_write = size_left;
            printf("size_to_write was changed, теперь =  %ld\n", size_left);
        }
        write_from_memory_to_file(out, memory_pointer, size_to_write);
        size_left -= FILESIZE_B;
        memory_pointer += FILESIZE_B;
    }
    printf("Работа с файлами закончена...\n");

}
//////////////////////FILES_WRITE///////////////////////////////////////////////////////////////////////////


void file_lock(int fd) {
    int flockRc;

    if (flock(fd, LOCK_SH) != 0)  {
        printf("Ошибка при блокировании, errno = %d\n", errno);
        exit(1);
    }
 
}
 
void file_unlock(int fd) {
    int flockRc = flock(fd, LOCK_UN); // Сброс блокировки
    if (flockRc != 0)  { 
        printf("Ошибка при разблокировке, errno = %d\n", errno);
        exit(1);
    }
 
}


void analyze_file(const char* fileName) {
    int fd = open(fileName, O_CREAT | O_RDONLY, S_IRWXU | S_IRGRP | S_IROTH);
 
    if (fd < 0) {
        printf("Ошибка при открытии '%s' только для чтения\n", fileName);
        return;
    }
 
    file_lock(fd);
 
    //Читаем размер файла с помощью указателя
    off_t size = lseek(fd, 0L, SEEK_END);
    //Возвращаем указатель обратно
    lseek(fd, 0, SEEK_SET);
 
    // Выделяем место
    int* data = (int*) malloc(size);
    
    // Читаем файл в память целиком
    read(fd, data, size);
 
    file_unlock(fd);
    close(fd);
 
    // Считаем сумму в файле
    __uint64_t sum = 0;
    __uint64_t amount = size/sizeof(int);
    __uint64_t avg = 0;
    for (size_t i = 1; i < amount; i ++) {
            sum += data[i];
    }
    // avg = sum/amount;

    printf("Расчётная сумма: '%s' = %ld\n", fileName, sum);
    // printf("Расчётное среднее: '%s' = %ld\n", fileName, avg);
 
    free(data);
}

void* read_from_file_thread(void* vargPtr) {
    while(1) {
        for (int i = 0; i < (A/E)+1; i++) {
            const char* fileName = make_filename(i);
            analyze_file(fileName);
        }
    }
}
 

void read_from_files(){
    printf("Читаем файлы в %d потоков...\n", I);
 
    pthread_t threads[I];
    for (int i = 0; i < I; i++) {
        pthread_create(&threads[i], NULL, read_from_file_thread, NULL);
    }
}

int main(void){ 
    printf(" всего элементов: %ld\n",  MEMSIZE_B/sizeof(int));

    start_address = malloc(MEMSIZE_B);
    printf("Начальный адрес: %p\n", start_address); 
 
    while(1){
        fill_memory(MEMSIZE_B);
        fill_files(start_address);
        read_from_files();
    }


    // while(1);

    return 0;
}
