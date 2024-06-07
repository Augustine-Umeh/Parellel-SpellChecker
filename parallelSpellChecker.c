#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>

#define HASHSET_SIZE 200000
#define MAX_MISPELLED_WORDS 200000
#define DELIMITERS " ,.!?;*&$#@:\"()\n\r\t-/_[]/%"
#define MAX_WORD_LENGTH 1024
#define OUTPUTFILE "username_O.out"
#define THREAD_LIMIT 200

typedef struct HashSetNode
{
    char *word;
    struct HashSetNode *next;
} HashSetNode;

typedef struct
{
    char word[MAX_WORD_LENGTH];
    int count;
} WordCount;

typedef struct
{
    HashSetNode *table[HASHSET_SIZE];
} HashSet;

typedef struct
{
    char inputFile[MAX_WORD_LENGTH];
    char dictionaryFile[MAX_WORD_LENGTH];
} TaskArgs;

// Global variables
int totalMisspelled = 0;
int numOfFiles = 0;
int misspelledWords = 0;
char og_file[MAX_WORD_LENGTH];
WordCount aggregateMisspelledWords[MAX_MISPELLED_WORDS] = {0};
int activeTasks = 0;
pthread_mutex_t taskCountMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fileMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t runningTasks[THREAD_LIMIT];
pthread_cond_t conditionVar = PTHREAD_COND_INITIALIZER;
pthread_mutex_t counterMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t consoleMutex = PTHREAD_MUTEX_INITIALIZER;
bool logToFile = false;

// Function prototypes
void freeHashSet(HashSet *set);
unsigned long hashFunction(const char *str);
HashSet *createHashSet();
void insertHashSet(HashSet *set, const char *word);
bool readFileToHashSet(HashSet *set, const char *filename);
void *spellprocessing(void *arg);
void printTopMisspellings();
int checkSpellingAndCountMisspellings(HashSet *set, const char *filename);
int compareWordCount(const void *a, const void *b);
int containsHashSet(HashSet *set, const char *word);
void addCurrentThread(pthread_t thread);
void removeCurrentThread(pthread_t thread);
char *wordCheck(char *word);
bool hasDigit(const char *word);
void toLowerCase(char *word);
void callPythonScript(const char *input_filename, char *output_filename);
void processTask(TaskArgs *task_files);
void joinAllThreads();
void displayMenu();
void handleUserChoice(int user_choice);

// main
int main(int argc, char *argv[])
{

    if (argc > 1 && strcmp(argv[1], "-l") == 0)
    {
        logToFile = true;
    }

    int user_choice;
    while (true)
    {
        displayMenu();

        scanf("%d", &user_choice);
        handleUserChoice(user_choice);
    }
}

void displayMenu()
{
    pthread_mutex_lock(&consoleMutex); // Lock the console output
    printf("\nMain Menu\n");
    printf("Press 1 to start a new spellchecking task\n");
    printf("Press 2 to Exit\n");
    printf("Enter your choice (1-2): ");
    pthread_mutex_unlock(&consoleMutex); // Unlock the console output
}

