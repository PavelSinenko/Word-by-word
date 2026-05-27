#include <gtk/gtk.h>        // библиотека GTK3 для интерфейса
#include <locale.h>         // библиотека для русского языка (для UTF-8)
#include <stdlib.h>         // библиотека для функций rand, srand
#include <stdio.h>          // библиотека для ввода-вывода 
#include <string.h>         // библиотека для строк 
#include <stdbool.h>        // библиотека для true и false
#include <ctype.h>          // библиотека для работы с символами (tolower, isspace)
#include <time.h>           // библиотека для функций времени time, srand
#include <curl/curl.h>      // библиотека libcurl для HTTP-запросов к Wiktionary API

static const char *cubes_letters[10][6] = {  // 10 наборов по 6 букв 
    {"а", "о", "е", "и", "н", "т"},
    {"р", "с", "в", "л", "к", "м"},
    {"д", "п", "у", "я", "ё", "з"},
    {"б", "г", "ж", "й", "х", "ч"},
    {"ш", "щ", "ц", "э", "ю", "ф"},
    {"а", "р", "т", "о", "л", "с"},      
    {"и", "в", "н", "к", "м", "д"},    
    {"п", "у", "б", "г", "ч", "з"},      
    {"ж", "ш", "ц", "щ", "ф", "х"},
    {"е", "д", "р", "с", "л", "а"},
};
static const int positions[] = {1, 2, -1};                      // кубик с позициями
static GtkWidget *label_task;                   // метка для отображения текущего задания
static GtkWidget *label_result;                 // метка для отображения результата проверки слова
static GtkWidget *entry_word;                   // поле ввода, куда пользователь вводит слово
static int current_cube_set_index = 0;            // индекс текущей выбранной буквы 
static int current_position_index = 0;          // индекс текущей выбранной позиции 


// Функция получения позиции русских букв в строке
int utf8_byte_offset(const char* str, int char_pos) { // принимает введенную строку и номер позиции что надо найти
    int byte_pos = 0; // возвращаемая переменная 
    for (int i = 0; i < char_pos && str[byte_pos]; i++) { // цикл идет по символам введенной строки
        byte_pos++;
        while (str[byte_pos] && (str[byte_pos] & 0xC0) == 0x80) { // промежуточная проверка байтов одной буквы
            byte_pos++;
        }
    }
    return byte_pos; // возвращает смещение в байтах до начала нужного символа
}


// Структура для хранения HTTP ответа от сервера
typedef struct {
    char *memory; // указатель на буфер куда идут данные 
    size_t size;  // сколько байт накопилось
} MemoryStruct;

//callback функция libcurl, вызывается когда от сервера приходят данные
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) { // принимает присланные данные, размер одного из элементов, их кол-во, указатель на структуру MemoryStruct
    size_t realsize = size * nmemb;       // вычисление размера полученных данных
    MemoryStruct *mem = (MemoryStruct *)userp; // доступ к структуре

    char *ptr = realloc(mem->memory, mem->size + realsize + 1); // добавление места для новых данных
    if (ptr == NULL) {    // если память кончилась то ошибка
        return 0;
    }

    mem->memory = ptr;                                         // сохраняет новый адрес буфера
    memcpy(&(mem->memory[mem->size]), contents, realsize);     // копирует новую порцию данных в освободившееся место
    mem->size += realsize;                                    // обновляет размер накопленных данных
    mem->memory[mem->size] = 0;                               // записывает \0 в самый конец для корректной строки

    return realsize; // возвращает кол-во обработанных байт
}

