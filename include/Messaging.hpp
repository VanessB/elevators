#ifndef MESSAGING
#define MESSAGING

#include <cinttypes>
#include <queue>
#include <shared_mutex>
#include <condition_variable>

#include <thread>
#include <iostream>
using namespace std::chrono_literals;

// Типы данных.
typedef uint64_t tick_t; // Тип данных для хранения количества тиков.
typedef int64_t mid_t;   // Message ID. ID сообщения.

////////////////    Message    /////////////////
// Структура-основа для сообщений.
struct Message
{
    mid_t id;         // Идентификатор сообщения.
    tick_t timestamp; // Время прихода сообщения.
};


////////////////   Messaging   /////////////////
// Интерфейс для межпоточного общения путём сообщений.
template<typename T>
class Messaging
{
public:
    Messaging()
    {
        // ...
    }
    Messaging(const Messaging& messaging)
    {
        messages = messaging.messages;
    }
    ~Messaging()
    {
        // ...
    }

    void send(const T& message) // Отправить сообщение.
    {
        std::unique_lock<std::shared_mutex> lock(mutex_messages);
        messages.push(message);
        condition_messages.notify_one();
    }
    T receive() // Принять сообщение.
    {
        T message;
        while (true)
        {
            std::unique_lock<std::shared_mutex> lock(mutex_messages);
            if (messages.empty() == false)
            {
                // Получение верхнего сообщения.
                message = messages.front();
                messages.pop();
                break;
            }
            // Ожидаение в случае отсутствия сообщений.
            else
            { condition_messages.wait(lock); }
        }
        return message;
    }
    bool try_receive(T& message) // Попытка принять сообщение.
    {
        std::unique_lock<std::shared_mutex> lock(mutex_messages);
        if (messages.empty() == false)
        {
            // Получение верхнего сообщения.
            message = messages.front();
            messages.pop();
            return true;
        }
        else { return false; }
    }

    Messaging<T>& operator=(const Messaging<T>& messaging)
    {
        messages = messaging.messages;
        return *this;
    }

protected:
    // Сообщения.
    std::queue<T> messages;
    std::shared_mutex mutex_messages;
    std::condition_variable_any condition_messages;

private:

};

#endif
