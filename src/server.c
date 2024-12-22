#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h> // for PATH_MAX
#include <time.h>

#include <signal.h>
#include <sys/wait.h>
#include <msgpack.h>

#define PORT 5555
#define BUFFER_SIZE 1024
const char *filename = "log.txt";               // Имя файла для записи


void writeArrayToFile(const char *filename, char *array, int size);
void sigterm_handler(int sig);

int client_pid = 0;


int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <directory> <frequency>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    // char buffer[BUFFER_SIZE] = {0};
    char project_path[1000];
    
    // struct sigaction sa;

    // sa.sa_handler = sigterm_handler;
    // sigemptyset(&sa.sa_mask);
    // sa.sa_flags = 0;
    // sigaction(SIGTERM, &sa, NULL);

    // 1. Создание сокета
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 2. Настройка адреса
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 3. Привязка сокета к адресу и порту
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // 4. Прослушивание входящих соединений
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);


    // Запуск клиента, как отдельный процесс
    pid_t pid = fork();
    if (pid == 0) {
        execl("./client", "client", NULL);
        perror("execl");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else {
        client_pid = pid;
    }


    // 5. Принятие входящего соединения
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE] = {0};
    snprintf(buffer, BUFFER_SIZE, "%s %d", argv[1], atoi(argv[2]));
    send(new_socket, buffer, strlen(buffer), 0);
    // printf("Server sent: %s\n", buffer);

    char json_message[512];
    memset(json_message, 0, sizeof(json_message));

        while(recv(new_socket, json_message, sizeof(json_message), 0) > 0 )
        {
            int size_buf = sizeof(json_message) / sizeof(json_message[0]);    // Размер массива
            writeArrayToFile(filename, json_message, size_buf);

            printf("\n%s\n", json_message);
            printf("\n");

        }

    // 8. Закрытие соединения и сокета
    close(new_socket);
    close(server_fd);

    return 0;
}

void sigterm_handler(int sig) {
    if (client_pid > 0) {
        kill(client_pid, SIGTERM);
        waitpid(client_pid, NULL, 0);
    }
    exit(0);
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

    // Записываем массив в файл
    if (fputs(array, file) == EOF) {
        perror("Ошибка записи в файл");
        fclose(file);
    }
    fprintf(file, "\n");

    fclose(file); // Закрываем файл
}