//Функция проверки слова онлайн
bool check_online(const char* word) {
    CURL *curl;          // указатель на сессию libcurl 
    CURLcode res;        // код результата выполнения запроса был успех или ошибка
    char url[512];       // строка в которой собирается полный адрес для запроса
    long http_code = 0;  // HTTP-код ответа, 200 успех, 404 не найдено
    

    //структура для хранения ответа от сервера
    MemoryStruct chunk;
    chunk.memory = malloc(1); // выделение минимального кусочка памяти
    chunk.size = 0;           

    if (!chunk.memory) return false; // если память не выделилась то ошибка

    // Инициализация libcurl
    curl = curl_easy_init();
    if (!curl) {
        free(chunk.memory);   // освобождает память
        return false;
    }

    // Кодирование слова для URL
    char *encoded_word = curl_easy_escape(curl, word, 0);
    if (!encoded_word) {
        curl_easy_cleanup(curl);
        free(chunk.memory);
        return false;
    }

    // Формирование строки запроса к Викисловарю
    snprintf(url, sizeof(url), // подставляет закодированное слово вместо %s
        "https://ru.wiktionary.org/w/api.php?action=query&titles=%s&format=json",
        encoded_word);

    curl_easy_setopt(curl, CURLOPT_URL, url);                       // адрес сервера
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback); // функция что принимает данные
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);            // куда заносить данные
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);                 // ждать не больше 10 секунд
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "WordByWord/1.0");  // имя откуда запрос

    //Выполнение запроса
    res = curl_easy_perform(curl);
    
    //Проверка результата
    bool result = false;      //базово ниче нет
    if (res == CURLE_OK) {     //если запрос без ошибок
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);   // получение http кода
        // слово существует если в ответе нет поля "missing"
        if (http_code == 200 && strstr(chunk.memory, "\"missing\"") == NULL) {
            result = true;
        }
    }

    curl_free(encoded_word);    // освобождает закодированное слово
    curl_easy_cleanup(curl);    // закрывает сессию libcurl
    free(chunk.memory);         // освобождает буфер с ответом
    return result;              // возвращает true или false
}

//Функция отображения задания
static void update_task(void) {
    char buffer[256];                               // буфер для текста задания
    char cubes_display[128] = "";                   // строка для отображения букв кубиков
    
    // собирает строку из букв текущего набора кубиков 
    for (int i = 0; i < 6; i++) {
        strcat(cubes_display, cubes_letters[current_cube_set_index][i]);
        if (i < 5) strcat(cubes_display, " ");
    }
    
    int pos = positions[current_position_index];
    if (pos == -1) {
        // позиция "последняя"
        snprintf(buffer, sizeof(buffer), 
                 "Кубики: [ %s ]\nПридумайте слово с одной из букв выше на последней позиции.",
                 cubes_display);
    } else {
        // позиция 1 или 2
        snprintf(buffer, sizeof(buffer), 
                 "Кубики: [ %s ]\nПридумайте слово с одной из букв выше на позиции %d.",
                 cubes_display, pos);
    }
    
    gtk_label_set_text(GTK_LABEL(label_task), buffer);  // выводит задание на экран
    gtk_label_set_text(GTK_LABEL(label_result), "");    // очищает результат
    gtk_entry_set_text(GTK_ENTRY(entry_word), "");      // очищает поле ввода
}

//Функция рандома
static void choose_new_task(void) {
    current_cube_set_index = rand() % 10;                                         // случайный набор кубиков 
    current_position_index = rand() % (sizeof(positions) / sizeof(positions[0])); // случайная позиция 
    update_task();                                                               // обновляет задание на экране
}

