# Минимальная версия CMake
cmake_minimum_required(VERSION 3.10)

# Название проекта
project("TCPClientServer")
message("Project name: ${PROJECT_NAME}") # Выведет "TCP_Client_Server_Implementation_in_C"

# Версия стандарта языка C
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)



set(SOURCES_CLIENT
    src/client.c
)

set(SOURCES_SERVER
    src/server.c
)

# Указываем путь до заголовочных файлов
include_directories(${CMAKE_SOURCE_DIR})

# Проверяем, что мы на Windows, и добавляем необходимые библиотеки
if(WIN32)
    # Клиент: используем OpenSSL и Pthread
    find_package(OpenSSL REQUIRED)
    include_directories(${OPENSSL_INCLUDE_DIR})

    # Добавляем флаги для поддержки Pthread (Windows не поддерживает Pthread нативно)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)

    # Добавляем исполняемый файл для клиента
    add_executable(client.c ${SOURCES_CLIENT})
    target_link_libraries(client.c Threads::Threads OpenSSL::SSL OpenSSL::Crypto)

    # Сервер: используем Winsock
    add_executable(server.c ${SOURCES_SERVER})
    target_link_libraries(server.c ws2_32)
else()
    # Если не Windows (например, Linux или macOS), сообщаем об ошибке
    message(FATAL_ERROR "Этот проект предназначен только для Windows.")
endif()


