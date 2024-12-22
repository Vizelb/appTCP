// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/inotify.h>
#include <limits.h>
#include <sys/inotify.h>
#include <signal.h>
#include <pthread.h>

#include <openssl/ssl.h>
#include <msgpack.h>

#define PORT 5555
#define SERVER_IP "127.0.0.1" // localhost
#define BUFFER_SIZE 1024
#define EVENT_SIZE  (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

const int frequency = 10;

int sock = 0;
pthread_t inotify_thread, sender_thread;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
char event_buffer[BUFFER_SIZE] = {0};

void *event_sender(void *arg);
void *inotify_watcher(void *arg);
void sigterm_handler(int sig);
void compute_sha256(const char *str, char *outputBuffer);
char* get_full_path(const char *dir_path, const char *file_name_path) ;

int main() {
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    // struct sigaction sa;

    // sa.sa_handler = sigterm_handler;
    // sigemptyset(&sa.sa_mask);
    // sa.sa_flags = 0;
    // sigaction(SIGTERM, &sa, NULL);

    // 1. Создание сокета
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    // 2. Настройка адреса сервера
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Преобразование IP-адреса из строки в бинарный формат
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    // 3. Подключение к серверу
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }

    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    pthread_create(&inotify_thread, NULL, inotify_watcher, &cond);
    pthread_create(&sender_thread, NULL, event_sender, &cond);

    pthread_join(inotify_thread, NULL);
    pthread_join(sender_thread, NULL);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    close(sock);

    return 0;
}

void sigterm_handler(int sig) {
    pthread_cancel(inotify_thread);
    pthread_cancel(sender_thread);
    close(sock);
    exit(0);
}

void *inotify_watcher(void *arg) {
    int fd = inotify_init();
    if (fd < 0) {
        perror("inotify_init");
        pthread_exit(NULL);
    }

    const char *directory_to_watch = "/programming/appTCP/appTCP/build";
    int wd = inotify_add_watch(fd, directory_to_watch, IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd == -1) {
        perror("inotify_add_watch");
        pthread_exit(NULL);
    }

    clock_t start_time = clock();           // Запоминаем начальное время
    int timer_seconds = frequency;          // Таймер на x секунд

    while(1) {
        if (!((clock() - start_time) > timer_seconds * CLOCKS_PER_SEC)){
            continue;
        }
        start_time = clock();

        int length = read(fd, event_buffer, EVENT_BUF_LEN);
        if (length < 0) {
            perror("read");
            continue;
        }

        int i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &event_buffer[i];
            if (event->len) {
                pthread_mutex_lock(&mutex);
             
                char *full_path = get_full_path(directory_to_watch, event->name);                
                const char *type_of_event;

                if (event->mask & IN_CREATE || event->mask & IN_MOVED_TO ){
                    // if (event->mask & IN_ISDIR) {
                    //     printf("Directory created: %s\n", full_path);
                    // } else {
                    //     printf("File created: %s\n", full_path);
                    // }
                    type_of_event = "create";
                }
                else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
                    // if (event->mask & IN_ISDIR) {
                    //     printf("Directory deleted: %s\n", full_path);
                    // } else {
                    //     printf("File deleted: %s\n", full_path);
                    // }
                    type_of_event = "DELETE";
                } else {
                    // if (event->mask & IN_ISDIR) {
                    //     printf("Directory modified: %s\n", full_path);
                    // } else {
                    //     printf("File modified: %s\n", full_path);
                    // }
                    type_of_event = "unknown";
                }
                // else type_of_event = "UNKNOWN";

                char sha256_buff[65];
                // printf("path %s\n", full_path);
                compute_sha256(full_path, sha256_buff);

                char file_name_d[255 + 1] = {0};  
                strncpy(file_name_d, event->name, event->len); 
                file_name_d[event->len] = '\0';  

                snprintf(event_buffer, BUFFER_SIZE, "type_of_event: %s, file_name: %.255s, sha256: %s", 
                         type_of_event,
                         file_name_d,
                         sha256_buff);
                pthread_mutex_unlock(&mutex);
                pthread_cond_signal((pthread_cond_t*)arg);
            }
            i += EVENT_SIZE + event->len;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
    pthread_exit(NULL);
}

void *event_sender(void *arg) {
    pthread_cond_t *cond = (pthread_cond_t*)arg;
    while(1) {
        pthread_cond_wait(cond, &mutex);
        send(sock, event_buffer, strlen(event_buffer), 0);
        // printf("Client sent: %s\n", event_buffer);
    }
    pthread_exit(NULL);
}

// Функция для получения полного пути к файлу
char* get_full_path(const char *dir_path, const char *file_name_path) {
    static char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, file_name_path);
    return full_path;
}


// // Функция для вычисления SHA256-хэша строки
void compute_sha256(const char *str, char *outputBuffer) {
    unsigned char hash[SHA256_DIGEST_LENGTH]; // Массив для хранения хэша
    SHA256_CTX sha256;
    SHA256_Init(&sha256);                     // Инициализация SHA256
    SHA256_Update(&sha256, str, strlen(str)); // Обновление хэша с использованием данных
    SHA256_Final(hash, &sha256);              // Финализация расчета

    // Преобразуем бинарный хэш в строку формата HEX
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[64] = 0; // Завершаем строку нулевым символом
}





// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <locale.h>
// #include <time.h>
// #include <pthread.h>
// #include <unistd.h>

// #include <sys/inotify.h>

// #include <arpa/inet.h>
// #include <openssl/ssl.h>
// #include <msgpack.h>


// void InitWatchDirectory(const char *path);
// void SendMessageToServer();
// void DirectoryWatcher(void);
// void compute_sha256(const char *str, char *outputBuffer);
// void create_json_message(const char *type_of_event, const char *file_name);

// void *send_data_to_server(void *arg);
// void *monitor_directory(void *arg);


// // unsigned char directory_name[128] = "C:/DanyaMain/Projects_programming/C/C/ClientTCP/";  old
// // const unsigned char directory_name[128] = "C:/DanyaMain/Projects_programming/C/appTCP/build/";
// #define PATH_TO_WATCH "/programming/appTCP/appTCP"

// int hDir = 0;
// int flag_event = 0;

// #define PORT 5555
// #define BUFFER_SIZE 1024

// int sock = 0;           // дескриптор сокета

// char json_message[512];

// #define MAX_EVENTS 16
// #define EVENT_SIZE (sizeof(struct inotify_event))
// #define BUF_LEN (MAX_EVENTS * (EVENT_SIZE + 16))

// char *directory_to_watch = "/path/to/watch"; // Замените на нужную директорию


// int main()
// {
//     SSL_library_init();
//     SSL_load_error_strings();
//     printf("OpenSSL Version: %s\n", OpenSSL_version(OPENSSL_VERSION));

//     setlocale(LC_ALL, "Russian");           // для выводы в консоль кириллицы

//     printf("I am CLIENT!\n");
//     sleep(3);

    
//     //int sock;           // дескриптор сокета
//     sock = socket(AF_INET, SOCK_STREAM, 0);
//     if (sock < 0) {
//         perror("socket failed");
//         sleep(5);
//         exit(EXIT_FAILURE);
//     }

//     struct sockaddr_in sa;
//     memset(&sa, 0, sizeof(sa));
//     sa.sin_family = AF_INET;
//     sa.sin_port = htons(PORT);

//     // sa.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
//     // sa.sin_addr.s_addr = htonl(INADDR_ANY);

//     // Преобразование IP-адреса из строки в бинарный формат
//     if (inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr) <= 0) {
//         printf("\nInvalid address/ Address not supported \n");
//         sleep(5);
//         return -1;
//     }

//     if(connect(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {   // установка соединения с сервером
//         printf("\nConnection Failed \n");
//         sleep(5);
//         return -1;
//     }

//     // InitWatchDirectory("C:/DanyaMain/Projects_programming/C/C/ClientTCP");
//     // InitWatchDirectory("C:/DanyaMain/Projects_programming/C/appTCP/build/");


//     // Создаем мьютекс и условную переменную
//     pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
//     pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

//     // Создаем потоки
//     pthread_t thread_send, thread_monitor;
//     pthread_create(&thread_send, NULL, send_data_to_server, &sock);
//     pthread_create(&thread_monitor, NULL, monitor_directory, &sock);

//     // Ждем завершения потоков
//     pthread_join(thread_send, NULL);
//     pthread_join(thread_monitor, NULL);

//     close(sock);

//     // hMutex = CreateMutex(NULL, FALSE, NULL);
//     // if (hMutex == NULL) {
//     //     printf("Не удалось создать мьютекс\n");
//     //     CloseHandle(hDir);
//     //     return 1;
//     // }

//     // // Создаем поток для наблюдения за директорией
//     // int hThreadWatcher = CreateThread(NULL, 0, DirectoryWatcher, NULL, 0, NULL);
//     // if (hThreadWatcher == NULL) {
//     //     printf("Не удалось создать поток наблюдателя\n");
//     //     CloseHandle(hMutex);
//     //     CloseHandle(hDir);
//     //     return 1;
//     // }

//     // // Создаем поток для отправки сообщений
//     // int hThreadMessendger = CreateThread(NULL, 0, SendMessageToServer, NULL, 0, NULL);
//     // if (hThreadMessendger == NULL) {
//     //     printf("Не удалось создать поток\n");
//     //     CloseHandle(hThreadWatcher);
//     //     CloseHandle(hMutex);
//     //     CloseHandle(hDir);
//     //     return 1;
//     // }

//     // // Ждем завершения обоих потоков (в реальном приложении это может быть бесконечный цикл или условие выхода)
//     // WaitForSingleObject(hThreadWatcher, INFINITE);
//     // WaitForSingleObject(hThreadMessendger, INFINITE);

//     // // Освобождаем ресурсы
//     // CloseHandle(hThreadWatcher);
//     // CloseHandle(hThreadMessendger);
//     // CloseHandle(hMutex);
//     // CloseHandle(hDir);

//     printf("Закрытие программы");

//     return 0;
// }





