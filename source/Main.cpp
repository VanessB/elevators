#include <cinttypes>
#include <iostream>

#include "Controller.hpp"

//#define DEBUG_SETTINGS

int main()
{
    Elevator::Settings default_settings;
    size_t floors_number = 0;
    size_t elevators_number = 0;

    #ifdef DEBUG_SETTINGS
    floors_number = 10;
    elevators_number = 10;
    default_settings.capacity = 5;
    default_settings.stage = 3;
    default_settings.open = 2;
    default_settings.idle = 4;
    default_settings.close = 2;
    default_settings.in = 1;
    default_settings.out = 1;
    #endif

    #ifndef DEBUG_SETTINGS
    std::cout << "\033[2J\033[1;1H"; // Очистка экрана.
    std::cout << "Введите параметры модели: ";
    std::cin >> floors_number >> elevators_number
             >> default_settings.capacity
             >> default_settings.stage
             >> default_settings.open
             >> default_settings.idle
             >> default_settings.close
             >> default_settings.in
             >> default_settings.out;
    #endif

    Controller controller(floors_number, elevators_number, default_settings);
    controller.loop();
    return 0;
}
