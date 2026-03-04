#include <stdio.h>
#include <string.h>

int main () {
    char letters[] = {"a"};
    int position = 2;
    printf("Придумайте слово, в котором будет буква: %s\n", letters);
    printf("Эта буква должна быть в слове на позиции номер: %i\n", position);

    char word[20];

    printf("Введите слово: ");
    fgets(word, sizeof(word), stdin);   
    word[strcspn(word,"\n")] = 0;       

    if (word[position - 1] == letters[0]) {
        printf("Красава, хорошо придумал!");
    } else {
        printf("Не, слово не подходит");
    }

    return 0;
}