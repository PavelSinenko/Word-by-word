#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "dictionary.h"

int main () {

    if (!load("Dictionary.txt")) {
        printf("Словарь не загрузился, давай по новой\n");
        return 1; 
    }

    char letters[] = {'a', 'b', 'c', 'd', 'e', 'f'};
    int lettersCount = sizeof(letters) / sizeof(letters[0]);
    int positions[] = {1, 2, 3};
    int positionsCount = sizeof(positions) / sizeof(positions[0]);

    srand(time(NULL));

    int randomLetter = rand() % lettersCount;
    int randomPosition = rand() % positionsCount;

    printf("Придумайте слово, в котором будет буква: %c\n", letters[randomLetter]);
    printf("Эта буква должна быть в слове на позиции номер: %i\n", positions[randomPosition]);
    printf("Слово будет проверяться через английский словарь\n");

    char word[20];

    printf("Введите слово: ");
    fgets(word, sizeof(word), stdin);   
    word[strcspn(word,"\n")] = 0;       

    if (check(word)) {
        printf("Такое слово есть\n");
        if (word[randomPosition] == letters[randomLetter]) {
            printf("И ты красава, хорошо придумал!");
        } else {
            printf("Но нет, оно не подходит");
        }
    } else {
        printf("Ты по-моему перепутал. Такого слова нет\n");
    }

    unload ();
    return 0;
}