#include <gtk/gtk.h>        // библиотека GTK3 для интерфейса
#include <locale.h>         // библиотека для русского языка (для UTF-8)
#include <stdlib.h>         // библиотека для функций rand, srand
#include <stdio.h>          // библиотека для ввода-вывода 
#include <string.h>         // библиотека для строк 
#include <stdbool.h>        // библиотека для true и false
#include <ctype.h>          // библиотека для работы с символами (tolower, isspace)
#include <time.h>           // библиотека для функций времени time, srand
#include <curl/curl.h>      // библиотека libcurl для HTTP-запросов к Wiktionary API

static const char *letters[] = {"а", "б", "в", "г", "д", "е"}; // кубик с буквами
static const int positions[] = {1, 2, 3};                      // кубик с позициями
static GtkWidget *label_task;                   // метка для отображения текущего задания
static GtkWidget *label_result;                 // метка для отображения результата проверки слова
static GtkWidget *entry_word;                   // поле ввода, куда пользователь вводит слово
static int current_letter_index = 0;            // индекс текущей выбранной буквы 
static int current_position_index = 0;          // индекс текущей выбранной позиции 

// Получение позиции n-го символа на основе количества символов в строке, а не байтов
int utf8_byte_offset(const char* str, int char_pos) {
    int byte_pos = 0;
    for (int i = 0; i < char_pos && str[byte_pos]; i++) {
        byte_pos++;
        while (str[byte_pos] && (str[byte_pos] & 0xC0) == 0x80) {
            byte_pos++;
        }
    }
    return byte_pos;
}

typedef struct {
    char *memory;
    size_t size;
} MemoryStruct;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL) {
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

bool check_online(const char* word) {
    CURL *curl;
    CURLcode res;
    char url[512];
    long http_code = 0;

    MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    if (!chunk.memory) return false;

    curl = curl_easy_init();
    if (!curl) {
        free(chunk.memory);
        return false;
    }

    char *encoded_word = curl_easy_escape(curl, word, 0);
    if (!encoded_word) {
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return false;
    }

    // Запрос к русскому Викисловарю
    snprintf(url, sizeof(url), 
        "https://ru.wiktionary.org/w/api.php?action=query&titles=%s&format=json",
        encoded_word);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "WordByWord/1.0");

    res = curl_easy_perform(curl);
    
    bool result = false;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        // Страница существует, если в ответе нет поля "missing"
        if (http_code == 200 && strstr(chunk.memory, "\"missing\"") == NULL) {
            result = true;
        }
    }

    curl_free(encoded_word);
    curl_easy_cleanup(curl);
    free(chunk.memory);
    return result;
}
static void update_task(void) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Придумайте слово с буквой \"%s\" на позиции %d.",
             letters[current_letter_index], positions[current_position_index]);
    gtk_label_set_text(GTK_LABEL(label_task), buffer);
    gtk_label_set_text(GTK_LABEL(label_result), "");
    gtk_entry_set_text(GTK_ENTRY(entry_word), "");
}

static void choose_new_task(void) {
    current_letter_index = rand() % (sizeof(letters) / sizeof(letters[0]));
    current_position_index = rand() % (sizeof(positions) / sizeof(positions[0]));
    update_task();
}

static void on_check_word(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    const char *word = gtk_entry_get_text(GTK_ENTRY(entry_word));
    if (g_utf8_strlen(word, -1) == 0) {
        gtk_label_set_text(GTK_LABEL(label_result), "Введите слово.");
        return;
    }

    if (!check_online(word)) {
        gtk_label_set_text(GTK_LABEL(label_result), "Слово не найдено в словаре.");
        return;
    }

    int byte_pos = utf8_byte_offset(word, positions[current_position_index] - 1);
    if (strncmp(&word[byte_pos], letters[current_letter_index], strlen(letters[current_letter_index])) == 0) {
        gtk_label_set_text(GTK_LABEL(label_result), "Поздравляю! Слово подходит.");
    } else {
        gtk_label_set_text(GTK_LABEL(label_result), "Такое слово существует, но не подходит.");
    }
}

