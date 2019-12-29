# elevators
Многопоточная модель лифтов.

## Спецификация
[plroblems23-FUPM.pdf](https://www.babichev.org/os/problems23-FUPM.pdf) - постановка задачи.

## Начало работы
### Построение
Для построения проекта требуется CMake версии не ниже 3.7.2. Листинг команд, используемых для построения проекта из директории, находящейся на два уровня ниже файла CMakeLists.txt:
```
cmake -DCMAKE_BUILD_TYPE=Release ../..
make
```