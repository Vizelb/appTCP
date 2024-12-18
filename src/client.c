#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ws2tcpip.h>
#include <winsock2.h>

#include <windows.h>

#include <locale.h>

#include <time.h>

#include <pthread.h>
#include <unistd.h>

#include <openssl/ssl.h>

void InitWatchDirectory(const char *path);
DWORD WINAPI SendMessageToServer(LPVOID lpParam);
DWORD WINAPI DirectoryWatcher(LPVOID lpParam);
void compute_sha256(const char *str, char *outputBuffer);
void create_json_message(const char *type_of_event, const char *file_name);

// unsigned char directory_name[128] = "C:/DanyaMain/Projects_programming/C/C/ClientTCP/";  old
const unsigned char directory_name[128] = "C:/DanyaMain/Projects_programming/C/appTCP/build/";

HANDLE hDir;
HANDLE hMutex;

int flag_event = 0;

SOCKET s;           // дескриптор сокета

char json_message[512];

int main()
{
    SSL_library_init();
    SSL_load_error_strings();
    printf("OpenSSL Version: %s\n", OpenSSL_version(OPENSSL_VERSION));

    setlocale(LC_ALL, "Russian");           // для выводы в консоль кириллицы

    printf("I am CLIENT!\n");

    WSADATA ws;
    WSAStartup( MAKEWORD(2, 2), &ws);      // инициализаия использования секитов

    //SOCKET s;           // дескриптор сокета
    s = socket(AF_INET, SOCK_STREAM, 0);

    SOCKADDR_IN sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(5555);


    sa.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

    // sa.sin_addr.s_addr = htonl(INADDR_ANY);

    //sleep(10);

    connect(s, (struct sockaddr *)&sa, sizeof(sa));    // установка соединения с сервером

    // InitWatchDirectory("C:/DanyaMain/Projects_programming/C/C/ClientTCP");
    InitWatchDirectory("C:/DanyaMain/Projects_programming/C/appTCP/build/");


    // Создаем мьютекс
    hMutex = CreateMutex(NULL, FALSE, NULL);
    if (hMutex == NULL) {
        printf("Не удалось создать мьютекс\n");
        CloseHandle(hDir);
        return 1;
    }

    // Создаем поток для наблюдения за директорией
    HANDLE hThreadWatcher = CreateThread(NULL, 0, DirectoryWatcher, NULL, 0, NULL);
    if (hThreadWatcher == NULL) {
        printf("Не удалось создать поток наблюдателя\n");
        CloseHandle(hMutex);
        CloseHandle(hDir);
        return 1;
    }

    // Создаем поток для отправки сообщений
    HANDLE hThreadMessendger = CreateThread(NULL, 0, SendMessageToServer, NULL, 0, NULL);
    if (hThreadMessendger == NULL) {
        printf("Не удалось создать поток\n");
        CloseHandle(hThreadWatcher);
        CloseHandle(hMutex);
        CloseHandle(hDir);
        return 1;
    }

    // Ждем завершения обоих потоков (в реальном приложении это может быть бесконечный цикл или условие выхода)
    WaitForSingleObject(hThreadWatcher, INFINITE);
    WaitForSingleObject(hThreadMessendger, INFINITE);

    // Освобождаем ресурсы
    CloseHandle(hThreadWatcher);
    CloseHandle(hThreadMessendger);
    CloseHandle(hMutex);
    CloseHandle(hDir);

    printf("Закрытие программы");

    return 0;
}


void InitWatchDirectory(const char *path)
{
    // Открываем директорию для наблюдения
    hDir = CreateFile(
        path,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );

    if (hDir == INVALID_HANDLE_VALUE) {
        printf("Не удалось открыть директорию: %s\n", path);
        return;
    }

    // printf("Наблюдение за изменениями в директории: %s\n", path);
    printf("Watch for derictory: %s\n", path);
}


