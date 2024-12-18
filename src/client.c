#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ws2tcpip.h>
#include <winsock2.h>

//#include <windows.h>

#include <locale.h>

#include <time.h>

#include <pthread.h>
#include <unistd.h>

//#include <openssl/sha.h> // Для расчета SHA256
#include <openssl/ssl.h>
//#include <openssl/err.h>
//#include <openssl/evp.h>
//#include <pNotify.h>     // Заголовочный файл для pNotify (предположительно подключен)

//#include "cJSON.h"

void InitWatchDirectory(const char *path);
//void* WatchDirectory(void);
void* SendMessageToServer();

DWORD WINAPI DirectoryWatcher(LPVOID lpParam);      // не используется


char* calculate_sha256(const char* file_path);      // не используется
DWORD WINAPI DirectoryWatcher_v2(LPVOID lpParam);


void compute_sha256(const char *str, char *outputBuffer);
void create_json_message(const char *type_of_event, const char *file_name);


// Мьютекс для синхронизации доступа к счетчику
//pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

HANDLE hDir;
HANDLE hMutex;

DWORD createCount = 0;  // не используется
DWORD deleteCount = 0;  // не используется

char mes_event[7] = "";


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

    connect(s, &sa, sizeof(sa));    // установка соединения с сервером

    InitWatchDirectory("C:/DanyaMain/Projects_programming/C/C/ClientTCP");


    // Создаем мьютекс
    hMutex = CreateMutex(NULL, FALSE, NULL);
    if (hMutex == NULL) {
        printf("Не удалось создать мьютекс\n");
        CloseHandle(hDir);
        return 1;
    }

    // Создаем поток для наблюдения за директорией
    HANDLE hThreadWatcher = CreateThread(NULL, 0, DirectoryWatcher_v2, NULL, 0, NULL);
    if (hThreadWatcher == NULL) {
        printf("Не удалось создать поток наблюдателя\n");
        CloseHandle(hMutex);
        CloseHandle(hDir);
        return 1;
    }

    // Создаем поток для увеличения счетчика
    HANDLE hThreadMessendger = CreateThread(NULL, 0, SendMessageToServer, NULL, 0, NULL);
    if (hThreadMessendger == NULL) {
        printf("Не удалось создать поток счетчика\n");
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

    printf("Наблюдение за изменениями в директории: %s\n", path);
}

// Функция для обработки изменений в директории
DWORD WINAPI DirectoryWatcher(LPVOID lpParam) {
    DWORD dwBytesReturned;
    BOOL bRet;
    char buffer[1024];
    FILE_NOTIFY_INFORMATION *pNotify;

    while (1) {
        // Ждем изменений в директории
        bRet = ReadDirectoryChangesW(
            hDir,                    // Дескриптор открытой директории
            buffer,                 // Буфер для получения информации об изменениях
            sizeof(buffer),         // Размер буфера
            TRUE,                   // Наблюдать за подкаталогами
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME, // Типы изменений
            &dwBytesReturned,       // Количество байт, записанных в буфер
            NULL,                   // Не используется
            NULL                    // Не используется
        );

        if (!bRet) {
            printf("Ошибка при чтении изменений в директории\n");
            break;
        }

        pNotify = (FILE_NOTIFY_INFORMATION *)buffer;

        do {
            // Защищаем доступ к счетчикам с помощью мьютекса
            WaitForSingleObject(hMutex, INFINITE);

            if (pNotify->Action == FILE_ACTION_ADDED) {
                createCount++;
                printf("Файл создан: %S\n", pNotify->FileName);

                //memcpy(pNotify->FileName, "0", sizeof(pNotify->FileName));
                memcpy(mes_event, "create", sizeof("create"));
                //flag_event = 1;
            } else if (pNotify->Action == FILE_ACTION_REMOVED) {
                deleteCount++;
                printf("Файл удален: %S\n", pNotify->FileName);
                //memcpy(pNotify->FileName, "0", sizeof(pNotify->FileName));
                memcpy(mes_event, "delete", sizeof("delete"));
                //flag_event = 1;
            }

            // Освобождаем мьютекс
            ReleaseMutex(hMutex);

            // Переходим к следующему уведомлению, если оно есть
            if (pNotify->NextEntryOffset == 0)
                break;
            pNotify = (FILE_NOTIFY_INFORMATION *)((char *)pNotify + pNotify->NextEntryOffset);
        } while (TRUE);
    }

    return 0;
}