//Функция проверки слова
static void on_check_word(GtkButton *button, gpointer user_data) {
    (void)button;                                      
    (void)user_data;                                    
    
    const char *word = gtk_entry_get_text(GTK_ENTRY(entry_word)); // получает введённое слово
    if (g_utf8_strlen(word, -1) == 0) {                           // если поле пустое
        gtk_label_set_text(GTK_LABEL(label_result), "Введите слово.");
        return;
    }

    // приводит слово к нижнему регистру 
    char *lower_word = g_utf8_strdown(word, -1);

    if (!check_online(lower_word)) {    // проверяет существование слова
        gtk_label_set_text(GTK_LABEL(label_result), "Слово не найдено в словаре.");
        g_free(lower_word);          
        return;
    }

    int target_pos = positions[current_position_index]; // берёт позицию из задания
    if (target_pos == -1) {                            
        target_pos = (int)g_utf8_strlen(lower_word, -1); // позиция это длина слова в символах
    }

    if (target_pos > (int)g_utf8_strlen(lower_word, -1)) { // если слово короче, чем нужно
        gtk_label_set_text(GTK_LABEL(label_result), "Слово слишком короткое для этой позиции.");
        g_free(lower_word);
        return;
    }

    int byte_pos = utf8_byte_offset(lower_word, target_pos - 1); // байтовое смещение до нужной буквы
    
    // проверяет есть ли буква на позиции в текущем наборе кубиков
    bool letter_found = false;
    for (int i = 0; i < 6; i++) {
        const char *allowed = cubes_letters[current_cube_set_index][i];
        if (strncmp(&lower_word[byte_pos], allowed, strlen(allowed)) == 0) {
            letter_found = true;                        
            break;
        }
    }
    
    if (letter_found) {
        gtk_label_set_text(GTK_LABEL(label_result), "Поздравляю! Слово подходит.");
    } else {
        gtk_label_set_text(GTK_LABEL(label_result), 
            "Слово есть, но на позиции нет ни одной из букв с кубиков.");
    }
    
    g_free(lower_word);                                 // освобождает память
}

//Функция нового задания
static void on_new_task(GtkButton *button, gpointer user_data) {
    (void)button;                                       
    (void)user_data;                                    
    choose_new_task();                                  // генерирует новое задание
}

//Функция кнопки правил
static void on_show_rules(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    // создаёт модальное окно 
    GtkWidget *rules_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(rules_window), "Правила игры");
    gtk_window_set_modal(GTK_WINDOW(rules_window), TRUE);           
    gtk_window_set_transient_for(GTK_WINDOW(rules_window),           
        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))));
    gtk_container_set_border_width(GTK_CONTAINER(rules_window), 12); 
    gtk_window_set_resizable(GTK_WINDOW(rules_window), FALSE);       
    
    const char *rules_text = 
        "Правила игры «Word by Word»\n\n"
        "1. В каждом раунде выпадают 6 случайных букв\n"
        "   (так называемые «кубики»).\n\n"
        "2. Также выбирается позиция:\n"
        "   • 1 — первая буква слова\n"
        "   • 2 — вторая буква слова\n"
        "   • последняя - последняя буква слова\n\n"
        "3. Ваша задача - придумать русское слово,\n"
        "   содержащее ОДНУ из выпавших букв\n"
        "   на указанной позиции.\n\n"
        "4. Слово проверяется по онлайн-словарю\n"
        "   Wiktionary. Если слова нет в словаре, то\n"
        "   оно не засчитывается.\n\n"
        "5. Можно использовать любую букву\n"
        "   из набора кубиков.\n\n"
        "6. Нажмите на кнопку «Новое задание» чтобы\n"
        "   поменять набор букв на новый.\n\n"
        "Удачи вам!";
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);      // вертикальный контейнер
    gtk_container_add(GTK_CONTAINER(rules_window), vbox);
    
    GtkWidget *label = gtk_label_new(rules_text);                    // метка с текстом правил
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);                     // выравнивание по левому краю
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);                 // перенос строк
    gtk_label_set_max_width_chars(GTK_LABEL(label), 50);             // макс. ширина в символах
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    
    gtk_widget_show_all(rules_window);                               // показывает окно
}