void handleUserChoice(int user_choice)
{
    switch (user_choice)
    {
    case 1:
    {
        pthread_mutex_lock(&taskCountMutex);
        if (activeTasks >= THREAD_LIMIT)
        {
            pthread_mutex_unlock(&taskCountMutex);
            pthread_mutex_lock(&consoleMutex); // Lock the console output
            printf("Maximum number of active tasks reached.\n");
            pthread_mutex_unlock(&consoleMutex); // Unlock the console output
            break;
        }
        pthread_mutex_unlock(&taskCountMutex);

        TaskArgs *task_files = malloc(sizeof(TaskArgs));
        if (task_files == NULL)
        {
            perror("Failed to allocate memory");
            break;
        }

        pthread_mutex_lock(&consoleMutex);
        printf("Enter the input text file name: ");
        scanf("%s", task_files->inputFile);

        printf("Enter the dictionary file name: ");
        scanf("%s", task_files->dictionaryFile);
        pthread_mutex_unlock(&consoleMutex);

        char processed_input_filename[MAX_WORD_LENGTH];
        char processed_dictionary_filename[MAX_WORD_LENGTH];

        strcpy(og_file, task_files->inputFile);

        callPythonScript(task_files->inputFile, processed_input_filename);
        callPythonScript(task_files->dictionaryFile, processed_dictionary_filename);

        strcpy(task_files->inputFile, processed_input_filename);
        strcpy(task_files->dictionaryFile, processed_dictionary_filename);

        pthread_mutex_lock(&taskCountMutex);

        if (pthread_create(&runningTasks[activeTasks], NULL, spellprocessing, task_files) != 0)
        {
            perror("Failed to create thread");
            free(task_files);
        }
        else
        {
            activeTasks++;
        }
        pthread_mutex_unlock(&taskCountMutex);
        break;
    }

    case 2:
        joinAllThreads();
        if (!logToFile)
        {
            pthread_mutex_lock(&consoleMutex);
            printf("\nProgram closed, below are results\n\n");
            printf("Number of files processed: %d\n", numOfFiles);
            printf("Number of spelling errors: %d\n", misspelledWords);
            printTopMisspellings();
            pthread_mutex_unlock(&consoleMutex);
        }
        exit(0);

    default:
        pthread_mutex_lock(&consoleMutex); // Lock the console output
        printf("Invalid choice. Please enter 1 or 2.\n");
        pthread_mutex_unlock(&consoleMutex); // Unlock the console output
        break;
    }
}

void joinAllThreads()
{
    pthread_mutex_lock(&taskCountMutex);
    for (int i = 0; i < activeTasks; i++)
    {
        if (pthread_join(runningTasks[i], NULL) != 0)
        {
            pthread_mutex_lock(&consoleMutex); // Lock the console output
            fprintf(stderr, "Failed to join thread %d\n", i);
            pthread_mutex_unlock(&consoleMutex); // Unlock the console output
        }
    }
    pthread_mutex_unlock(&taskCountMutex);
}

void *spellprocessing(void *arg)
{

    addCurrentThread(pthread_self());

    TaskArgs *task_files = (TaskArgs *)arg;
    HashSet *set = createHashSet();

    if (!set)
    {
        freeHashSet(set);
        free(task_files);
        removeCurrentThread(pthread_self());
        return NULL;
    }

    if (!readFileToHashSet(set, task_files->dictionaryFile))
    {
        freeHashSet(set);
        free(task_files);
        removeCurrentThread(pthread_self());

        pthread_mutex_lock(&consoleMutex);
        fprintf(stderr, "Failed to open dictionary file: %s\n", task_files->dictionaryFile);
        pthread_mutex_unlock(&consoleMutex);

        return NULL;
    }

    int misspelled = checkSpellingAndCountMisspellings(set, task_files->inputFile);
    misspelledWords += misspelled;

    if (misspelled == 0)
    {
        freeHashSet(set);
        free(task_files);
        removeCurrentThread(pthread_self());
        return NULL;
    }

    pthread_mutex_lock(&taskCountMutex);
    numOfFiles++;
    pthread_mutex_unlock(&taskCountMutex);

    if (logToFile)
    {

        pthread_mutex_lock(&taskCountMutex);
        FILE *dest_file = fopen(OUTPUTFILE, "a");

        if (dest_file != NULL)
        {
            fprintf(dest_file, "%s %d ", og_file, misspelled);
            qsort(aggregateMisspelledWords, totalMisspelled, sizeof(WordCount), compareWordCount);

            for (int i = 0; i < 3; i++)
            {

                fprintf(dest_file, " %s", aggregateMisspelledWords[i].word);
            }

            fprintf(dest_file, "\n");
            fclose(dest_file);
        }
        else
        {

            pthread_mutex_lock(&consoleMutex);
            fprintf(stderr, "Failed to open output file: %s\n", strerror(errno));
            pthread_mutex_unlock(&consoleMutex);
        }

        pthread_mutex_unlock(&taskCountMutex);
    }

    freeHashSet(set);
    free(task_files);
    removeCurrentThread(pthread_self());
    return NULL;
}

