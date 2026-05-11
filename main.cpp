#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <limits>

using namespace std;

const int MAX_LINE_LEN = 1000;
const int MAX_BOARD_LEN = 100;

// Структура записи о рейсе
struct FlightRecord
{
    int flightNumber;                    // Номер рейса (натуральное число)
    char boardNumber[MAX_BOARD_LEN];     // Бортовой номер
    int passengerCount;                  // Количество пассажиров на борту
    int delayDays, delayHours, delayMinutes; // Опоздание (сутки, часы, минуты)

};

// Массив сообщений об ошибках (коды 1..17)
const char* ErrorMessages[] = {
    "",                                                                     // 0
    "строка пустая",                                                        // 1
    "строка содержит только пробелы",                                       // 2
    "ошибка при чтении номера рейса",                                       // 3
    "ошибка при чтении бортового номера",                                   // 4
    "ошибка при чтении количества пассажиров",                              // 5
    "ошибка при чтении времени опоздания",                                  // 6
    "лишние данные в строке",                                               // 7
    "номер рейса должен быть натуральным числом",                           // 8
    "бортовой номер слишком короткий (минимум 4 байта)",                    // 9
    "первый символ бортового номера не заглавная русская буква",            // 10
    "второй символ бортового номера не дефис",                              // 11
    "после дефиса должны быть только цифры",                                // 12
    "количество пассажиров не может быть отрицательным",                    // 13
    "некорректное время опоздания (сутки>=0, часы 0-23, минуты 0-59)",     // 14
    "",                                                                     // 15 (файл не найден – особый вывод)
    "",                                                                     // 16 (нет корректных строк – особый вывод)
    "строка превышает максимальную длину"                                   // 17
};

// Вывод сообщения об ошибке в stderr
void PrintError(int code, const char* filename, int line)
{
    cerr << "Ошибка";
    if (line != -1 && code != 16) cerr << " в строке №" << line;
    cerr << ": ";

    if (code == 15)
        cerr << "не удалось открыть файл " << filename << endl;
    else if (code == 16)
        cerr << "в файле " << filename << " нет ни одной корректной строки" << endl;
    else if (code >= 1 && code <= 17 && ErrorMessages[code][0] != '\0')
        cerr << ErrorMessages[code] << endl;
    else
        cerr << "неизвестная ошибка (код " << code << ")" << endl;
}

// Проверка первого символа на заглавную русскую букву (UTF-8)
bool IsRussianCapitalLetter(const char* str)
{
    unsigned char c1 = str[0], c2 = str[1];
    if (c1 == 0xD0 && ((c2 >= 0x90 && c2 <= 0xAF) || c2 == 0x81)) // А-Я, Ё
        return true;
    return false;
}

// Чтение строки, разбор, проверка корректности; возврат 0 или кода ошибки
int ReadFlightRecord(ifstream& file, FlightRecord& record, bool printLine = false, int lineNum = -1)
{
    char line[MAX_LINE_LEN];
    if (!file.getline(line, MAX_LINE_LEN)) {
        if (file.eof()) return -1;
        file.clear();
        file.ignore(numeric_limits<streamsize>::max(), '\n');
        return 17; // слишком длинная строка
    }

    if (printLine)
        cout << "Строка №" << lineNum + 1 << ": \"" << line << "\"" << endl;

    int len = 0;
    while (line[len] != '\0') len++;
    if (len == 0) return 1;        // пусто

    bool onlySpaces = true;
    for (int i = 0; i < len; i++)
        if (!isspace(line[i])) { onlySpaces = false; break; }
    if (onlySpaces) return 2;      // только пробелы

    int flightNum, passengers, days, hours, minutes, charsRead;
    char boardNum[MAX_BOARD_LEN];

    int fieldsRead = sscanf(line, "%d %99s %d %d:%d:%d %n",
                            &flightNum, boardNum, &passengers,
                            &days, &hours, &minutes, &charsRead);
    if (fieldsRead < 4) {
        if (fieldsRead < 1) return 3;
        if (fieldsRead < 2) return 4;
        if (fieldsRead < 3) return 5;
        return 6;
    }

    // Проверка на лишние символы после времени
    int i = charsRead;
    while (line[i] == ' ') i++;
    if (line[i] != '\0') return 7;

    if (flightNum <= 0) return 8;

    // Бортовой номер: длина >=4 байт, заглавная русская, дефис, цифры
    if (strlen(boardNum) < 4) return 9;
    if (!IsRussianCapitalLetter(boardNum)) return 10;
    if (boardNum[2] != '-') return 11;
    for (int j = 3; j < strlen(boardNum); j++)
        if (!isdigit(boardNum[j])) return 12;

    if (passengers < 0) return 13;
    if (days < 0 || hours < 0 || hours > 23 || minutes < 0 || minutes > 59) return 14;

    // Заполнение структуры
    record.flightNumber = flightNum;
    int k = 0;
    while (boardNum[k] != '\0' && k < MAX_BOARD_LEN - 1)
        record.boardNumber[k] = boardNum[k++];
    record.boardNumber[k] = '\0';
    record.passengerCount = passengers;
    record.delayDays = days;
    record.delayHours = hours;
    record.delayMinutes = minutes;
    return 0;
}

