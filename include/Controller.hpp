#ifndef CONTROLLER
#define CONTROLLER

#include <list>
#include <memory>
#include <thread>
#include <iostream>
#include "Elevator.hpp"

////////////////   Controller   ////////////////
// Класс для управления лифтами.
class Controller
{
public:
    Controller(const size_t floors_number, const size_t elevators_number, const Elevator::Settings& default_settings);
    ~Controller();

    void loop();

protected:
    // Коммуникация с лифтами.
    mid_t id_counter = 0;
    tick_t timestamp = 0;

    // Структуры, связанные с лифтами.
    std::vector<Elevator> elevators;
    std::vector<std::thread> elevators_threads;

    // Структуры, связанные с людьми.
    std::vector<std::list<std::pair<Elevator::Direction, Person>>> floor_persons; // Массив с людьми для каждого этажа.

    // Структуры, связанные с отрисовкой модели.
    std::vector<std::string> elevators_strings; // Строки, отображающие текущий набор людей в лифте.
    std::vector<size_t> elevators_floors; // Номера текущих этажей лифтов.

    inline void broadcast(const Elevator::Incoming& message); // Рассылка сообщений.
    void print_info(); // Вывод информации.

private:

};

#endif
