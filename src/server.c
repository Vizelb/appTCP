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


// void handle_client(int client_sock) ;
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

    //  // Запуск клиента (компиляция и выполнение) в отдельном процессе.
    //  if (system("./client &") != 0) { // Запускаем client в фоне (&)
    //     perror("Failed to run client");
    //     exit(EXIT_FAILURE);
    // }

    // Forking to run client in a new process
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
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

    // handle_client(new_socket);   
    char buffer[BUFFER_SIZE] = {0};
    snprintf(buffer, BUFFER_SIZE, "%s %d", argv[1], atoi(argv[2]));
    send(new_socket, buffer, strlen(buffer), 0);
    // printf("Server sent: %s\n", buffer);

    char json_message[512];
    memset(json_message, 0, sizeof(json_message));

        while(recv(new_socket, json_message, sizeof(json_message), 0) > 0 )
        {
            //int size_buf = sizeof(buf) / sizeof(buf[0]);    // Размер массива
            int size_buf = sizeof(json_message) / sizeof(json_message[0]);    // Размер массива
            writeArrayToFile(filename, json_message, size_buf);

            printf("\n%s\n", json_message);
            printf("\n");


            /*char nm[20] = "popopopopo\0";
            send(client_socket, nm, sizeof(nm), 0);*/

        }

    // // Получаем текущий путь к директории, где находится server.c (предполагаем, что это и есть проект)
    //  if (getcwd(project_path, sizeof(project_path)) == NULL) {
    //     perror("getcwd() error");
    //     exit(EXIT_FAILURE);
    //  }
    // // 6. Отправка пути проекта клиенту
    // if (send(new_socket, project_path, strlen(project_path), 0) < 0)
    // {
    //      perror("send failed");
    //      exit(EXIT_FAILURE);
    // }
    //  printf("Project path sent: %s\n", project_path);

    // // 7. Чтение ответа от клиента
    // if (recv(new_socket, buffer, BUFFER_SIZE, 0) < 0)
    // {
    //     perror("recv failed");
    //      exit(EXIT_FAILURE);
    // }
    // printf("Client response: %s\n", buffer);

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

void handle_client(int client_sock) {
    // char buffer[BUFFER_SIZE] = {0};
    // msgpack_packer pk;
    // msgpack_sbuffer sbuf;

    // msgpack_sbuffer_init(&sbuf);
    // msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

    // while(1) {
    //     int bytes_read = read(client_sock, buffer, sizeof(buffer));
    //     if (bytes_read <= 0) break;

    //     msgpack_unpacked msg;
    //     msgpack_unpacked_init(&msg);
    //     if (msgpack_unpack_next(&msg, buffer, bytes_read, NULL) != MSGPACK_UNPACK_SUCCESS) {
    //         printf("Failed to unpack message\n");
    //         continue;
    //     }

    //     if (msg.data.type != MSGPACK_OBJECT_MAP) {
    //         printf("Invalid message format\n");
    //         continue;
    //     }

    //     msgpack_object_kv* kv = msg.data.via.map.ptr;
    //     printf("Event: %s, Path: %s\n", kv[0].val.via.str.ptr, kv[1].val.via.str.ptr);

    //     msgpack_sbuffer_destroy(&sbuf);
    //     msgpack_sbuffer_init(&sbuf);
    //     msgpack_unpacked_destroy(&msg);
    // }

    // close(client_sock);
}

// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <locale.h>
// #include <string.h>
// #include <stdbool.h>
// #include <time.h>

// #include <sys/socket.h>
// // #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <sys/types.h>
// #include <sys/wait.h>
// #include <signal.h>


// #define PORT 5555
// #define BUFFER_SIZE 1024
// pid_t client_pid = -1;

// void writeArrayToFile(const char *filename, char *array, int size);
// bool LaunchClient(int server_fd, int new_socket); 

// void signal_handler(int signum);

// const char *filename = "log.txt";               // Имя файла для записи

// int connection_counter = 0;