// Загрузка данных из файла (два прохода), выделение памяти, заполнение массивов
int LoadFlightData(const char* filename, FlightRecord*& records, int*& indexArray, int& validRecords)
{
    ifstream file(filename);
    if (!file) return 15;

    int goodLines = 0, errorCode, lineNumber = 0;
    FlightRecord tempRecord;

    // Первый проход – подсчёт корректных строк
    while (true) {
        errorCode = ReadFlightRecord(file, tempRecord, true, lineNumber);
        if (errorCode == -1) break;
        lineNumber++;
        if (errorCode == 0) {
            goodLines++;
            cout << "В строке №" << lineNumber << " ошибок не обнаружено" << endl;
        } else {
            PrintError(errorCode, filename, lineNumber);
        }
    }
    file.close();

    if (goodLines == 0) return 16;

    records = new FlightRecord[goodLines];
    indexArray = new int[goodLines];

    // Второй проход – заполнение массивов
    ifstream file2(filename);
    int idx = 0;
    lineNumber = 0;
    while (idx < goodLines) {
        errorCode = ReadFlightRecord(file2, tempRecord);
        if (errorCode == -1) break;
        lineNumber++;
        if (errorCode == 0) {
            records[idx] = tempRecord;
            indexArray[idx] = idx++;
        }
    }
    file2.close();

    validRecords = goodLines;
    return 0;
}

// Индексная сортировка (пузырёк) по убыванию времени опоздания
void SortByDelayDesc(FlightRecord* records, int* indices, int n)
{
    for (int i = 0; i < n - 1; i++) {
        bool swapped = false;
        for (int j = 0; j < n - i - 1; j++) {
            int t1 = records[indices[j]].delayDays * 1440 +
                     records[indices[j]].delayHours * 60 +
                     records[indices[j]].delayMinutes;
            int t2 = records[indices[j+1]].delayDays * 1440 +
                     records[indices[j+1]].delayHours * 60 +
                     records[indices[j+1]].delayMinutes;
            if (t1 < t2) {                           // нужно по убыванию
                int tmp = indices[j];
                indices[j] = indices[j+1];
                indices[j+1] = tmp;
                swapped = true;
            }
        }
        if (!swapped) break;
    }
}

// Вывод итоговой таблицы
void DisplayFlightTable(FlightRecord* records, int* indices, int n)
{
    cout << "\nРезультаты сортировки (по убыванию времени опоздания):\n";
    cout << "----------------------------------------------------------------------\n";
    cout << left << setw(14) << "Рейс" << setw(20) << "Борт"
         << setw(10) << "Пассажиры"<<"        "<< "Опоздание (д:чч:мм)\n";
    cout << "----------------------------------------------------------------------\n";

    for (int i = 0; i < n; i++) {
        FlightRecord& rec = records[indices[i]];
        cout << left << setw(10) << rec.flightNumber
             << setw(20) << rec.boardNumber
             << setw(15) << rec.passengerCount
             << rec.delayDays << ":"
             << setfill('0') << setw(2) << rec.delayHours << ":"
             << setw(2) << rec.delayMinutes << setfill(' ') << endl;
    }
    cout << "----------------------------------------------------------------------\n";
}
// main
int main()
{
    const char* filename = "data.txt";
    FlightRecord* records = nullptr;
    int* sortIndices = nullptr;
    int validRecords = 0;

    cout << "Чтение данных из файла...\n";
    int errorCode = LoadFlightData(filename, records, sortIndices, validRecords);
    if (errorCode != 0) return 1;

    cout << "Обработано корректных записей: " << validRecords << endl;
    if (validRecords > 0) {
        SortByDelayDesc(records, sortIndices, validRecords);
        DisplayFlightTable(records, sortIndices, validRecords);
    } else {
        cout << "Нет данных для вывода.\n";
    }

    delete[] records;
    delete[] sortIndices;
    return 0;
}