// Функция для обработки изменений в директории
DWORD WINAPI DirectoryWatcher_v2(LPVOID lpParam) {
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
            unsigned char hash_name[128] = "C:/DanyaMain/Projects_programming/C/C/ClientTCP/";
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

void* SendMessageToServer()
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
    return NULL;

}



// Функция для вычисления SHA-256 файла
char* calculate_sha256(const char* file_path) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        printf("Не удалось открыть файл: %s\n", file_path);
        return NULL;
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_sha256();
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    if (!EVP_DigestInit_ex(mdctx, md, NULL)) {
        fclose(file);
        EVP_MD_CTX_free(mdctx);
        return NULL;
    }

    // Читаем файл по блокам и обновляем хэш
    unsigned char buffer[4096];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (!EVP_DigestUpdate(mdctx, buffer, bytes_read)) {
            fclose(file);
            EVP_MD_CTX_free(mdctx);
            return NULL;
        }
    }

    if (!EVP_DigestFinal_ex(mdctx, md_value, &md_len)) {
        fclose(file);
        EVP_MD_CTX_free(mdctx);
        return NULL;
    }

    fclose(file);
    EVP_MD_CTX_free(mdctx);

    // Преобразуем бинарный хэш в строку
    char *hex_digest = (char *)malloc(2 * md_len + 1);
    for (unsigned int i = 0; i < md_len; ++i) {
        sprintf(hex_digest + (i * 2), "%02x", md_value[i]);
    }
    hex_digest[2 * md_len] = '\0';

    return hex_digest;
}






// ver 2.0
//
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
    printf("\nСформированное JSON-сообщение:\n%s\n", json_message);
}





//







void* WatchDirectory(void)
{
    char buffer[1024];
    DWORD bytesReturned;

    //int timer = 0;
    while (1) {
        // Блокируем мьютекс перед доступом к счетчику
        //pthread_mutex_lock(&mutex);

        // Читаем изменения в директории
        if (ReadDirectoryChangesW(
            hDir,                                // Дескриптор директории
            &buffer,                             // Буфер для событий
            sizeof(buffer),                      // Размер буфера
            TRUE,                                // Рекурсивное наблюдение за поддиректориями   FALSE - только за текущей директорией
            FILE_NOTIFY_CHANGE_FILE_NAME |       // Отслеживаем следующие изменения:
            FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_ATTRIBUTES |
            FILE_NOTIFY_CHANGE_SIZE |
            FILE_NOTIFY_CHANGE_LAST_WRITE |
            FILE_NOTIFY_CHANGE_LAST_ACCESS |
            FILE_NOTIFY_CHANGE_CREATION |
            FILE_NOTIFY_CHANGE_SECURITY,
            &bytesReturned,                      // Количество возвращённых байт
            NULL,
            NULL
        ) == 0) {
            printf("Ошибка чтения изменений. Код ошибки: %lu\n", GetLastError());
            break;
        }

        // Обработка событий
            FILE_NOTIFY_INFORMATION *info = (FILE_NOTIFY_INFORMATION *)buffer;
            do {
                // Получаем имя файла
                char filename[MAX_PATH];
                int count = WideCharToMultiByte(
                    CP_UTF8,
                    0,
                    info->FileName,
                    info->FileNameLength / sizeof(WCHAR),
                    filename,
                    sizeof(filename),
                    NULL,
                    NULL
                );
                filename[count] = '\0'; // Завершаем строку

                // Определяем тип события
                switch (info->Action) {
                    case FILE_ACTION_ADDED:
                        printf("Файл создан: %s\n", filename);
                        //info = (FILE_NOTIFY_INFORMATION *)((char *)info + info->NextEntryOffset);
                        //return;
                        break;
                    case FILE_ACTION_REMOVED:
                        printf("Файл удалён: %s\n", filename);
                        //info = (FILE_NOTIFY_INFORMATION *)((char *)info + info->NextEntryOffset);
                        //return;
                        break;
                    case FILE_ACTION_MODIFIED:
                        printf("Файл изменён: %s\n", filename);
                        //info = (FILE_NOTIFY_INFORMATION *)((char *)info + info->NextEntryOffset);
                        //return;
                        break;
                    case FILE_ACTION_RENAMED_OLD_NAME:
                        printf("Файл переименован (старое имя): %s\n", filename);
                        //info = (FILE_NOTIFY_INFORMATION *)((char *)info + info->NextEntryOffset);
                        //return;
                        break;
                    case FILE_ACTION_RENAMED_NEW_NAME:
                        printf("Файл переименован (новое имя): %s\n", filename);
                        //info = (FILE_NOTIFY_INFORMATION *)((char *)info + info->NextEntryOffset);
                        //return;
                        break;
                    default:
                        printf("Неизвестное действие с файлом: %s\n", filename);
                        //info = (FILE_NOTIFY_INFORMATION *)((char *)info + info->NextEntryOffset);
                        //return;
                        break;
                }

                // Переходим к следующему событию
                /*if (info->NextEntryOffset == 0) {
                    printf("переход к следующему событию\n");
                    //return;
                    break;
                }*/
                info = (FILE_NOTIFY_INFORMATION *)((char *)info + info->NextEntryOffset);

                //printf("inner - %d\n\r", timer);

            } while (info->NextEntryOffset != 0);
        /*} else {
            printf("Ошибка чтения событий: %lu\n", GetLastError());
            //break;
        }*/
        //timer++;
        //printf("external - %d\n\r", timer);
        //if (timer > 1000000)
            //break;


        // Закрываем дескриптор директории
        CloseHandle(hDir);

        printf("\nexternal - Разблокируем мьютекс после регестрирования события\n");
        // Разблокируем мьютекс после доступа к счетчику
        //pthread_mutex_unlock(&mutex);
    }
    return NULL;
}