//Главная функция 
int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");                               // включает поддержку UTF-8
    srand((unsigned int) time(NULL));                    // инициализация генератора случайных чисел

    gtk_init(&argc, &argv);                              // запуск системы GTK

    // создаёт главное окно
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Word by Word");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 240); 
    gtk_container_set_border_width(GTK_CONTAINER(window), 16); 

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL); // закрытие окна это выход

    // главный вертикальный контейнер 
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    // метка с заданием
    label_task = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label_task), 0.0);    
    gtk_box_pack_start(GTK_BOX(main_box), label_task, FALSE, FALSE, 0);

    // контейнер с меткой ваше слово + поле ввода
    GtkWidget *entry_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(main_box), entry_box, FALSE, FALSE, 0);

    GtkWidget *entry_label = gtk_label_new("Ваше слово:");
    gtk_box_pack_start(GTK_BOX(entry_box), entry_label, FALSE, FALSE, 0);

    // поле ввода
    entry_word = gtk_entry_new();   
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_word), "Введите слово здесь"); 
    gtk_box_pack_start(GTK_BOX(entry_box), entry_word, TRUE, TRUE, 0);

    // горизонтальный контейнер для кнопок
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 0);

    // кнопка "Проверить"
    GtkWidget *check_button = gtk_button_new_with_label("Проверить");
    g_signal_connect(check_button, "clicked", G_CALLBACK(on_check_word), NULL); 
    gtk_box_pack_start(GTK_BOX(button_box), check_button, TRUE, TRUE, 0);

    // кнопка "Новое задание"
    GtkWidget *new_button = gtk_button_new_with_label("Новое задание");
    g_signal_connect(new_button, "clicked", G_CALLBACK(on_new_task), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), new_button, TRUE, TRUE, 0);

    // кнопка "Правила"
    GtkWidget *rules_button = gtk_button_new_with_label("Правила");
    g_signal_connect(rules_button, "clicked", G_CALLBACK(on_show_rules), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), rules_button, TRUE, TRUE, 0);

    // метка для вывода результата проверки
    label_result = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(label_result), 0.0);
    gtk_box_pack_start(GTK_BOX(main_box), label_result, FALSE, FALSE, 0);

    gtk_widget_show_all(window);                         // отображает всё окно
    gtk_main();                                          // главный цикл GTK или ожидание действий

    return 0;
}

