# life-cli
Простая реализация игры "Жизнь" Конвея в окне терминала с использованием символов шрифта Брайля в качестве элементов псевдографики. 
## Компиляция
```
make
```
## Использование
```
Life
```
По умолчанию генерируется поле, случайно заполненное клетками, и используется правило B3/S23, то есть клетка рождается при наличии ровно 3 соседей и выживает, если число соседей равняется 2 или 3.

Для изменения правила используется следующий параметр коммандной строки:
```
-r B1234567890/S1234567890
--rule B1234567890/S1234567890
```
В качестве подчинённого параметра принимается строка, в которой используется два целых числа, уникальные цифры в которых устанавливают, при каком количестве соседей клетка будет рождаться, а при каком выживать.

Для предустановки начального паттерна, который будет использоваться при каждой инициализации поля игры (при старте программы или изменении размера окна терминала) используется следующий параметр:
```
-f <путь/до/файла>
--file <путь/до/файла>
```
Файл, на который указывает путь в параметре должен быть текстовым, причём, пробелы в файле означают мёртвые клетки, а остальные печатные ASCII символы означают живую клетку. Для примера прикреплён файл с глайдером.

Для вывода справки:
```
-h
--help
```
