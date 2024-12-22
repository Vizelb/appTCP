#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/inotify.h>
#include <signal.h>
#include <pthread.h>
#include <msgpack.h>
#include <openssl/ssl.h>

#define PORT 5555
#define SERVER_IP "127.0.0.1" 
#define BUFFER_SIZE 1024
#define EVENT_SIZE  (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (BUFFER_SIZE * (EVENT_SIZE + 16))

void *event_sender(void *arg);
void *inotify_watcher(void *arg);
void sigterm_handler(int sig);
void compute_sha256(const char *str, char *outputBuffer);
char* get_full_path(const char *dir_path, const char *file_name_path) ;
void send_event(int sock, const char *type_of_event, const char *file_name, const char *sha256);

char dir[BUFFER_SIZE] = {0};
int freq = 0;

int sock = 0;
pthread_t inotify_thread, sender_thread;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

char *full_path;
char *type_of_event;
#define SHA256_BUFF_SIZE 65
char sha256_buff[SHA256_BUFF_SIZE];

int main() {
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    if (signal(SIGTERM, sigterm_handler) == SIG_ERR) {
        perror("error to connect sigterm\n");
        exit(1);
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

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
    printf("-d: %s, -t: %d\n", dir, freq);

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

    const char *directory_to_watch = dir;
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

        char event_buffer[BUFFER_SIZE] = {0};

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
             
                full_path = get_full_path(directory_to_watch, event->name);                

                if (event->mask & IN_CREATE || event->mask & IN_MOVED_TO ){
                    type_of_event = "create";
                }
                else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
                    type_of_event = "delete";
                } else {
                    type_of_event = "unknown";
                }

                compute_sha256(full_path, sha256_buff);

                snprintf(event_buffer, BUFFER_SIZE, "type_of_event: %s, file_name: %.255s, sha256: %s", 
                         type_of_event,
                        full_path,
                        sha256_buff);
                // printf("event buff = %s\n", event_buffer);
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
        send_event(sock, type_of_event, full_path, sha256_buff);
        // send(sock, event_buffer, strlen(event_buffer), 0);
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


// Функция для вычисления SHA256-хэша строки
void compute_sha256(const char *str, char *outputBuffer) {
    unsigned char hash[SHA256_DIGEST_LENGTH]; 
    SHA256_CTX sha256;
    SHA256_Init(&sha256);                     // Инициализация SHA256
    SHA256_Update(&sha256, str, strlen(str)); // Обновление хэша с использованием данных
    SHA256_Final(hash, &sha256);              // Финализация расчета

    // Преобразуем бинарный хэш в строку формата HEX
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
    }
    outputBuffer[SHA256_BUFF_SIZE-1] = 0;
}




void send_event(int sock, const char *type_of_event, const char *file_name, const char *sha256) {
    msgpack_sbuffer buffer;
    msgpack_packer pk;  

    msgpack_sbuffer_init(&buffer);
    msgpack_packer_init(&pk, &buffer, msgpack_sbuffer_write);

    // msgpack_pack_map(&pk, 3);
    // msgpack_pack_str(&pk, "type_of_event");
    // msgpack_pack_str(&pk, type_of_event);
    // msgpack_pack_str(&pk, "file_name");
    // msgpack_pack_str(&pk, file_name);
    // msgpack_pack_str(&pk, "sha256");
    // msgpack_pack_str(&pk, sha256);
    
    msgpack_pack_map(&pk, 3);
    msgpack_pack_str_with_body(&pk, "type_of_event", 12);
    msgpack_pack_str_with_body(&pk, type_of_event, strlen(type_of_event));
    msgpack_pack_str_with_body(&pk, "file_name", 9);
    msgpack_pack_str_with_body(&pk, file_name, strlen(file_name));
    msgpack_pack_str_with_body(&pk, "sha256", 6);
    msgpack_pack_str_with_body(&pk, sha256, strlen(sha256));

    send(sock, buffer.data, buffer.size, 0);
    msgpack_sbuffer_destroy(&buffer);
}
