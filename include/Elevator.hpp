#ifndef ELEVATOR
#define ELEVATOR

#include <memory>
#include <set>
#include <map>
#include <unordered_map>
#include <atomic>

#include "Messaging.hpp"


////////////////     Person     ////////////////
// Структура для описания одного человека.
struct Person
{
    tick_t timestamp;    // Время прибытия.
    ssize_t origin;      // Этаж прибытия.
    ssize_t destination; // Этаж-пункт назначения.
};

////////////////    Elevator    ////////////////
// Класс логики лифта.
class Elevator
{
public:
    enum class Direction
    {
        None,
        Upwards,
        Downwards,
    };

    struct Settings
    {
        uint64_t capacity; // Вместимость.
        tick_t stage;      // Время подъёма на один этаж.
        tick_t open;       // Время открытия дверей.
        tick_t close;      // Время закрытия дверей.
        tick_t idle;       // Время ожидания.
        tick_t in;         // Время входа одного человека.
        tick_t out;        // Время выхода одного человека.
    };

    // Состояния.
    enum class State
    {
        Waiting,      // Режим глубокого ожидания (двери закрыты, вызовов нет).
        MovingUp,     // Движение вверх.
        MovingDown,   // Движение вниз.
        Opening,      // Открытие дверей.
        Idle,         // Ожидание.
        Closing,      // Закрытие дверей.
        Embarking,    // Вход человека.
        Disembarking, // Выход человека.
    };

    struct Incoming : public Message
    {
        enum class Code
        {
            Tick,      // Прошёл интервал времени.
            Call,      // Произошёл вызов.
            Cancel,    // Вызов отменён.
            Embark,    // Попытка входа человека.
            Disembark, // Попытка выхода человека (ага, пытайся, этот лифт кодил самый альтернативно одарённый программист ФУПМа).
        };

        Code code;           // Код сообщения.
        tick_t delta_tick;   // [Tick]:  Прошедшее время.
        ssize_t floor;       // [Call]:  Номер этажа.
        Direction direction; // [Call]:  Направление вызова.
        Person person;       // [Enter]: Входящий человек.
        bool response;       // Требуется ли ответ.
    };

    struct Outcoming : public Message
    {
        enum class Code
        {
            Response,   // Заглушка-ответ.
            Success,    // Успешное выполнение операции.
            Denied,     // Операция запрещена.
            Arrived,    // Прибытие на этаж.
            Departured, // Отбытие с этажа.
            Idling,     // Двери открыты, ожидание.
            InProgress, // В процессе.
            Empty,      // Пустой.
            Full,       // Полный.
        };

        Code code;           // Код сообщения.
        State state;         // Состояние.
        tick_t progress;     // Прогресс.
        Direction direction; // Направление вызова.
        ssize_t floor;       // Номер этажа.
    };

    // Сообщения.
    Messaging<Incoming> inbox;
    Messaging<Outcoming> outbox;

    // Ожидание ответа.
    std::mutex mutex_response;
    std::condition_variable condition_response;

    // Работа.
    std::atomic<bool> working = true;

    Elevator(const Settings& init_settings);
    Elevator(const Elevator& elevator);
    ~Elevator();

    void loop(); // Цикл работы.
    std::vector<Person> get_persons(); // Получение массива находящихся в лифте людей.

    Elevator& operator=(const Elevator& elevator);

protected:
    // Настройки.
    Settings _settings;

    // Время и сообщения.
    mid_t id_counter = 0;
    tick_t timestamp = 0;

    // Состояние.
    State state = State::Waiting;
    tick_t progress = 0;

    // Движение.
    ssize_t floor = 0;
    bool is_destination_selected = false;
    bool is_ignoring_other = false;
    ssize_t destination = 0;
    Direction direction = Direction::None;

    // Присутствующие в лифте люди, отсортированные по этажам.
    std::multimap<ssize_t, Person> floor_person;
    std::shared_mutex mutex_floor_person;

    // Поступившие вызовы.
    std::map<Direction, std::set<ssize_t>> calls;

    bool switch_state(); // Изменить состояние лифта.
    bool _switch_closest();
    bool _switch_selected();
    bool _switch_not_selected();

    // Добавить вызов.
    void insert_call(ssize_t floor, Direction direction);

    // Отменить вызов.
    void erase_call(ssize_t floor, Direction direction);

    // Получить шаблон исходящего сообщения.
    Outcoming _create_outcoming(Outcoming::Code = Outcoming::Code::Success);

private:

};

#endif