/*
    Заметки по программе «Word by Word»:
    
    Структура программы:
    - Программа случайно выбирает набор из 6 букв «кубиков» и позицию (1, 2 или последняя).
      Пользователь должен ввести русское слово, содержащее одну из букв набора на указанной позиции.
      Слово проверяется через онлайн-словарь Wiktionary API.
    
    Глобальные переменные:
    - cubes_letters[10][6] — 10 наборов «кубиков» по 6 букв кириллицы
    - positions[] = {1, 2, -1} — допустимые позиции 
    - label_task — метка для отображения задания
    - label_result — метка для отображения результата проверки
    - entry_word — поле ввода слова
    - current_cube_set_index — индекс текущего набора кубиков (0..9)
    - current_position_index — индекс текущей позиции (0..2)
    
    Типы данных:
    - MemoryStruct — структура для накопления HTTP-ответа 
    
    Функции программы:
    
    utf8_byte_offset(str, char_pos):
    - вычисляет байтовое смещение для указанного символа в строке UTF-8
    - нужно, потому что русские буквы в UTF-8 занимают 2 байта, а не 1
    - внешний цикл for считает символы, внутренний while пропускает продолженные байты
    - принимает строку и номер символа, возвращает смещение в байтах
    
    write_callback(contents, size, nmemb, userp):
    - callback-функция для libcurl, вызывается при получении каждой порции данных от сервера
    - вычисляет размер порции (size * nmemb)
    - расширяет буфер MemoryStruct через realloc
    - копирует новые данные в конец буфера через memcpy
    - добавляет нуль-терминатор \0 в конец для работы со строковыми функциями
    
    check_online(word):
    - проверяет существование слова через Wiktionary API
    - создаёт сессию libcurl (curl_easy_init)
    - кодирует слово для URL (curl_easy_escape) — русские буквы превращаются в %D0%BA и т.д.
    - формирует URL запроса: https://ru.wiktionary.org/w/api.php?action=query&titles=СЛОВО&format=json
    - настраивает параметры: URL, callback-функцию, таймаут 10 сек, User-Agent
    - выполняет запрос (curl_easy_perform), ответ накапливается в MemoryStruct через write_callback
    - проверяет HTTP-код 200 и отсутствие поля "missing" в JSON-ответе через strstr
    - возвращает true, если слово найдено, иначе false
    - в конце освобождает все ресурсы (curl_free, curl_easy_cleanup, free)
    
    update_task():
    - формирует строку из букв текущего набора кубиков (cubes_display)
    - создаёт текст задания через snprintf с учётом позиции (1, 2 или «последняя»)
    - обновляет метку задания, очищает метку результата и поле ввода
    
    choose_new_task():
    - выбирает случайный набор кубиков: rand() % 10
    - выбирает случайную позицию: rand() % 3 → значение из positions[] = {1, 2, -1}
    - вызывает update_task() для отображения нового задания
    
    on_check_word(button, user_data):
    - обработчик нажатия кнопки «Проверить»
    - получает введённое слово из entry_word
    - проверяет на пустой ввод через g_utf8_strlen
    - приводит слово к нижнему регистру через g_utf8_strdown
    - вызывает check_online() для проверки существования слова
    - определяет целевую позицию: если -1, то позиция = длина слова в символах
    - находит байтовое смещение через utf8_byte_offset
    - сравнивает букву на позиции с буквами текущего набора кубиков (strncmp в цикле)
    - выводит результат в label_result
    - освобождает память через g_free
    
    on_new_task(button, user_data):
    - обработчик нажатия кнопки «Новое задание»
    - вызывает choose_new_task()
    
    on_show_rules(button, user_data):
    - обработчик нажатия кнопки «Правила»
    - создаёт модальное окно с текстом правил игры
    - окно не растягиваемое, закрывается по крестику в заголовке
    
    main(argc, argv):
    - инициализирует локаль (setlocale) для поддержки UTF-8
    - инициализирует генератор случайных чисел (srand(time(NULL)))
    - инициализирует GTK (gtk_init)
    - создаёт главное окно 500×240 с заголовком «Word by Word»
    - создаёт виджеты: метка задания, поле ввода, кнопки, метка результата
    - генерирует первое задание через choose_new_task
    - отображает окно (gtk_widget_show_all) и запускает главный цикл (gtk_main)
    
    Работа с GTK:
    - GtkWindow — главное окно приложения
    - GtkBox (вертикальный и горизонтальный) — контейнеры для размещения виджетов
    - GtkLabel — текстовые метки (задание и результат)
    - GtkEntry — поле ввода слова
    - GtkButton — кнопки «Проверить», «Новое задание», «Правила»
    - g_signal_connect() — связывает события (clicked) с функциями-обработчиками
    
    Проверка буквы на позиции:
    - g_utf8_strlen() — определяет длину строки в символах (не в байтах)
    - utf8_byte_offset() — находит байтовое смещение для указанной позиции символа
    - strncmp(&word[byte_pos], allowed, strlen(allowed)) — сравнивает букву с кубиком
    - возвращает 0, если буквы совпадают
    
    Особенности реализации:
    - srand(time(NULL)) — инициализация генератора случайных чисел текущим временем
    - setlocale(LC_ALL, "") — включает поддержку UTF-8 для работы с русским текстом
    - g_utf8_strdown() — приведение слова к нижнему регистру (заглавные буквы не мешают проверке)
    - gtk_entry_set_placeholder_text() — подсказка в поле ввода
    
    Основная логика: 
    1. Генерируется случайное задание (набор кубиков + позиция)
    2. Пользователь вводит слово
    3. Слово приводится к нижнему регистру
    4. Слово проверяется через Wiktionary API
    5. Если слово существует, проверяется буква на нужной позиции
    6. Выводится результат: подходит / не подходит / не найдено
*/