# Минимальная версия CMake
cmake_minimum_required(VERSION 3.10)

# Название проекта
project("appTCP")

# Версия стандарта языка C
set(CMAKE_C_STANDARD 17)
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
# if(WIN32)
    # Клиент: используем OpenSSL и Pthread
    find_package(OpenSSL REQUIRED)
    include_directories(${OPENSSL_INCLUDE_DIR})

    # Добавляем флаги для поддержки Pthread (Windows не поддерживает Pthread нативно)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)

    # Добавляем исполняемый файл для клиента
    add_executable(client ${SOURCES_CLIENT})
    target_link_libraries(client Threads::Threads OpenSSL::SSL OpenSSL::Crypto)

    # target_link_libraries(client ws2_32)

    # Сервер: используем Winsock
    add_executable(server ${SOURCES_SERVER})
    # target_link_libraries(server PRIVATE MsgPack::msgpack)
    # target_link_libraries(server ws2_32)
# else()
#     # Если не Windows (например, Linux или macOS), сообщаем об ошибке
#     message(FATAL_ERROR "Этот проект предназначен только для Windows.")
# endif()