void callPythonScript(const char *input_filename, char *output_filename)
{

    // Command to call the Python script
    char command[MAX_WORD_LENGTH];
    sprintf(command, "python3 replaceApostrophe.py %s", input_filename);

    // Open a pipe to read the output of the Python script
    FILE *pipe = popen(command, "r");

    if (!pipe)
    {
        fprintf(stderr, "Failed to run Python script.\n");
        return;
    }

    // Read the output from the Python script
    if (fgets(output_filename, MAX_WORD_LENGTH, pipe) != NULL)
    {

        // Remove the newline character from the output
        size_t len = strlen(output_filename);

        if (len > 0 && output_filename[len - 1] == '\n')
        {
            output_filename[len - 1] = '\0';
        }
    }

    // Close the pipe
    pclose(pipe);
}

HashSet *createHashSet()
{

    HashSet *set = (HashSet *)malloc(sizeof(HashSet));

    if (!set)
    {
        perror("Couldn't create dictionary\n");
        return NULL;
    }

    for (int i = 0; i < HASHSET_SIZE; i++)
    {

        set->table[i] = NULL;
    }

    return set;
}

void printTopMisspellings()
{

    qsort(aggregateMisspelledWords, totalMisspelled, sizeof(WordCount), compareWordCount);

    if (totalMisspelled < 1)
    {
        printf("There are no misspelled words");
    }
    else if (totalMisspelled < 2)
    {
        printf("There is only one misspelled word: ");
    }
    else if (totalMisspelled < 3)
    {
        printf("There are only two misspelled words: ");
    }
    else
    {
        printf("Three most common misspellings: ");
    }

    int limit = totalMisspelled > 3 ? 3 : totalMisspelled;

    for (int i = 0; i < limit; i++)
    {

        if (aggregateMisspelledWords[i].count > 1)
        {
            printf("%s (%d times) ", aggregateMisspelledWords[i].word, aggregateMisspelledWords[i].count);
        }
        else
        {
            printf("%s (%d time) ", aggregateMisspelledWords[i].word, aggregateMisspelledWords[i].count);
        }
    }

    printf("\n\n");
}

int compareWordCount(const void *a, const void *b)
{

    WordCount *wc1 = (WordCount *)a;
    WordCount *wc2 = (WordCount *)b;
    return wc2->count - wc1->count;
}

void addCurrentThread(pthread_t thread)
{

    pthread_mutex_lock(&taskCountMutex);

    for (int i = 0; i < THREAD_LIMIT; i++)
    {

        if (runningTasks[i] == 0)
        {
            runningTasks[i] = thread;
            break;
        }
    }

    pthread_mutex_unlock(&taskCountMutex);
}

void removeCurrentThread(pthread_t thread)
{

    pthread_mutex_lock(&taskCountMutex);

    for (int i = 0; i < THREAD_LIMIT; i++)
    {

        if (runningTasks[i] == thread)
        {
            runningTasks[i] = 0;
            activeTasks--;
            break;
        }
    }

    pthread_mutex_unlock(&taskCountMutex);
}

char *wordCheck(char *word)
{

    // Check and strip non-letter characters from the beginning
    while (*word && !isalpha(*word))
    {
        word++;
    }

    // If the word becomes empty after stripping, return NULL
    if (*word == '\0')
    {
        return NULL;
    }

    // Check and strip non-letter characters from the end
    char *end = word + strlen(word) - 1;
    while (end > word && !isalpha(*end))
    {
        *end = '\0';
        end--;
    }

    // If the resulting word is still empty, return NULL
    if (*word == '\0')
    {
        return NULL;
    }

    // Check if the word has any digits
    if (hasDigit(word))
    {
        return NULL;
    }

    // Convert the word to lowercase
    toLowerCase(word);

    return word;
}

