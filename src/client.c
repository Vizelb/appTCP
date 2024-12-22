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

char dir[BUFFER_SIZE] = {0};
int freq = 0;

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

    if (signal(SIGTERM, sigterm_handler) == SIG_ERR) {
        perror("error to connect sigterm\n");
        exit(1);
    }

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

    // Прием дериктории и частоты от сервера
    int bytes_read = read(sock, buffer, BUFFER_SIZE);
    if (bytes_read <= 0) {
        printf("Failed to receive initial data\n");
        return -1;
    }
    sscanf(buffer, "%s %d", dir, &freq);
    printf("Client received: Directory: %s, Frequency: %d\n", dir, freq);

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
    // pid_t pid = getpid();
    // printf("client get signal SIGTERM - pid %d\n", pid);

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

    const char *directory_to_watch = dir;//"/programming/appTCP/appTCP/build";
    int wd = inotify_add_watch(fd, directory_to_watch, IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
    if (wd == -1) {
        perror("inotify_add_watch");
        pthread_exit(NULL);
    }

    clock_t start_time = clock();           // Запоминаем начальное время
    int timer_seconds = freq;               // Таймер на x секунд

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
                    type_of_event = "create";
                }
                else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
                    type_of_event = "delete";
                } else {
                    type_of_event = "unknown";
                }

                char sha256_buff[65];
                // printf("path %s\n", full_path);
                compute_sha256(full_path, sha256_buff);

                snprintf(event_buffer, BUFFER_SIZE, "type_of_event: %s, file_name: %.255s, sha256: %s", 
                         type_of_event,
                        //  file_name_d,
                        full_path,
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





