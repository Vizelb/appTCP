#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <msgpack.h>
#include <signal.h>
#include <sys/wait.h>


#define PORT 5555
#define BUFFER_SIZE 1024

// Структура сообщения
typedef struct {
    char type_of_event[50];
    char file_name[256];
    char sha256[65];
} Message;

static void writeArrayToFile(const char *filename, char *array, int size);
static void sigterm_handler(int sig);
static int deserialize_message(const char *data, size_t len, Message *msg);

static const char *filename = "log.txt";               // Имя файла для записи
static int client_pid = 0;


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
    if (frequency == 0){
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
    // printf("Server listening on port %d...\n", PORT);


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

    char json_message[BUFFER_SIZE];
    memset(json_message, 0, sizeof(json_message));

    ssize_t bytes_received;

    while(bytes_received = recv(new_socket, json_message, sizeof(json_message), 0) > 0 )
    {
        // Message msg;
        // if (deserialize_message(json_message, bytes_received, &msg) == 0) {
            // printf("Received message:\n");
            // printf("Type of Event: %s\n", msg.type_of_event);
            // printf("File Name: %s\n", msg.file_name);
            // printf("SHA256: %s\n", msg.sha256);
        // }   
        // char event_buffer[BUFFER_SIZE];
        // snprintf(event_buffer, BUFFER_SIZE, "type_of_event: %s, file_name: %.255s, sha256: %s", 
        //     msg.type_of_event,
        //     msg.file_name,
        //     msg.sha256);

        int size_buf = sizeof(json_message) / sizeof(json_message[0]); 
        writeArrayToFile(filename, json_message, size_buf);

        printf("\njson_message: %s\n", json_message);
    }

    // Закрытие соединения и сокета
    close(new_socket);
    close(server_fd);

    return 0;
}

static void sigterm_handler(int sig) {
    if (client_pid > 0) {
        printf("kill - client_pid = %d\n", client_pid);
        kill(client_pid, SIGTERM);
        waitpid(client_pid, NULL, 0);
    }
    exit(0);
}


static void writeArrayToFile(const char *filename, char *array, int size)
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


static int deserialize_message(const char *data, size_t len, Message *msg) {
    msgpack_unpacked result;
    msgpack_unpacked_init(&result);

    // printf("data = %s\n", data);
    if (!msgpack_unpack_next(&result, data, len, NULL)) {
        fprintf(stderr, "Failed to unpack message\n");
        msgpack_unpacked_destroy(&result);
        return -1;
    }

    msgpack_object obj = result.data;
    // printf("obj.type = %d (expected: %d)\n", obj.type, MSGPACK_OBJECT_MAP);
    // printf("data = %s\n", data);
    if ((int)obj.type != (int)MSGPACK_OBJECT_MAP) {
        fprintf(stderr, "Invalid message format\n");
        msgpack_unpacked_destroy(&result);
        return -1;
    }
    
    msgpack_object_kv *kv = obj.via.map.ptr;
    for (size_t i = 0; i < obj.via.map.size; ++i) {
        if (strncmp(kv[i].key.via.str.ptr, "type_of_event: ", kv[i].key.via.str.size) == 0) {
            snprintf(msg->type_of_event, sizeof(msg->type_of_event), "%.*s",
                     (int)kv[i].val.via.str.size, kv[i].val.via.str.ptr);
        } else if (strncmp(kv[i].key.via.str.ptr, "file_name: ", kv[i].key.via.str.size) == 0) {
            snprintf(msg->file_name, sizeof(msg->file_name), "%.*s",
                     (int)kv[i].val.via.str.size, kv[i].val.via.str.ptr);
        } else if (strncmp(kv[i].key.via.str.ptr, "sha256: ", kv[i].key.via.str.size) == 0) {
            snprintf(msg->sha256, sizeof(msg->sha256), "%.*s",
                     (int)kv[i].val.via.str.size, kv[i].val.via.str.ptr);
        }
    }

    msgpack_unpacked_destroy(&result);
    return 0;
}
