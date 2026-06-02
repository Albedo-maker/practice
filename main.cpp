/******************************************************************************
*                      КАФЕДРА №304 1 КУРС ПРОГИНЖ                            *
*                           Летняя Практика                                   *
*-----------------------------------------------------------------------------*
* Project Type  : Win32 Console Application                                   *
* Project Name  : Practice                                                    *
* File Name     : main.cpp                                                    *
* Language      : C/C++                                                       *
* Programmer    : Петр Бадрихин                                               *
* Modified By   :                                                             *
* Created       : 11/05/26                                                    *
* Last Revision : 03/06/26                                                    *
* Comment(s)    : Работа со структурами и индексной сортировкой               *
******************************************************************************/

#include <iostream>   // для cout, cerr
#include <fstream>    // для ifstream
#include <iomanip>    // для setw, setfill
#include <cstdio>     // для sscanf
#include <cctype>     // для isdigit

using namespace std;

// ----- константы -----
const int MAX_LINE_LEN = 200;   // хватит для любой разумной строки
const int MAX_BOARD_LEN = 20;   // бортовой номер типа "Б-1234" влезает
const int MAX_TESTS = 18;       // сколько тестовых файлов

// имена тестовых файлов (выбор в меню)
const char* TEST_FILES[MAX_TESTS] = {
    "test_missing.txt", "test_empty.txt", "test_bad_fields.txt",
    "test_bad_extra.txt", "test_bad_board.txt", "test_bad_flightnum.txt",
    "test_bad_pass.txt", "test_bad_time.txt", "test_dup_board.txt",
    "test_dup_flight.txt", "test_mixed_errors.txt", "test_good_basic.txt",
    "test_good_mixed.txt", "test_good_same_time.txt", "test_good_spaces.txt",
    "test_good_max.txt", "test_good_sort.txt", "test_mixed_full.txt"
};

// структура для хранения одной записи
struct FlightRecord {
    int flightNumber;                // номер рейса (натуральное)
    char boardNumber[MAX_BOARD_LEN]; // бортовой номер, например Б-1234
    int passengerCount;              // количество пассажиров (≥0)
    int delayDays;                   // сутки опоздания
    int delayHours;                  // часы (0-23)
    int delayMinutes;                // минуты (0-59)
};

// ----- самодельные функции для работы со строками -----
int my_strlen(const char* s);
void my_strcpy(char* dest, const char* src);
int my_strcmp(const char* a, const char* b);

// ----- прототипы остальных функций -----
void PrintError(int code, const char* filename, int line);
bool IsValidBoardNumber(const char* board);
bool IsValidDelayTime(const char* timeStr, int& days, int& hours, int& minutes);
int ParseAndValidateRecord(const char* line, FlightRecord& rec);
int CheckDuplicate(const FlightRecord* records, int count, const FlightRecord& rec);
int LoadFlightData(const char* filename, FlightRecord*& records, int*& indices, int& outCount);
void SortByDelayDesc(FlightRecord* records, int* indices, int n);
void DisplayTable(FlightRecord* records, int* indices, int n);
int selectTestFile();

// ------------------------------------------------------------------
//  Главная функция
// ------------------------------------------------------------------
int main() {
    cout << "========================================\n";
    cout << "  Вариант №23 – сортировка по опозданию\n";
    cout << "========================================\n";
    int idx = selectTestFile();
    const char* filename = TEST_FILES[idx];

    FlightRecord* records = nullptr;
    int* indices = nullptr;
    int count = 0;
    int err = LoadFlightData(filename, records, indices, count);
    if (err != 0) {
        PrintError(err, filename, -1);
        return 1;
    }
    SortByDelayDesc(records, indices, count);
    DisplayTable(records, indices, count);

    delete[] records;
    delete[] indices;
    return 0;
}

// ------------------------------------------------------------------
//  Реализации функций
// ------------------------------------------------------------------

// считаем длину строки
int my_strlen(const char* s) {
    int len = 0;
    while (s[len]) ++len;
    return len;
}

// копируем строку (как strcpy)
void my_strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

// сравниваем две строки: 0 если равны
int my_strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

// ------------------------------------------------------------------
//  Вывод ошибок в поток cerr (чтобы не мешал таблице)
//  code – номер ошибки, filename – имя файла, line – номер строки
// ------------------------------------------------------------------
void PrintError(int code, const char* filename, int line) {
    cerr << "Ошибка";
    if (line > 0) cerr << " в строке " << line;
    cerr << ": ";
    switch (code) {
        case 1:  cerr << "пустая строка\n"; break;
        case 2:  cerr << "только пробелы\n"; break;
        case 3:  cerr << "не хватает полей (нужно 4)\n"; break;
        case 4:  cerr << "ошибка номера рейса\n"; break;
        case 5:  cerr << "ошибка бортового номера\n"; break;
        case 6:  cerr << "ошибка количества пассажиров\n"; break;
        case 7:  cerr << "ошибка формата времени\n"; break;
        case 8:  cerr << "лишние символы в строке\n"; break;
        case 9:  cerr << "номер рейса должен быть >0\n"; break;
        case 10: cerr << "неверный формат БН (нужно Б-1234 или B-1234)\n"; break;
        case 11: cerr << "пассажиры не могут быть отрицательными\n"; break;
        case 12: cerr << "неверное время (д:чч:мм, чч/мм двузначные)\n"; break;
        case 13: cerr << "дубликат: борт+время уже есть\n"; break;
        case 14: cerr << "дубликат: рейс+время уже есть\n"; break;
        case 15: cerr << "не удалось открыть файл " << filename << '\n'; break;
        case 16: cerr << "нет корректных записей\n"; break;
        default: cerr << "неизвестная ошибка\n";
    }
}

