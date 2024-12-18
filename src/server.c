#include <stdio.h>
#include <stdlib.h>

#include <locale.h>

#include <string.h>

#include <ws2tcpip.h>
#include <winsock2.h>

#include <time.h>

void writeArrayToFile(const char *filename, char *array, int size);

const char *filename = "log.txt";               // Имя файла для записи
//int array[] = {1, 2, 3, 4, 5};                  // Пример массива
//int size = sizeof(array) / sizeof(array[0]);    // Размер массива

int connection_counter = 0;

int main()
{
    setlocale(LC_ALL, "Russian");           // для выводы в консоль кириллицы

    printf("I am SERVER!\n");

    WSADATA ws;
    WSAStartup( MAKEWORD(2, 2), &ws);      // инициализаия использования секитов

    SOCKET s;           // дескриптор сокета
    s = socket(AF_INET, SOCK_STREAM, 0);

    SOCKADDR_IN sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(5555);


    bind(s, &sa, sizeof(sa));

    listen(s, 100);  // 100 - размер очередиы

    int buf[5];
    char json_message[512];
    memset(json_message, 0, sizeof(json_message));
    memset(buf, 0, sizeof(buf));

    SOCKET client_socket;
    SOCKADDR_IN client_addr;
    int client_addr_size = sizeof(client_addr);

    while(client_socket = accept(s, &client_addr, &client_addr_size))
    {

        printf("Connect OK");
        printf(" - number = %d\n", ++connection_counter);


        while(recv(client_socket, json_message, sizeof(json_message), 0) > 0 )
        {
            //int size_buf = sizeof(buf) / sizeof(buf[0]);    // Размер массива
            int size_buf = sizeof(json_message) / sizeof(json_message[0]);    // Размер массива
            writeArrayToFile(filename, json_message, size_buf);

            /*for (int i = 0; i < 512; i++) {
                printf("%c ", buf_c[i]);
            }*/
            printf("\n%s\n", json_message);
            printf("\n");


            /*char nm[20] = "popopopopo\0";
            send(client_socket, nm, sizeof(nm), 0);*/

        }
    }





    closesocket(s);

    return 0;
}


void writeArrayToFile(const char *filename, char *array, int size)
{
    FILE *file = fopen(filename, "a+"); // Открываем файл в режиме добавления (создаёт файл, если его нет)

    if (file == NULL) {
        perror("Ошибка открытия файла");
        exit(EXIT_FAILURE);
    }


    // Получаем текущее время
    time_t now = time(NULL);
    if (now == -1) {
        perror("Ошибка получения времени");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Преобразуем время в локальное
    struct tm *localTime = localtime(&now);
    if (localTime == NULL) {
        perror("Ошибка преобразования времени");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Форматируем метку времени
    char timestamp[64];
    if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localTime) == 0) {
        fprintf(stderr, "Ошибка форматирования времени\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }



    // Проверяем, пуст ли файл
    fseek(file, 0, SEEK_END);       // Перемещаем указатель в конец файла
    long fileSize = ftell(file);    // Узнаём размер файла
    if (fileSize > 0) {
        fprintf(file, "\n");        // Если файл не пустой, добавляем новую строку перед записью
    }

    // Записываем метку времени в файл
    fprintf(file, "[%s] ", timestamp);

    // Записываем элементы массива в файл через пробел
    /*for (int i = 0; i < size; i++) {
        fprintf(file, "%d", array[i]);
        if (i < size - 1) {
            fprintf(file, " "); // Добавляем пробел между элементами
        }
    }*/
    // Записываем массив в файл
    if (fputs(array, file) == EOF) {
        perror("Ошибка записи в файл");
        fclose(file);
        return 1;
    }
    fprintf(file, "\n");

    fclose(file); // Закрываем файл
}

