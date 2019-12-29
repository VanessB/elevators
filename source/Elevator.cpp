#include "Elevator.hpp"
#include <iostream>

//#define DEBUG_SWITCH_STATE
//#define DEBUG_MESSAGE
//#define DEBUG_SWITCH_CLOSEST

////////////////    Elevator    ////////////////
// Класс логики лифта.
// PUBLIC:
Elevator::Elevator(const Settings& init_settings)
{
    _settings = init_settings;
    calls[Direction::None] = std::set<ssize_t>();
    calls[Direction::Upwards] = std::set<ssize_t>();
    calls[Direction::Downwards] = std::set<ssize_t>();
}
Elevator::Elevator(const Elevator& elevator)
{
    // TODO: потокобезопасное копирование.
    id_counter = elevator.id_counter;
    timestamp = elevator.timestamp;
    progress = elevator.progress;
    floor = elevator.floor;
    state = elevator.state;

    // Обработка активного вызова.
    is_destination_selected = elevator.is_destination_selected;
    is_ignoring_other = elevator.is_ignoring_other;
    destination = elevator.destination;
    direction = elevator.direction;

    _settings = elevator._settings;
    floor_person = elevator.floor_person;
    calls = elevator.calls;
    inbox = elevator.inbox;
    outbox = elevator.outbox;
}
Elevator::~Elevator()
{
    // ...
}

void Elevator::loop() // Цикл работы.
{
    while (working.load())
    {
        // Извлечение сообщений, если они есть.
        Incoming incoming = inbox.receive();

        #ifdef DEBUG_MESSAGE
        std::cout << "Получено сообщение. ID: " << incoming.id << " Код: " << static_cast<int>(incoming.code) << std::endl << std::endl;
        #endif

        // Обработка сообщения.
        switch (incoming.code)
        {
            // Прошёл интервал времени.
            case Incoming::Code::Tick:
            {
                timestamp += incoming.delta_tick;
                progress += incoming.delta_tick;
                while (switch_state());
                if (state == State::Waiting) { progress = 0; }

                #ifdef DEBUG_SWITCH_STATE
                {
                    size_t __size = floor_person.size();
                    {
                        std::shared_lock<std::shared_mutex> lock(mutex_floor_person);
                        __size = floor_person.size();
                    }
                    std::cout << "Время: " << timestamp << std::endl
                              << "Этаж: " << floor
                              << "; направление: " << static_cast<int>(direction) << " (" << is_destination_selected << "|" << is_ignoring_other << ")"
                              << "; загруженность: " << __size << "/" << _settings.capacity << std::endl
                              << "Состояние: " << static_cast<int>(state) << " (" << progress << ")" << std::endl << std::endl;
                }
                #endif
                break;
            }
            // Вызов.
            case Incoming::Code::Call:
            {
                insert_call(incoming.floor, incoming.direction);
                break;
            }
            // Отмена вызова.
            case Incoming::Code::Cancel:
            {
                erase_call(incoming.floor, incoming.direction);
                break;
            }
            case Incoming::Code::Embark:
            {
                Outcoming outcoming = _create_outcoming(Outcoming::Code::Success);

                // Если лифт ожидает с открытыми дверями.
                if (state == State::Idle)
                {
                    std::unique_lock<std::shared_mutex> lock(mutex_floor_person);
                    if (floor_person.size() >= _settings.capacity)
                    { outcoming.code = Outcoming::Code::Full; }
                    else
                    {
                        Person entered_person = incoming.person;
                        floor_person.insert(std::pair<size_t, Person>(entered_person.destination, entered_person));
                        progress = 0;
                        state = State::Embarking;
                    }
                }
                // Если происходит посадка/высадка.
                else if ((state == State::Embarking) || (state == State::Disembarking))
                { outcoming.code = Outcoming::Code::InProgress; }
                // Если двери закрыты.
                else
                { outcoming.code = Outcoming::Code::Denied; }

                outbox.send(outcoming);
                break;
            }
            case Incoming::Code::Disembark:
            {
                Outcoming outcoming = _create_outcoming(Outcoming::Code::Success);

                // Если лифт ожидает с открытыми дверями.
                if (state == State::Idle)
                {
                    std::unique_lock<std::shared_mutex> lock(mutex_floor_person);
                    auto found = floor_person.find(floor);
                    // Если людей на выход для текущего этажа нет.
                    if (found == floor_person.end())
                    { outcoming.code = Outcoming::Code::Empty; }
                    // Иначе человек извлекается.
                    else
                    {
                        floor_person.erase(found);
                        progress = 0;
                        state = State::Disembarking;
                    }
                }
                // Если происходит посадка/высадка.
                else if ((state == State::Embarking) || (state == State::Disembarking))
                { outcoming.code = Outcoming::Code::InProgress; }
                // Если двери закрыты.
                else
                { outcoming.code = Outcoming::Code::Denied; }

                outbox.send(outcoming);
                break;
            }
        }

        // В случае, если требуется ответ, происходит отправка требуемого сообщения.
        if (incoming.response)
        {
            Outcoming outcoming = _create_outcoming(Outcoming::Code::Response);
            outbox.send(outcoming);
        }
    }
}
std::vector<Person> Elevator::get_persons() // Получение массива находящихся в лифте людей.
{
    std::shared_lock<std::shared_mutex> lock(mutex_floor_person);
    std::vector<Person> result;
    result.reserve(floor_person.size());
    for (auto iterator = floor_person.begin(); iterator != floor_person.end(); ++ iterator)
    { result.push_back(iterator->second); }
    return result;
}