// ------------------------------------------------------------------
//  Проверяем, что бортовой номер имеет вид Б-1234 (русская Б)
//  или B-1234 (латинская). После дефиса обязательно 4 цифры.
// ------------------------------------------------------------------
bool IsValidBoardNumber(const char* board) {
    int len = my_strlen(board);
    // Длина: для латиницы 6 символов, для кириллицы 7 (два байта на 'Б')
    if (len != 6 && len != 7) return false;
    int pos = 0;
    if (len == 6) {
        if (board[0] != 'B') return false;
        pos = 1;
    } else {
        // русская 'Б' в UTF-8 — это два байта 0xD0 0x91
        unsigned char c1 = (unsigned char)board[0];
        unsigned char c2 = (unsigned char)board[1];
        if (!(c1 == 0xD0 && c2 == 0x91)) return false;
        pos = 2;
    }
    if (board[pos] != '-') return false;
    pos++;
    // ровно четыре цифры
    for (int i = 0; i < 4; ++i)
        if (!isdigit((unsigned char)board[pos + i])) return false;
    // после цифр ничего не должно быть
    return (pos + 4 == len);
}

// ------------------------------------------------------------------
//  Проверка времени: строгий формат "д:чч:мм", где часы и минуты
//  обязательно двумя цифрами (01, 02, ..., 23, 00..59).
// ------------------------------------------------------------------
bool IsValidDelayTime(const char* timeStr, int& days, int& hours, int& minutes) {
    int pos = 0;
    if (sscanf(timeStr, "%d%n", &days, &pos) != 1) return false;
    if (days < 0) return false;
    if (timeStr[pos] != ':') return false;
    pos++;
    // часы — две цифры
    if (!isdigit((unsigned char)timeStr[pos]) || !isdigit((unsigned char)timeStr[pos+1])) return false;
    hours = (timeStr[pos] - '0') * 10 + (timeStr[pos+1] - '0');
    if (hours < 0 || hours > 23) return false;
    pos += 2;
    if (timeStr[pos] != ':') return false;
    pos++;
    // минуты — две цифры
    if (!isdigit((unsigned char)timeStr[pos]) || !isdigit((unsigned char)timeStr[pos+1])) return false;
    minutes = (timeStr[pos] - '0') * 10 + (timeStr[pos+1] - '0');
    if (minutes < 0 || minutes > 59) return false;
    pos += 2;
    // после времени не должно быть ничего, кроме пробелов
    while (timeStr[pos] == ' ') pos++;
    if (timeStr[pos] != '\0') return false;
    return true;
}

// ------------------------------------------------------------------
//  Разбор одной строки: номер рейса, борт, пассажиры, время.
//  Возвращает 0, если всё ок, иначе код ошибки.
// ------------------------------------------------------------------
int ParseAndValidateRecord(const char* line, FlightRecord& rec) {
    // пропускаем пробелы в начале
    while (*line == ' ') line++;
    if (*line == '\0') return 1;   // пустая строка

    int flight, passengers;
    char board[MAX_BOARD_LEN];
    char timeStr[30];

    int fields = sscanf(line, "%d %19s %d %29s", &flight, board, &passengers, timeStr);
    if (fields < 4) return 3;   // не хватает полей

    // проверяем, нет ли лишнего мусора после времени
    int consumed;
    sscanf(line, "%*d %*s %*d %*s %n", &consumed);
    while (line[consumed] == ' ') consumed++;
    if (line[consumed] != '\0') return 8;

    if (flight <= 0) return 9;
    if (!IsValidBoardNumber(board)) return 10;
    if (passengers < 0) return 11;

    int days, hours, minutes;
    if (!IsValidDelayTime(timeStr, days, hours, minutes)) return 12;

    // заполняем структуру
    rec.flightNumber = flight;
    my_strcpy(rec.boardNumber, board);
    rec.passengerCount = passengers;
    rec.delayDays = days;
    rec.delayHours = hours;
    rec.delayMinutes = minutes;
    return 0;
}