// Функция для наблюдения за директорией
/*DWORD WINAPI DirectoryWatcher(LPVOID lpParam) {
    const char* directory = (const char*)lpParam;

    HANDLE hDir = CreateFileA(
        directory,                    // Директория для наблюдения
        FILE_LIST_DIRECTORY,          // Доступ к директории
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, // Совместный доступ
        NULL,                         // Атрибуты безопасности
        OPEN_EXISTING,                // Открытие существующей директории
        FILE_FLAG_BACKUP_SEMANTICS,   // Флаг для работы с директориями
        NULL                          // Шаблон файла
    );

    if (hDir == INVALID_HANDLE_VALUE) {
        printf("Не удалось открыть директорию. Код ошибки: %lu\n", GetLastError());
        return 1;
    }

    printf("Наблюдение за директорией: %s\n", directory);

    char buffer[1024];
    DWORD bytesReturned;

    while (!stop) {
        // Читаем изменения в директории
        if (ReadDirectoryChangesW(
            hDir,                      // Дескриптор директории
            &buffer,                   // Буфер для изменений
            sizeof(buffer),            // Размер буфера
            FALSE,                     // Рекурсивно (FALSE - только текущая директория)
            FILE_NOTIFY_CHANGE_FILE_NAME, // Уведомления о создании/удалении файлов
            &bytesReturned,            // Количество байт записано в буфер
            NULL,                      // Переключатель событий (NULL для синхронного вызова)
            NULL                       // Переменная перекрытия (NULL для синхронного вызова)
        ) == 0) {
            printf("Ошибка чтения изменений. Код ошибки: %lu\n", GetLastError());
            break;
        }

        // Обработка изменений
        FILE_NOTIFY_INFORMATION* pInfo = (FILE_NOTIFY_INFORMATION*)buffer;
        do {
            // Обновляем счетчики в критической секции
            EnterCriticalSection(&cs);
            switch (pInfo->Action) {
                case FILE_ACTION_ADDED:
                    printf("Файл создан\n");
                    creationCount++;
                    break;
                case FILE_ACTION_REMOVED:
                    printf("Файл удален\n");
                    deletionCount++;
                    break;
                default:
                    break;
            }
            LeaveCriticalSection(&cs);

            // Переход к следующему уведомлению (если есть)
            pInfo = (pInfo->NextEntryOffset != 0)
                ? (FILE_NOTIFY_INFORMATION*)((char*)pInfo + pInfo->NextEntryOffset)
                : NULL;
        } while (pInfo != NULL);
    }

    CloseHandle(hDir);
    return 0;
}*/



