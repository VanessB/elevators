#include "Controller.hpp"

#define DEBUG_DELAY
//#define DEBUG_MESSAGE_DELAY
//#define DEBUG_INFO
//#define DEBUG_MAIN_MESSAGES

// Короткая запись временных литералов.
using namespace std::chrono_literals;
auto delay = 100ms;

////////////////   Controller   ////////////////
// Класс для управления лифтами.
// PUBLIC:
Controller::Controller(const size_t floors_number, const size_t elevators_number, const Elevator::Settings& default_settings)
{
    // Инициализация лифтов и запуск потоков.
    for (size_t elevator = 0; elevator < elevators_number; ++elevator)
    { elevators.emplace(elevators.end(), default_settings); }
    for (size_t elevator = 0; elevator < elevators_number; ++elevator)
    { elevators_threads.emplace(elevators_threads.end(), &Elevator::loop, &(elevators[elevator])); }

    // Инициализация очередей.
    floor_persons = std::vector<std::list<std::pair<Elevator::Direction, Person>>>(floors_number);

    // Инициализация данных, связанных с отрисовкой.
    elevators_strings = std::vector<std::string>(elevators_number, "[]NW:0");
    elevators_floors = std::vector<size_t>(elevators_number, 0);

}
Controller::~Controller()
{
    // ...
}