bool hasDigit(const char *word)
{

    while (*word)
    {

        if (isdigit(*word))
        {
            return true;
        }
        word++;
    }

    return false;
}

void toLowerCase(char *word)
{

    while (*word)
    {

        *word = tolower(*word);
        word++;
    }
}

bool readFileToHashSet(HashSet *set, const char *filename)
{

    FILE *file = fopen(filename, "r");

    if (!file)
    {
        perror("Failed to open file");
        return false;
    }

    char line[MAX_WORD_LENGTH];
    char *token;

    while (fgets(line, sizeof(line), file))
    {

        // Remove the newline character if present
        line[strcspn(line, "\n")] = '\0';

        // Split the line by DELIMITERS
        token = strtok(line, DELIMITERS);

        while (token != NULL)
        {

            char *cleanedWord = wordCheck(token);

            if (cleanedWord != NULL)
            {
                if (!containsHashSet(set, cleanedWord))
                {
                    insertHashSet(set, cleanedWord);
                }
            }

            token = strtok(NULL, DELIMITERS);
        }
    }

    fclose(file);
    return true;
}

unsigned long hashFunction(const char *str)
{

    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
    {

        hash = ((hash << 5) + hash) + c;
    }

    return hash % HASHSET_SIZE;
}

void insertHashSet(HashSet *set, const char *word)
{

    if (!set || !word)
    {
        return;
    }

    unsigned long index = hashFunction(word);
    HashSetNode *newNode = (HashSetNode *)malloc(sizeof(HashSetNode));

    if (!newNode)
    {
        return;
    }

    newNode->word = strdup(word);

    if (!newNode->word)
    {
        free(newNode);
        return;
    }

    newNode->next = set->table[index];
    set->table[index] = newNode;
}

int containsHashSet(HashSet *set, const char *word)
{

    unsigned long index = hashFunction(word);
    HashSetNode *node = set->table[index];

    while (node != NULL)
    {

        if (strcmp(node->word, word) == 0)
        {
            return 1;
        }

        node = node->next;
    }

    return 0;
}

void freeHashSet(HashSet *set)
{

    if (!set)
    {
        return;
    }

    for (int i = 0; i < HASHSET_SIZE; i++)
    {

        HashSetNode *current = set->table[i];

        while (current != NULL)
        {

            HashSetNode *temp = current;
            current = current->next;
            free(temp->word);
            free(temp);
        }
    }

    free(set);
}

int checkSpellingAndCountMisspellings(HashSet *set, const char *filename)
{

    FILE *file = fopen(filename, "r");

    if (!file)
    {
        perror("Failed to open file");
        return 0;
    }

    int numMisSpelled = 0;
    char line[MAX_WORD_LENGTH];
    char *token;

    while (fgets(line, sizeof(line), file))
    {

        line[strcspn(line, "\n")] = '\0';

        // Split the line by DELIMITERS
        token = strtok(line, DELIMITERS);

        while (token != NULL)
        {

            char *cleanedWord = wordCheck(token);

            if (cleanedWord != NULL)
            {
                if (!containsHashSet(set, cleanedWord))
                {
                    numMisSpelled++;

                    pthread_mutex_lock(&taskCountMutex);

                    // Check if the word is already in aggregateMisspelledWords
                    bool found = false;
                    for (int i = 0; i < totalMisspelled; i++)
                    {
                        if (strcmp(aggregateMisspelledWords[i].word, cleanedWord) == 0)
                        {
                            aggregateMisspelledWords[i].count++;
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                    {
                        if (totalMisspelled < MAX_MISPELLED_WORDS)
                        {
                            strcpy(aggregateMisspelledWords[totalMisspelled].word, cleanedWord);
                            aggregateMisspelledWords[totalMisspelled].count = 1;
                            totalMisspelled++;
                        }
                    }

                    pthread_mutex_unlock(&taskCountMutex);
                }
            }
            token = strtok(NULL, DELIMITERS);
        }
    }

    fclose(file);
    return numMisSpelled;
}