static void on_new_task(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    choose_new_task();
}

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
    srand((unsigned int) time(NULL));

    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Word by Word");
    gtk_window_set_default_size(GTK_WINDOW(window), 420, 220);
    gtk_container_set_border_width(GTK_CONTAINER(window), 16);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    label_task = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label_task), 0.0);
    gtk_box_pack_start(GTK_BOX(main_box), label_task, FALSE, FALSE, 0);

    GtkWidget *entry_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(main_box), entry_box, FALSE, FALSE, 0);

    GtkWidget *entry_label = gtk_label_new("Ваше слово:");
    gtk_box_pack_start(GTK_BOX(entry_box), entry_label, FALSE, FALSE, 0);

    entry_word = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_word), "Введите слово здесь");
    gtk_box_pack_start(GTK_BOX(entry_box), entry_word, TRUE, TRUE, 0);

    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 0);

    GtkWidget *check_button = gtk_button_new_with_label("Проверить");
    g_signal_connect(check_button, "clicked", G_CALLBACK(on_check_word), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), check_button, TRUE, TRUE, 0);

    GtkWidget *new_button = gtk_button_new_with_label("Новое задание");
    g_signal_connect(new_button, "clicked", G_CALLBACK(on_new_task), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), new_button, TRUE, TRUE, 0);

    label_result = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label_result), 0.0);
    gtk_box_pack_start(GTK_BOX(main_box), label_result, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}

  /*
    Заметки по программе:
    
    Структура программы:
    - Программа загадывает букву и позицию, пользователь должен ввести слово,
    содержащее эту букву на указанной позиции. Слово проверяется через онлайн-словарь Wiktionary API.
    
    Глобальные переменные:
    - letters[] — массив букв 
    - positions[] — массив позиций 
    - current_letter_index — индекс текущей буквы в массиве letters
    - current_position_index — индекс текущей позиции в массиве positions
    
    Функции программы:
    
    utf8_byte_offset():
    - вычисляет позицию в байтах на основе количества символов
    - нужна, потому что русские буквы в UTF-8 занимают 2 байта, а не 1
    - принимает строку и позицию символа, возвращает смещение в байтах
    
    check_online():
    - проверяет существование слова через Wiktionary API
    - использует libcurl для отправки HTTP-запроса
    - URL: https://ru.wiktionary.org/w/api.php?action=query&titles=СЛОВО&format=json
    - возвращает true, если слово найдено в словаре (нет поля "missing" в ответе)
    - возвращает false, если слово не найдено или произошла ошибка соединения
    
    write_callback():
    - callback-функция для libcurl
    - накапливает ответ от сервера в динамическую строку MemoryStruct
    
    update_task():
    - обновляет текст задания в интерфейсе
    - очищает поле ввода и результат предыдущей проверки
    
    choose_new_task():
    - выбирает случайную букву из массива letters
    - выбирает случайную позицию из массива positions
    - вызывает update_task() для отображения нового задания
    
    on_check_word():
    - обработчик нажатия кнопки "Проверить"
    - получает введённое слово из entry_word
    - проверяет слово через check_online()
    - если слово существует, проверяет букву на нужной позиции
    - выводит результат в label_result
    
    on_new_task():
    - обработчик нажатия кнопки "Новое задание"
    - вызывает choose_new_task()
    
    Проверка буквы на позиции
    - g_utf8_strlen() — определяет длину строки в символах (не в байтах)
    - utf8_byte_offset() — находит байтовое смещение для указанной позиции символа
    - strncmp(&word[byte_pos], letters[...], strlen(...)) — сравнивает буквы
    - возвращает 0, если буквы совпадают
    
    Работа с GTK
    - GtkWindow — главное окно приложения
    - GtkBox (вертикальный и горизонтальный) — контейнеры для размещения виджетов
    - GtkLabel — текстовые метки (задание и результат)
    - GtkEntry — поле ввода слова
    - GtkButton — кнопки "Проверить" и "Новое задание"
    - g_signal_connect() — связывает события (клики) с функциями-обработчиками
    
    Особенности реализации
    - srand(time(NULL)) — инициализация генератора случайных чисел текущим временем
      чтобы при каждом запуске задания были разными
    - setlocale(LC_ALL, "") — включает поддержку UTF-8 для корректной работы с русским текстом
    - gtk_entry_set_placeholder_text() — подсказка в поле ввода
    
    Отличие от предыдущей версии 
    - Раньше использовался Yandex Dictionary API (требовал API-ключ)
    - Теперь используется открытый Wiktionary API (не требует ключа)
    - Функция check_online() переписана под Wiktionary
    
    Основная логика: 
    1. Генерируется случайное задание (буква + позиция)
    2. Пользователь вводит слово
    3. Слово проверяется через Wiktionary API
    4. Если слово существует, проверяется буква на нужной позиции
    5. Выводится результат: подходит / не подходит / не найдено
*/