void Controller::loop()
{
    Person next_person = {0, 0, 0};
    while (true)
    {
        // Печать информации.
        print_info();

        // Чтение данных о следующем человеке.
        std::cout << "Следующий человек: ";
        std::cin >> next_person.timestamp >> next_person.origin >> next_person.destination;

        // Если время прихода следующего человека ещё не пришло, обрабатываем тик времени.
        while (next_person.timestamp > timestamp)
        {
            // Печать информации.
            print_info();

            // Рассылка сообщения о прошедшем времени.
            {
                #ifdef DEBUG_DELAY
                std::this_thread::sleep_for(delay);
                #endif

                Elevator::Incoming incoming;
                incoming.id = id_counter++;
                incoming.timestamp = timestamp;
                incoming.code = Elevator::Incoming::Code::Tick;
                incoming.delta_tick = 1;
                incoming.response = true;
                timestamp += incoming.delta_tick;

                std::cout << timestamp << std::endl;
                broadcast(incoming);
            }

            // Обработка событий от лифтов.
            for (size_t elevator = 0; elevator < elevators.size(); ++elevator)
            {
                Elevator::Outcoming outcoming;
                bool in_loop = true;
                //while (elevators[i].outbox.try_receive(outcoming))
                while (in_loop)
                {
                    #ifdef DEBUG_MESSAGE_DELAY
                    std::this_thread::sleep_for(delay);
                    #endif

                    outcoming = elevators[elevator].outbox.receive();
                    #ifdef DEBUG_MAIN_MESSAGES
                    std::cout << "Получено сообщение с кодом " << static_cast<int>(outcoming.code) << " от лифта под номером " << elevator << std::endl;
                    #endif

                    switch (outcoming.code)
                    {
                        case Elevator::Outcoming::Code::Response:
                        {
                            in_loop = false;
                            break;
                        }
                        case Elevator::Outcoming::Code::Success: { break; } // Заглушки.
                        case Elevator::Outcoming::Code::Denied:  { break; }
                        case Elevator::Outcoming::Code::Arrived:
                        {
                            #ifdef DEBUG_INFO
                            std::cout << "Лифт " << elevator << " прибыл на этаж " << outcoming.floor << std::endl;
                            #endif

                            // Лифт прибыл, отзываются вызовы по его направлению (если оно не нейтральное).
                            Elevator::Incoming incoming;
                            incoming.id = id_counter++;
                            incoming.timestamp = timestamp;
                            incoming.code = Elevator::Incoming::Code::Cancel;
                            incoming.floor = outcoming.floor;
                            incoming.direction = outcoming.direction;
                            incoming.response = false;

                            if (outcoming.direction != Elevator::Direction::None) { broadcast(incoming); }
                            incoming.direction = Elevator::Direction::None; // Дополнительно отменяется нейтральный вызов для прибывшего лифта.
                            incoming.id = id_counter++;

                            #ifdef DEBUG_MAIN_MESSAGES
                            std::cout << "Отправка сообщения с кодом " << static_cast<int>(incoming.code) << " лифту под номером " << elevator << std::endl;
                            #endif
                            elevators[elevator].inbox.send(incoming);
                            break;
                        }
                        case Elevator::Outcoming::Code::Departured:
                        {
                            #ifdef DEBUG_INFO
                            std::cout << "Лифт " << elevator << " отправился с этажа " << outcoming.floor << std::endl;
                            #endif

                            // При отбытии лифта необходимо заново сделать вызов, если остались люди.
                            std::set<Elevator::Direction> need_recalling;

                            // Проход по очереди людей и получение множества необходимых направлений.
                            for (auto iterator = floor_persons[outcoming.floor].begin(); iterator != floor_persons[outcoming.floor].end(); ++iterator)
                            { need_recalling.insert(iterator->first); }

                            // Проход по требуемым направлениям и вызов лифтов.
                            for (auto iterator = need_recalling.begin(); iterator != need_recalling.end(); ++iterator)
                            {
                                Elevator::Incoming incoming;
                                incoming.id = id_counter++;
                                incoming.timestamp = timestamp;
                                incoming.code = Elevator::Incoming::Code::Call;
                                incoming.floor = outcoming.floor;
                                incoming.direction = *iterator;
                                incoming.response = false;

                                broadcast(incoming);
                            }
                            break;
                        }
                        case Elevator::Outcoming::Code::Idling:
                        {
                            #ifdef DEBUG_INFO
                            std::cout << "Лифт " << elevator << " ожидает на этаже " << outcoming.floor << std::endl;
                            #endif

                            // Пропуск маркера синхронизации.
                            elevators[elevator].outbox.receive();
                            in_loop = false;

                            // Из лифта можно извлечь человека или посадить внутрь.
                            size_t floor = outcoming.floor;

                            // Так как извлечение людей приоритетнее, отправляется сообщение на извлечение очередного человека, для которого этот этаж - пункт назначения.
                            Elevator::Incoming incoming;
                            incoming.id = id_counter++;
                            incoming.timestamp = timestamp;
                            incoming.code = Elevator::Incoming::Code::Disembark;
                            incoming.response = false;

                            #ifdef DEBUG_MAIN_MESSAGES
                            std::cout << "Отправка сообщения с кодом " << static_cast<int>(incoming.code) << " лифту под номером " << elevator << std::endl;
                            #endif
                            elevators[elevator].inbox.send(incoming);
                            outcoming = elevators[elevator].outbox.receive();
                            #ifdef DEBUG_MAIN_MESSAGES
                            std::cout << "Получено сообщение с кодом " << static_cast<int>(outcoming.code) << " от лифта под номером " << elevator << std::endl;
                            #endif

                            switch (outcoming.code)
                            {
                                // Нет ни одного человека, которому требуется выйти на этом этаже.
                                case Elevator::Outcoming::Code::Empty:
                                {
                                    // В случае, если все требуемые люди извлечены, производится посадка.
                                    for (auto iterator = floor_persons[floor].begin(); iterator != floor_persons[floor].end(); ++iterator)
                                    {
                                        // При проходе по очереди пассажиров находится первый, которому нужно ехать в том же направлении, что и лифту.
                                        if (iterator->first == outcoming.direction)
                                        {
                                            // Отправляется сообщение о посадке человека.
                                            incoming.id = id_counter++;
                                            incoming.code = Elevator::Incoming::Code::Embark;
                                            incoming.person = iterator->second;
                                            incoming.response = false;

                                            #ifdef DEBUG_MAIN_MESSAGES
                                            std::cout << "Отправка сообщения с кодом " << static_cast<int>(incoming.code) << " лифту под номером " << elevator << std::endl;
                                            #endif
                                            elevators[elevator].inbox.send(incoming);
                                            outcoming = elevators[elevator].outbox.receive();
                                            #ifdef DEBUG_MAIN_MESSAGES
                                            std::cout << "Получено сообщение с кодом " << static_cast<int>(outcoming.code) << " от лифта под номером " << elevator << std::endl;
                                            #endif

                                            switch (outcoming.code)
                                            {
                                                // Место есть.
                                                case Elevator::Outcoming::Code::Success:
                                                {
                                                    // Пункт назначения добавляется в список вызовов.
                                                    incoming.id = id_counter++;
                                                    incoming.timestamp = timestamp;
                                                    incoming.code = Elevator::Incoming::Code::Call;
                                                    incoming.floor = iterator->second.destination;
                                                    incoming.direction = Elevator::Direction::None;
                                                    incoming.response = false;

                                                    #ifdef DEBUG_MAIN_MESSAGES
                                                    std::cout << "Отправка сообщения с кодом " << static_cast<int>(incoming.code) << " лифту под номером " << elevator << std::endl << std::endl;
                                                    #endif
                                                    elevators[elevator].inbox.send(incoming);

                                                    // При успешной посадке человека он извлекается из очереди.
                                                    floor_persons[floor].erase(iterator);
                                                    break;
                                                }
                                                // Мест нет.
                                                case Elevator::Outcoming::Code::Full: { break; }
                                                default: { break; }
                                            }

                                            // Выход из цикла в любом случае.
                                            break;
                                        }
                                    }
                                    break;
                                }
                                case Elevator::Outcoming::Code::InProgress: { break; }
                                default: { break; }
                            }

                            // Обновление строки, отображающей пассажиров в лифте.
                            {
                                std::vector<Person> __persons = elevators[elevator].get_persons();
                                elevators_strings[elevator] = "[";
                                for (size_t person = 0; person < __persons.size(); ++person)
                                { elevators_strings[elevator] += std::to_string(__persons[person].destination) + (person + 1 == __persons.size() ? "" : " "); }
                                elevators_strings[elevator] += "]";
                            }
                            break;
                        }
                        case Elevator::Outcoming::Code::InProgress: { break; } // Заглушки.
                        case Elevator::Outcoming::Code::Empty:      { break; }
                        case Elevator::Outcoming::Code::Full:       { break; }
                    }
                }

                // Изменение строки состояния лифта согласно последнему принятому сообщению.
                {
                    elevators_floors[elevator] = outcoming.floor; // Обновление этажа лифта.

                    // Список людей.
                    std::vector<Person> __persons = elevators[elevator].get_persons();
                    elevators_strings[elevator] = "[";
                    for (size_t person = 0; person < __persons.size(); ++person)
                    { elevators_strings[elevator] += std::to_string(__persons[person].destination) + (person + 1 == __persons.size() ? "" : " "); }
                    elevators_strings[elevator] += "]";

                    // Направление, состояние и прогресс.
                    switch (outcoming.direction)
                    {
                        case Elevator::Direction::None:      { elevators_strings[elevator] += "N"; break; }
                        case Elevator::Direction::Upwards:   { elevators_strings[elevator] += "U"; break; }
                        case Elevator::Direction::Downwards: { elevators_strings[elevator] += "D"; break; }
                    }
                    switch (outcoming.state)
                    {
                        case Elevator::State::Waiting:      { elevators_strings[elevator] += "W"; break; }
                        case Elevator::State::MovingUp:     { elevators_strings[elevator] += "U"; break; }
                        case Elevator::State::MovingDown:   { elevators_strings[elevator] += "D"; break; }
                        case Elevator::State::Opening:      { elevators_strings[elevator] += "O"; break; }
                        case Elevator::State::Idle:         { elevators_strings[elevator] += "I"; break; }
                        case Elevator::State::Closing:      { elevators_strings[elevator] += "C"; break; }
                        case Elevator::State::Embarking:    { elevators_strings[elevator] += "e"; break; }
                        case Elevator::State::Disembarking: { elevators_strings[elevator] += "d"; break; }
                    }
                    elevators_strings[elevator] += ":" + std::to_string(outcoming.progress);
                }
            }
        }

        // Получение требуемого направления.
        Elevator::Direction direction = Elevator::Direction::None;
        if (next_person.origin > next_person.destination)       { direction = Elevator::Direction::Downwards; }
        else if (next_person.origin < next_person.destination)  { direction = Elevator::Direction::Upwards; }

        // Постановка человека в очередь.
        floor_persons[next_person.origin].push_back(std::make_pair(direction, next_person));

        // Вызов лифта.
        Elevator::Incoming incoming;
        incoming.id = id_counter++;
        incoming.timestamp = timestamp;
        incoming.code = Elevator::Incoming::Code::Call;
        incoming.floor = next_person.origin;
        incoming.direction = direction;
        incoming.response = false;

        broadcast(incoming);
    }
}

