#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

int main () {
    char letters[] = {'a', 'b', 'c', 'd', 'e', 'f'};
    int lettersCount = sizeof(letters) / sizeof(letters[0]);
    int positions[] = {1, 2, 3};
    int positionsCount = sizeof(positions) / sizeof(positions[0]);

    srand(time(NULL));

    int randomLetter = rand() % lettersCount;
    int randomPosition = rand() % positionsCount;

    printf("Придумайте слово, в котором будет буква: %c\n", letters[randomLetter]);
    printf("Эта буква должна быть в слове на позиции номер: %i\n", positions[randomPosition]);

    char word[20];

    printf("Введите слово: ");
    fgets(word, sizeof(word), stdin);   
    word[strcspn(word,"\n")] = 0;       

    if (word[randomPosition] == letters[randomLetter]) {
        printf("Красава, хорошо придумал!");
    } else {
        printf("Не, слово не подходит");
    }

    return 0;
}