Elevator& Elevator::operator=(const Elevator& elevator)
{
    // TODO: потокобезопасное копирование.
    id_counter = elevator.id_counter;
    timestamp = elevator.timestamp;
    progress = elevator.progress;
    floor = elevator.floor;
    state = elevator.state;

    // Обработка активного вызова.
    is_destination_selected = elevator.is_destination_selected;
    is_ignoring_other = elevator.is_ignoring_other;
    destination = elevator.destination;
    direction = elevator.direction;

    _settings = elevator._settings;
    floor_person = elevator.floor_person;
    calls = elevator.calls;
    inbox = elevator.inbox;
    outbox = elevator.outbox;

    return *this;
}

// PROTECTED:
bool Elevator::switch_state() // Изменить состояние лифта.
{
    switch (state)
    {
        case State::Waiting:
        {
            return is_destination_selected ? _switch_selected() : _switch_not_selected();
            break;
        }
        case State::MovingUp:
        {
            if (progress >= _settings.stage)
            {
                progress -= _settings.stage;
                ++floor;
                state = State::Waiting;
                return true;
            }
            break;
        }
        case State::MovingDown:
        {
            if (progress >= _settings.stage)
            {
                progress -= _settings.stage;
                --floor;
                state = State::Waiting;
                return true;
            }
            break;
        }
        case State::Opening:
        {
            if (progress >= _settings.open)
            {
                progress -= _settings.open;
                state = State::Idle;
                Outcoming outcoming = _create_outcoming(Outcoming::Code::Idling);
                outbox.send(outcoming);
            }
            break;
        }
        case State::Idle:
        {
            if (progress >= _settings.idle)
            {
                progress -= _settings.idle;
                state = State::Closing;
            }
            break;
        }
        case State::Closing:
        {
            if (progress >= _settings.close)
            {
                progress -= _settings.close;
                state = State::Waiting;
                return true;
            }
            break;
        }
        case State::Embarking:
        {
            if (progress >= _settings.in)
            {
                progress -= _settings.in;
                state = State::Idle;
                Outcoming outcoming = _create_outcoming(Outcoming::Code::Idling);
                outbox.send(outcoming);
            }
            break;
        }
        case State::Disembarking:
        {
            if (progress >= _settings.out)
            {
                progress -= _settings.out;
                state = State::Idle;
                Outcoming outcoming = _create_outcoming(Outcoming::Code::Idling);
                outbox.send(outcoming);
            }
            break;
        }
    }
    return false;
}
// Вспомогательные функции переключения состояния.
bool Elevator::_switch_closest()
{
    // Получение отсортированного по удалённости от лифта множества вызовов.
    std::set<std::tuple<ssize_t, ssize_t, Direction>> sorted;
    for (auto iterator = calls.begin(); iterator != calls.end(); ++iterator)
    {
        auto after = iterator->second.lower_bound(floor);
        auto before = after;
        if (before != iterator->second.begin()) { --before; }
        else { before = iterator->second.end(); }

        if (after != iterator->second.end())
        { sorted.insert(std::make_tuple(std::abs(*after - floor), *after, iterator->first)); }
        if (before != iterator->second.end())
        { sorted.insert(std::make_tuple(std::abs(*before - floor), *before, iterator->first)); }
    }

    // Если вызовов нет, продолжается ожидание.
    if (sorted.empty())
    { return false; }

    // Иначе обрабатывается ближайший вызов.
    auto top = sorted.begin();
    is_destination_selected = true;
    destination = std::get<1>(*top);
    direction = std::get<2>(*top);
    is_ignoring_other = true;

    #ifdef DEBUG_SWITCH_CLOSEST
    std::cout << "Новая цель: " << destination << std::endl << std::endl;
    #endif
    return true;
}
bool Elevator::_switch_selected()
{
    if (destination == floor)
    {
        state = State::Waiting;
        is_destination_selected = false;
        is_ignoring_other = false;
        return true;
    }
    else if (destination > floor)
    { state = State::MovingUp; }
    else if (destination < floor)
    { state = State::MovingDown; }

    Outcoming outcoming = _create_outcoming(Outcoming::Code::Departured);
    outbox.send(outcoming);
    return false;
}
bool Elevator::_switch_not_selected()
{
    switch (direction)
    {
        case Direction::None:
        {
            // Обработка вызова на текущем этаже.
            {
                auto n_equal = calls[Direction::None].find(floor);
                if (n_equal != calls[Direction::None].end())
                {
                    is_destination_selected = false;
                    state = State::Opening;

                    Outcoming outcoming = _create_outcoming(Outcoming::Code::Arrived);
                    outbox.send(outcoming);
                    return false;
                }
            }

            // Выбор ближайшего вызова из всех трёх групп.
            return _switch_closest();
        }
        case Direction::Upwards:
        {
            // Обработка вызова на текущем этаже.
            {
                auto n_equal = calls[Direction::None].find(floor);
                auto u_equal = calls[Direction::Upwards].find(floor);
                if ((n_equal != calls[Direction::None].end()) || (u_equal != calls[Direction::Upwards].end()))
                {
                    is_destination_selected = false;
                    state = State::Opening;

                    Outcoming outcoming = _create_outcoming(Outcoming::Code::Arrived);
                    outbox.send(outcoming);
                    return false;
                }
            }

            // Обработка вызовов с этажей выше.
            {
                auto u_after = calls[Direction::Upwards].lower_bound(floor);
                auto n_after = calls[Direction::None].lower_bound(floor);

                if ((u_after != calls[Direction::Upwards].end()) && (n_after != calls[Direction::None].end()))
                {
                    is_destination_selected = true;
                    destination = std::min(*u_after, *n_after);
                    break;
                }
                if (u_after != calls[Direction::Upwards].end())
                {
                    is_destination_selected = true;
                    destination = *u_after;
                    break;
                }
                if (n_after != calls[Direction::None].end())
                {
                    is_destination_selected = true;
                    destination = *n_after;
                    break;
                }
            }

            // Выбор из стоячего положения.
            is_destination_selected = false;
            direction = Direction::None;
            state = State::Waiting;
            break;
        }
        case Direction::Downwards:
        {
            // Обработка вызова на текущем этаже.
            {
                auto d_equal = calls[Direction::Downwards].find(floor);
                auto n_equal = calls[Direction::None].find(floor);
                if ((d_equal != calls[Direction::Downwards].end()) || (n_equal != calls[Direction::None].end()))
                {
                    is_destination_selected = false;
                    state = State::Opening;

                    Outcoming outcoming = _create_outcoming(Outcoming::Code::Arrived);
                    outbox.send(outcoming);
                    return false;
                }
            }

            // Обработка вызовов с этажей ниже.
            {
                auto d_before = calls[Direction::Downwards].lower_bound(floor);
                auto n_before = calls[Direction::None].lower_bound(floor);
                if (d_before != calls[Direction::Downwards].begin()) { --d_before; }
                else { d_before = calls[Direction::Downwards].end(); }
                if (n_before != calls[Direction::None].begin()) { --n_before; }
                else { n_before = calls[Direction::None].end(); }

                if ((d_before != calls[Direction::Downwards].end()) && (n_before != calls[Direction::None].end()))
                {
                    is_destination_selected = true;
                    destination = std::max(*d_before, *n_before);
                    break;
                }
                if (d_before != calls[Direction::Downwards].end())
                {
                    is_destination_selected = true;
                    destination = *d_before;
                    break;
                }
                if (n_before != calls[Direction::None].end())
                {
                    is_destination_selected = true;
                    destination = *n_before;
                    break;
                }
            }

            // Выбор из стоячего положения.
            is_destination_selected = false;
            direction = Direction::None;
            state = State::Waiting;
            break;
        }
    }

    state = State::Waiting;
    return true;
}

// Добавить вызов.
void Elevator::insert_call(ssize_t floor, Direction direction)
{
    calls[direction].insert(floor);
    if (!is_ignoring_other)
    { is_destination_selected = false; }
}

// Отменить вызов.
void Elevator::erase_call(ssize_t floor, Direction direction)
{
    calls[direction].erase(floor);
    if (floor == destination)
    { is_destination_selected = false; }
}

Elevator::Outcoming Elevator::_create_outcoming(Outcoming::Code code)
{
    Outcoming outcoming = { id_counter++, timestamp, code, state, progress, direction, floor };
    return std::move(outcoming);
}

// PRIVATE:
