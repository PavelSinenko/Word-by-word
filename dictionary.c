/**
 * dictionary.c
 *
 * Implementations of dictionary library functionalities
 *
 * load - load dictionary into memory
 * check - check if a word is present in the dictionary
 * size - give number of words in dictionary
 * unload - unload dictionary to free memory
 * unload_rec - recursively unload children nodes from trie
 *
 * Copyright (C)   2015    Madhav Datt
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <curl/curl.h>

#include "dictionary.h"

// For number of words stored in the dictionary loaded
// Gives size of dicitionary
static int number_dict_words = 0;

// Define each node of the trie with 27 children nodes
typedef struct node
{
    bool is_word;
    
    // 0-25 for lowercase a-z, 26 for apostrophe (')
    struct node* next[27]; 
} node;

// Initialize trie to hold dictionary in memory
node* dict = NULL;

/**
 *
 * Checks if word is present in the dictionary - ie. is correctly spelled.
 * Returns true if word is in dictionary else false.
 *
 */
bool check (const char* word)
{
    int index;
    node* tmp = dict;

    int word_len = strlen (word);

    for (int i = 0; i < word_len; i++)
    {
        // Check if word is not just a prefix of a dictionary word
        if (i == word_len - 1 && tmp -> is_word != true)
            return false;

        // Handle ' as a special character
        if (word[i] == '\'')
            index = 26;

        // Handle letters of the alphabet
        // Index between 0 - 25
        else
            index = (int)tolower (word[i]) - 'a';

        // Handle error cases
        if (index > 26 || index < 0)
            continue;
        
        // Check for character in trie
        if (tmp -> next[index] == NULL)
            return false;

        else
            tmp = tmp -> next[index];
    }

    return true;
}

/**
 *
 * Loads dictionary into memory.  Returns true if successful else false.
 * Uses Tries as data structure to store dictionary in memory.
 *
 */
bool load (const char* dictionary)
{

    // Initialize trie for dictionary
    if ((dict = calloc (1, sizeof(node))) == NULL)
    {
        printf ("Out of memory. Dictionary could not be loaded.\n");
        return false;            
    }
    for (int i = 0; i < 26; i++)
        dict -> next[i] = NULL;

	int index;
	node* tmp = dict;

    // Dictionary file
    FILE* dict_file;
    dict_file = fopen (dictionary, "r");

    // Dictionary file not opened
    if (dict_file == NULL)
        return false;

    // Store words from Dictionary file
    char word_str[LENGTH + 1];

    while (fscanf (dict_file, "%s", word_str) != EOF)
    {
        int word_len = strlen (word_str);

        for (int i = 0; i < word_len; i++)
        {
            // Set word as present in dictionary
            if (i == word_len - 1)
                tmp -> is_word = true;

            // Handle ' as a special character
            if (word_str[i] == '\'')
                index = 26;

            // Handle letters of the alphabet
            // Neglect character case
            else if (isalpha (word_str[i]))
                index = (int)tolower (word_str[i]) - 'a';

            // Handle error cases
            if (index > 26 || index < 0)
                continue;

            // Insert character from word read from dictionary into Trie
            if (tmp -> next[index] == NULL)
            {
                // Create and initialize new next nodes for subsequent letter
                if ((tmp -> next[index] = calloc (1, sizeof (node))) == NULL)
                {
                    printf ("Out of memory. Dictionary could not be loaded.\n");
                    return false;
                }
                tmp = tmp -> next[index];
                for (int j = 0; j < 26; j++)
                    tmp -> next[j] = NULL;
            }
            else
                tmp = tmp -> next[index];
        }

        // Insert next word from dictionary
        tmp = dict;
        number_dict_words++; // Dictionary word count
    }
    
    // If end of file not reached before loop termination and error indicator is not set
    if (feof (dict_file) && !ferror (dict_file))
    {
        fclose (dict_file);
        return true;
    }

    fclose (dict_file);
    return false;
}

/**
 *
 * Gives size of dictionary.
 * Returns number of words in dictionary if loaded else 0 if not yet loaded.
 *
 */
unsigned int size (void)
{
    return number_dict_words;
}

/**
 *
 * Recursive function to unload trie from memory.
 *
 */
bool unload_rec (node* dict_rem)
{
    node* tmp = dict_rem;

    // Recursively freeing allocated memory
    for (int i = 0; i < 27; i++)
        if (tmp -> next[i] != NULL)
            unload_rec (tmp -> next[i]);

    free (tmp);
    tmp = NULL;
    
    return true;
}

/**
 *
 * Unloads dictionary from memory.  Returns true if successful else false.
 * Frees allocated memory blocks.
 *
 */
bool unload (void)
{
    return unload_rec (dict);
}

/**
 * Callback function for curl to handle response data
 */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    (void)userp;
    ((char *)contents)[size * nmemb] = 0;
    return size * nmemb;
}

/**
 *
 * Checks if word is present in online dictionary via Free Dictionary API.
 * Returns true if word exists in online dictionary else false.
 *
 */
bool check_online (const char* word)
{
    CURL *curl;
    CURLcode res;
    char url[256];
    long http_code = 0;
    
    // Initialize CURL
    curl = curl_easy_init();
    if (!curl) {
        printf("Ошибка инициализации CURL\n");
        return false;
    }
    
    // Build URL for API request
    snprintf(url, sizeof(url), "https://api.dictionaryapi.dev/api/v2/entries/en/%s", word);
    
    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "WordByWord/1.0");
    
    // Perform request
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        printf("Ошибка подключения к словарю: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return false;
    }
    
    // Get HTTP response code
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    // Cleanup
    curl_easy_cleanup(curl);
    
    // If HTTP 200 - word found, if 404 - not found
    if (http_code == 200) {
        return true;
    } else if (http_code == 404) {
        return false;
    } else {
        printf("Ошибка сервера: %ld\n", http_code);
        return false;
    }
}