// int main(/*int argc, char *argv[]*/ )
// {
//     setlocale(LC_ALL, "Russian");           // для выводы в консоль кириллицы

//     // if (argc != 3) {
//     //     printf("Error arguments counter\n");
//     //     sleep(5);
//     //     return EXIT_FAILURE;
//     // }
//     // if (strcmp(argv[1], "server") == 0) {
//     //     printf("correct start - server\n");
//     // } else {
//     //     printf("Error arguments counter\n");
//     //     return EXIT_FAILURE;
//     // }

//     printf("I am SERVER!\n");

//     signal(SIGINT, signal_handler);
//     signal(SIGTERM, signal_handler);

//     int server_socket, new_socket;           // дескриптор сокета
//     server_socket = socket(AF_INET, SOCK_STREAM, 0);
//     if (server_socket == 0) {
//         perror("socket failed");
//         sleep(5);
//         exit(EXIT_FAILURE);
//     }

//     struct sockaddr_in socket_addr;
//     int addrlen = sizeof(socket_addr);
//     memset(&socket_addr, 0, sizeof(socket_addr));

//     socket_addr.sin_family = AF_INET;
//     socket_addr.sin_port = htons(PORT);
//     socket_addr.sin_addr.s_addr = INADDR_ANY;

//     if (bind(server_socket, (struct sockaddr *)&socket_addr, sizeof(socket_addr)) < 0) {
//         perror("bind failed");
//         sleep(5);
//         close(server_socket);
//         exit(EXIT_FAILURE);
//     }

//     // listen(s, 100);  // 100 - размер очередиы
//     // 3. Прослушивание входящих соединений
//     if (listen(server_socket, 3) < 0) {
//         perror("listen failed");
//         sleep(5);
//         close(server_socket);
//         exit(EXIT_FAILURE);
//     }

//     printf("Server is listening on port %d\n", PORT);

//     char json_message[512];
//     memset(json_message, 0, sizeof(json_message));

//     int client_socket;
//     // SOCKADDR_IN client_addr;
//     // int client_addr_size = sizeof(client_addr);

//     // Запускаем клиента
//     // if (!LaunchClient()) {
//     //     printf("Failed to launch client.\n");
//     //     Sleep(5000);
//     //     return 1; // Или обработать ошибку по-другому
//     // }
//     // LaunchClient();

//         // if (LaunchClient(server_socket, client_socket) == 1) {
//         //     printf("Continue\n");
//         //     sleep(2);
//         //     // continue;
//         // }

//     // while(1)
//     while(client_socket = accept(server_socket, (struct sockaddr *)&socket_addr, &addrlen))
//     {
//         // if ((client_socket = accept(s, (struct sockaddr *)&socket_addr, &addrlen)) < 0) {
//         //     perror("accept failed");
//         //     close(s);
//         //     // sleep(5);
//         //     exit(EXIT_FAILURE);
//         // }

//         if (client_socket < 0){
//             printf("Continue socket\n");
//             // sleep(3);
//             continue;
//         }

//         if (LaunchClient(server_socket, client_socket) == 1) {
//             printf("Continue\n");
//             sleep(2);
//             continue;
//         }


//         printf("Connect OK");
//         printf(" - number = %d\n", ++connection_counter);


//         while(recv(client_socket, json_message, sizeof(json_message), 0) > 0 )
//         {
//             //int size_buf = sizeof(buf) / sizeof(buf[0]);    // Размер массива
//             int size_buf = sizeof(json_message) / sizeof(json_message[0]);    // Размер массива
//             writeArrayToFile(filename, json_message, size_buf);

//             printf("\n%s\n", json_message);
//             printf("\n");


//             /*char nm[20] = "popopopopo\0";
//             send(client_socket, nm, sizeof(nm), 0);*/

//         }
//         // close(client_socket);
//         // close(server_socket);
//     }
//     printf("Connect wtf\n");

//     close(client_socket);
//     close(server_socket);

//     return 0;
// }


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



