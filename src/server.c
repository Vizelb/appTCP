#include <stdio.h>
#include <stdlib.h>

#include <locale.h>

#include <string.h>

#include <ws2tcpip.h>
#include <winsock2.h>

#include <time.h>

void writeArrayToFile(const char *filename, char *array, int size);
BOOL LaunchClient(); 

const char *filename = "log.txt";               // Имя файла для записи

int connection_counter = 0;

int main()
{
    setlocale(LC_ALL, "Russian");           // для выводы в консоль кириллицы

    printf("I am SERVER!\n");

    int errWsasStartup;
    WSADATA ws;
    if (errWsasStartup = WSAStartup( MAKEWORD(2, 2), &ws) != 0)      // инициализаия использования секитов
        printf("Ошибка WSAStartup: %d\n", errWsasStartup);

    SOCKET s;           // дескриптор сокета
    s = socket(AF_INET, SOCK_STREAM, 0);

    SOCKADDR_IN sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(5555);
    sa.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(s, (struct sockaddr *)&sa, sizeof(sa));

    listen(s, 100);  // 100 - размер очередиы

    // int buf[5];
    char json_message[512];
    memset(json_message, 0, sizeof(json_message));
    // memset(buf, 0, sizeof(buf));

    SOCKET client_socket;
    SOCKADDR_IN client_addr;
    int client_addr_size = sizeof(client_addr);

    // Запускаем клиента
    if (!LaunchClient()) {
        printf("Failed to launch client.\n");
        Sleep(5000);
        return 1; // Или обработать ошибку по-другому
    }
    else printf("Successfully to launch client.\n");

    // while(1)
    while(client_socket = accept(s, (struct sockaddr *)&client_addr, &client_addr_size))
    {

        // client_socket = accept(s, (struct sockaddr *)&client_addr, &client_addr_size);

        // if (client_socket == INVALID_SOCKET)
        // {
        //     printf("Errpr connection to client\n");
        //     continue;
        // }

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
        // closesocket(s);
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
        //return 1;
    }
    fprintf(file, "\n");

    fclose(file); // Закрываем файл
}



// Функция для запуска клиента
BOOL LaunchClient() {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Путь к исполняемому файлу клиента
    // Важно! Укажите корректный путь.
    // Если клиент в той же директории, можно использовать просто "client.exe".
    if (!CreateProcess(
        "client.exe",   // Имя исполняемого файла
        NULL,           // Командная строка (можно передать аргументы)
        NULL,           // Дескриптор защиты процесса
        NULL,           // Дескриптор защиты потока
        FALSE,          // Наследование дескрипторов
        CREATE_NEW_CONSOLE,  // Создать новое консольное окно для клиента
        NULL,           // Блок среды
        NULL,           // Текущий каталог
        &si,            // STARTUPINFO
        &pi             // PROCESS_INFORMATION
    )) {
        printf("CreateProcess failed (%d).\n", GetLastError());
        return FALSE;
    }

    // Закрываем дескрипторы, которые нам больше не нужны на сервере.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return TRUE;
}