// ------------------------------------------------------------------
//  Проверяем, не является ли запись дубликатом среди уже добавленных.
//  Возвращает 0, если дубликата нет,
//  13, если совпадает борт+время,
//  14, если совпадает рейс+время.
// ------------------------------------------------------------------
int CheckDuplicate(const FlightRecord* records, int count, const FlightRecord& rec) {
    int t1 = rec.delayDays * 1440 + rec.delayHours * 60 + rec.delayMinutes;
    for (int i = 0; i < count; ++i) {
        int t2 = records[i].delayDays * 1440 + records[i].delayHours * 60 + records[i].delayMinutes;
        if (t1 == t2) {
            if (my_strcmp(records[i].boardNumber, rec.boardNumber) == 0) return 13; // борт+время
            if (records[i].flightNumber == rec.flightNumber) return 14;            // рейс+время
        }
    }
    return 0;
}

// ------------------------------------------------------------------
//  Загрузка данных из файла. Один проход: сразу выводим все ошибки,
//  корректные записи накапливаем в динамическом массиве.
//  Возвращает 0 при успехе, иначе код ошибки.
// ------------------------------------------------------------------
int LoadFlightData(const char* filename, FlightRecord*& records, int*& indices, int& outCount) {
    ifstream file(filename);
    if (!file) return 15;

    int capacity = 10;   // начальный размер массивов
    FlightRecord* tmpRec = new FlightRecord[capacity];
    int* tmpIdx = new int[capacity];
    int count = 0;

    char line[MAX_LINE_LEN];
    int lineNum = 0;
    FlightRecord rec;

    while (file.getline(line, MAX_LINE_LEN)) {
        lineNum++;
        int err = ParseAndValidateRecord(line, rec);
        if (err != 0) {
            PrintError(err, filename, lineNum);   // выводим любую синтаксическую ошибку
            continue;
        }
        int dup = CheckDuplicate(tmpRec, count, rec);
        if (dup != 0) {
            PrintError(dup, filename, lineNum);   // дубликат (13 или 14)
            continue;
        }
        // расширяем массивы, если нужно
        if (count >= capacity) {
            capacity *= 2;
            FlightRecord* newRec = new FlightRecord[capacity];
            int* newIdx = new int[capacity];
            for (int i = 0; i < count; ++i) {
                newRec[i] = tmpRec[i];
                newIdx[i] = tmpIdx[i];
            }
            delete[] tmpRec;
            delete[] tmpIdx;
            tmpRec = newRec;
            tmpIdx = newIdx;
        }
        tmpRec[count] = rec;
        tmpIdx[count] = count;
        count++;
    }
    file.close();

    if (count == 0) {
        delete[] tmpRec;
        delete[] tmpIdx;
        return 16;   // нет корректных записей
    }

    records = tmpRec;
    indices = tmpIdx;
    outCount = count;
    return 0;
}

// ------------------------------------------------------------------
//  Индексная сортировка пузырьком по убыванию времени опоздания.
//  Меняем только индексы, исходный массив не трогаем.
// ------------------------------------------------------------------
void SortByDelayDesc(FlightRecord* records, int* indices, int n) {
    for (int i = 0; i < n-1; ++i) {
        bool swapped = false;
        for (int j = 0; j < n-i-1; ++j) {
            int t1 = records[indices[j]].delayDays * 1440 +
                     records[indices[j]].delayHours * 60 +
                     records[indices[j]].delayMinutes;
            int t2 = records[indices[j+1]].delayDays * 1440 +
                     records[indices[j+1]].delayHours * 60 +
                     records[indices[j+1]].delayMinutes;
            if (t1 < t2) {            // по убыванию
                swap(indices[j], indices[j+1]);
                swapped = true;
            }
        }
        if (!swapped) break;
    }
}

// ------------------------------------------------------------------
//  Выводим таблицу: номер рейса, борт, пассажиры, опоздание.
//  Ширина колонок фиксированная, чтобы не разъезжалась.
// ------------------------------------------------------------------
void DisplayTable(FlightRecord* records, int* indices, int n) {
    cout << "\n----------------------------------------------------------------------\n";
    cout << "      Рейс       Бортовой номер    Пассажиров     Опоздание (д:чч:мм)\n";
    cout << "----------------------------------------------------------------------\n";
    for (int i = 0; i < n; ++i) {
        FlightRecord& rec = records[indices[i]];
        cout << right << setw(10) << rec.flightNumber
             << "         " << left << setw(13) << rec.boardNumber
             << right << setw(10) << rec.passengerCount
             << "           " << rec.delayDays << ":"
             << setfill('0') << setw(2) << rec.delayHours << ":"
             << setw(2) << rec.delayMinutes << setfill(' ') << '\n';
    }
    cout << "----------------------------------------------------------------------\n";
}

// ------------------------------------------------------------------
//  Меню выбора тестового файла.
// ------------------------------------------------------------------
int selectTestFile() {
    cout << "Выберите тестовый файл:\n";
    for (int i = 0; i < MAX_TESTS; ++i)
        cout << "  " << i+1 << ". " << TEST_FILES[i] << '\n';
    int choice;
    cin >> choice;
    while (choice < 1 || choice > MAX_TESTS) {
        cout << "Неверный номер, повторите: ";
        cin >> choice;
    }
    return choice - 1;
}
