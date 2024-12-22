#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h> // for PATH_MAX
#include <time.h>
#include <msgpack.h>
// #include <msgpack/unpack.h>
#include <signal.h>
#include <sys/wait.h>


#define PORT 5555
#define BUFFER_SIZE 1024
const char *filename = "log.txt";               // Имя файла для записи


void writeArrayToFile(const char *filename, char *array, int size);
void sigterm_handler(int sig);

int client_pid = 0;


int main(int argc, char *argv[]) {
        
    char *directory = NULL;
    int frequency = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 < argc && strcmp(argv[i+1], "-t") != 0 ){
                directory = argv[i + 1];
                i++;
            } else {
                printf("Error: Directory path is missing after -d flag.\n");
                return 1;
            }
        } else if (i == 1) {
            printf("Error: Flag -d is missing\n");
            return 1;
        } else if (strcmp(argv[i], "-t") == 0) {
            if (i + 1 < argc) {
                frequency = atoi(argv[i + 1]);
                i++;
            } else {
                printf("Error: Frequency value is missing after -t flag.\n");
                return 1;
            }
        } else if (i == 3) {
            printf("Error: Flag -t is missing\n");
            return 1;
        } else {
            printf("Error: Unknown argument '%s.\n", argv[i]);
            return 1;
        }
    }
    if (directory == NULL){
        printf("Error: Missing parameter: directory - %s\n", directory);
        return 1;
    }  
    if (frequency == NULL) {
        printf("Error: Missing parameter: frequency - %d\n", frequency);
        return 1; 
    }

    if (signal(SIGTERM, sigterm_handler) == SIG_ERR) {
        perror("error to connect sigterm\n");
        exit(1);
    }

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char project_path[1000];

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed\n");
        exit(EXIT_FAILURE);
    }

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

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE] = {0};
    snprintf(buffer, BUFFER_SIZE, "%s %d", directory, frequency);
    send(new_socket, buffer, strlen(buffer), 0);
    // printf("%s, %d\n", directory, frequency);

    char json_message[BUFFER_SIZE];
    memset(json_message, 0, sizeof(json_message));

    ssize_t bytes_received;

    while(bytes_received = recv(new_socket, json_message, sizeof(json_message), 0) > 0 )
    {
        msgpack_unpacked unpacked;
        msgpack_unpacked_init(&unpacked);

        size_t offset = 0;

        if (msgpack_unpack_next(&unpacked, json_message, bytes_received, &offset)) {
            msgpack_object obj = unpacked.data;

            // Ожидаем msgpack-объект типа MAP, содержащий ключи type_of_event, file_name, sha256
            if (obj.type == MSGPACK_OBJECT_MAP) {
                printf("Received event:\n");
                for (uint32_t i = 0; i < obj.via.map.size; i++) {
                    msgpack_object_kv kv = obj.via.map.ptr[i];
                    printf("%.*s: %.*s\n",
                           (int)kv.key.via.str.size, kv.key.via.str.ptr,
                           (int)kv.val.via.str.size, kv.val.via.str.ptr);
                }
            }
        }
        msgpack_unpacked_destroy(&unpacked);

        int size_buf = sizeof(json_message) / sizeof(json_message[0]); 
        writeArrayToFile(filename, json_message, size_buf);

        printf("\njson_message: %s\n", json_message);
        // printf("\n");

    }

    // Закрытие соединения и сокета
    close(new_socket);
    close(server_fd);

    return 0;
}

void sigterm_handler(int sig) {
    if (client_pid > 0) {
        printf("kill - client_pid = %d\n", client_pid);
        kill(client_pid, SIGTERM);
        waitpid(client_pid, NULL, 0);
    }
    exit(0);
}


void writeArrayToFile(const char *filename, char *array, int size)
{
    FILE *file = fopen(filename, "a+");

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
    fseek(file, 0, SEEK_END);       
    long fileSize = ftell(file);   
    if (fileSize > 0) {
        fprintf(file, "\n");       
    }

    fprintf(file, "[%s] ", timestamp);

    if (fputs(array, file) == EOF) {
        perror("Ошибка записи в файл");
        fclose(file);
    }
    fprintf(file, "\n");

    fclose(file); 
}