// Функция для обработки изменений в директории
DWORD WINAPI DirectoryWatcher(LPVOID lpParam) {
    DWORD dwBytesReturned;
    BOOL b_event;
    char buffer[4096];
    FILE_NOTIFY_INFORMATION *pNotify;

    while (1) {
        // Ждем изменений в директории
        b_event = ReadDirectoryChangesW(
            hDir,                    // Дескриптор открытой директории
            buffer,                 // Буфер для получения информации об изменениях
            sizeof(buffer),         // Размер буфера
            TRUE,                   // Наблюдать за подкаталогами
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME, // Типы изменений
            &dwBytesReturned,       // Количество байт, записанных в буфер
            NULL,                   // Не используется
            NULL                    // Не используется
        );

        if (!b_event) {
            printf("Ошибка при чтении изменений в директории\n");
            break;
        }

        pNotify = (FILE_NOTIFY_INFORMATION *)buffer;

        do {
            const char *type_of_event;
            // Определяем тип события
            switch (pNotify->Action) {
                case FILE_ACTION_ADDED:
                    type_of_event = "create";
                    //printf("Файл создан\n");
                    break;
                case FILE_ACTION_REMOVED:
                    type_of_event = "delete";
                    //printf("Файл удален\n");
                    break;
                case FILE_ACTION_RENAMED_OLD_NAME :
                    type_of_event = "delete";
                    //printf("Файл удален\n");
                    break;
                case FILE_ACTION_RENAMED_NEW_NAME:
                    type_of_event = "create";
                    //printf("Файл удален\n");
                    break;
                default:
                    break;
            }
            unsigned char hash_name[128] = "C:/DanyaMain/Projects_programming/C/appTCP/build/";
            char *file_name_utf8 = (char *)malloc((wcslen(pNotify->FileName) + 1) * 4);  // UTF-8 может занимать до 4 байт на символ
            wcstombs(file_name_utf8, pNotify->FileName, wcslen(pNotify->FileName) + 1);
            strcat(hash_name, file_name_utf8);
            //printf("\n hash_name - %s\n", hash_name);

            create_json_message(type_of_event, hash_name);

            // Переходим к следующему уведомлению, если оно есть
            if (pNotify->NextEntryOffset == 0)
                break;
            pNotify = (FILE_NOTIFY_INFORMATION *)((char *)pNotify + pNotify->NextEntryOffset);
        } while (TRUE);
    }

    return 0;
}

// void* SendMessageToServer()
DWORD WINAPI SendMessageToServer(LPVOID lpParam)
{
    clock_t start_time = clock(); // Запоминаем начальное время
    int timer_seconds = 2;        // Таймер на 5 секунд

    while (1)
    {
        // Защищаем доступ к счетчикам с помощью мьютекса
        WaitForSingleObject(hMutex, INFINITE);

        if ((clock() - start_time) > timer_seconds * CLOCKS_PER_SEC && flag_event == 1)
        {
            flag_event = 0;
            start_time = clock();

            send(s, json_message, sizeof(json_message), 0);

            //printf("Перед отправкой\n");
            //printf("%s\n", json_message);
            //printf("\n");
            /*char st[20];
            memset(st, 0, sizeof(st));
            recv(s, st, sizeof(st), 0);
            printf(st);*/

        }
        // Освобождаем мьютекс
        ReleaseMutex(hMutex);
    }
    // return NULL;

}

// Функция для вычисления SHA256-хэша строки
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

// Функция для формирования JSON-сообщения
void create_json_message(const char *type_of_event, const char *file_name) {
    char sha256_hash[65]; // Для хранения SHA256-хэша в виде строки (64 символа + \0)

    // Рассчитываем SHA256 для имени файла (или содержимого файла, в зависимости от задачи)
    compute_sha256(file_name, sha256_hash);

    // Формируем JSON-сообщение
    //char json_message[512];
    snprintf(json_message, sizeof(json_message),
             "{\n"
             "  \"type_of_event\": \"%s\",\n"
             "  \"file_name\": \"%s\",\n"
             "  \"sha256\": \"%s\"\n"
             "}",
             type_of_event, file_name, sha256_hash);
    flag_event = 1;
    // Выводим JSON-сообщение
    // printf("\nСформированное JSON-сообщение:\n%s\n", json_message);
    printf("\nFormed JSON-messege:\n%s\n", json_message);
}