// PROTECTED:
void Controller::broadcast(const Elevator::Incoming& message)
{
    #ifdef DEBUG_MAIN_MESSAGES
    std::cout << "Броадкаст сообщения с кодом " << static_cast<int>(message.code) << std::endl;
    #endif
    for (size_t elevator = 0; elevator < elevators.size(); ++elevator)
    {
        #ifdef DEBUG_MAIN_MESSAGES
        std::cout << "   Лфит " << elevator << std::endl;
        #endif

        elevators[elevator].inbox.send(message);

        #ifdef DEBUG_MESSAGE_DELAY
        std::this_thread::sleep_for(delay);
        #endif
    }
}
void Controller::print_info()
{
    // Отрисовка состояния лифтов.
    std::cout << "\033[2J\033[1;1H"; // Очистка экрана.
    std::cout << "Время: " << timestamp << std::endl;
    for (size_t floor = 0; floor < floor_persons.size(); ++floor)
    {
        // Номер этажа.
        std::cout << floor << ". ";

        // Отрисовка лифтов.
        for (size_t elevator = 0; elevator < elevators.size(); ++elevator)
        {
            if (elevators_floors[elevator] == floor) { std::cout << elevators_strings[elevator]; }
            else
            {
                for (size_t i = 0; i < elevators_strings[elevator].size(); ++i)
                { std::cout << " "; }
            }
            std::cout << "|";
        }

        // Отрисовка очереди людей.
        for (auto iterator = floor_persons[floor].begin(); iterator != floor_persons[floor].end(); ++iterator)
        { std::cout << iterator->second.destination << " "; }
        std::cout << std::endl;
    }
}

// PRIVATE:
