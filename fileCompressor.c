#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/time.h>

#define bufferlen 100

int hashCode(char *str);
int buildCodeBookFile(char * fileName);
struct hashnode * removeFromHeap();
void insertToHeap(struct hashnode * hashNode);
void insertToHash(char * word, int length);
void printTree(struct hashnode * root, int * encoding, int index, int fd);
int storeHuffmanCode();
int compareControl(char * ctrl, int ctrlLen, char * parse, int parLen);
int isFile(char *path);
void listFiles(char * startPath);
void createHashTable(char flaggy);
void createMinHeap();
void updateEscapes();
void initGlobals();
void freeTree(struct hashnode * root);
int compressFile(char * filename, char* huffmanCodeBook);
char * searchReturn(char * word, char flagger);
void cMakeFile(char * fileName);
void cParseForHash(char flagger);
void dirCompressFiles(char * startPath, char flagger);
void reconstructFiles(char * fileName);

struct hashnode{
    char* word;
    char id;
    int length;
    int frequency;
    struct hashnode * next;
    struct hashnode * right;
    struct hashnode * left;
};

struct decompNode{
    char* word;
    char* hashCode;
    struct decompNode * next;
};

struct bufferNode{
    char buffer[bufferlen];
    int length;
    struct bufferNode * next;
};

int uniqueWords = 0;
int currentHeapSize = 0;
int HashTableSize = 0;
int bufferSize = bufferlen;

char * control = NULL;
int controlLength = 0;
int terminate;
int keyHolder[6];
//compress and decompress
char * escapeCodeHold;
struct decompNode ** cHashTable;
//only initialize these if building the initial huffmancodebook
struct bufferNode * dirHead;
struct bufferNode * dirTail;
struct bufferNode * fileHead;
struct hashnode ** hashtable;
struct hashnode ** minHeap;

int main(int argc, char ** argv){
    //the absolute minimum case is ./filecompress -b ./someFile
    //absolute max case is ./filecompress -R -C ./somePath ./Huffmancodebook
    if(argc < 3 ){
        printf("Not enough arguments! \n");
        return 0;
    }
    if(argc > 5 ){
        printf("Too many arguments! \n");
        return 0;
    }
    if(argv[1][1] == 'R'){
        if(argv[2][1] != 'b' && argc < 5){
            //did not provide a huffman code book for operations other than b
            printf("You did not provide a huffman code book for those operations");
            return 0;
        }
    }
     if(argv[1][1] != 'R'){
        if(argv[1][1] != 'b' && argc < 4){
            //did not provide a huffman code book for operations other than b
            printf("You did not provide a huffman code book for those operations");
            return 0;
        }
    }
    initGlobals();
    //second arguemnt is the flags
    if (argv[1][1] == 'R'){
        //recursive directory searching will commence
        if(isFile(argv[3]) != 0){
            printf("did not provide a directory\n");
            return 0;
        }
        if(argv[2][1] == 'b'){
            listFiles(argv[3]);
            if(HashTableSize == 0){
                printf("Did not provide any info to be encoded, now exiting\n");
                return 0;
            }
            createHashTable('d');
            createMinHeap();
            int success  = storeHuffmanCode();
            if(success ==-1){
                return 0;
            }
            free(control);
            //call recursive code book making
        }else if(argv[2][1] == 'c'){
            if(compressFile(argv[3], argv[4]) == -1){
                return 0;
            }
            cParseForHash('c');
            dirCompressFiles(argv[3], 'c');
            if(escapeCodeHold != NULL){
                free(escapeCodeHold);
            }
        }else if(argv[2][1] == 'd'){
            if(compressFile(argv[3], argv[4]) == -1){
                return 0;
            }
            cParseForHash('d');
            dirCompressFiles(argv[3], 'd');
            if(escapeCodeHold != NULL){
                free(escapeCodeHold);
            }

        }else{
            printf("Sorry the flag: %s, is not recognized", argv[2]);
            return 0;
        }
    }else{
       // just a singular file pr
        if( isFile(argv[2]) != 1){
            printf("You did not provide a file name\n");
            return 0;
        }
        if(argv[1][1] == 'b'){
            if(buildCodeBookFile(argv[2])== -1){
                return 0;
            }
            if(HashTableSize == 0){
                printf("Did not provide any info to be encoded, now exiting\n");
                return 0;
            }
            createHashTable('f');
            //now the table should be filled and insertion into minheap commence
            createMinHeap();
//            printf("\n");

            int success  = storeHuffmanCode();
            if(success ==-1){
                return -1;
            }
           // free(control);

        }else if(argv[1][1] == 'c'){
            if(compressFile(argv[2], argv[3]) == -1){
                return 0;
            }
            cParseForHash('c');
            cMakeFile(argv[2]);
            if(escapeCodeHold != NULL){
                free(escapeCodeHold);
            }

        }else if(argv[1][1] == 'd'){
            if(compressFile(argv[2], argv[3]) == -1){
                return 0;
            }
            cParseForHash('d');
            reconstructFiles(argv[2]);
            if(escapeCodeHold != NULL){
                free(escapeCodeHold);
            }
        }else{
            printf("Sorry the flag: %s, is not recognized", argv[1]);
            return 0;
        }
    }
    if(cHashTable != NULL){
        int i;
        for(i = 0; i < HashTableSize; i++){
            if(cHashTable[i] != NULL){
                struct decompNode * ptr = cHashTable[i];
                struct decompNode * lag = ptr;
                while(ptr!=NULL){
                    ptr = ptr->next;
                    free(lag->hashCode);
                    free(lag->word);
                    free(lag);
                    lag = ptr;
                }
            }
        }
        free(cHashTable);
    }
    return 0;
}
void initGlobals(){
dirHead = NULL;
dirTail = NULL;
fileHead = NULL;
hashtable = NULL;
minHeap = NULL;
escapeCodeHold = NULL;
cHashTable = NULL;
return;
}

//build code book
int buildCodeBookFile(char * fileName){

    int fileLen = strlen(fileName);
    if(fileLen > 4){
        if(fileName[fileLen - 4] == '.' &&fileName[fileLen - 3] == 'h' && fileName[fileLen - 2] == 'c' && fileName[fileLen - 1] == 'z'){
            //already decompressed
            printf("file: %s, has already compressed, cannot generate a huffman code book for this\n", fileName);
            return -1;
        }
    }


    int fd = open(fileName, O_RDONLY);

    if(fd < 0){
        printf("Fatal Error: file \"%s\" does not exit\n", fileName);
        return -1;
    }

    //read into buffer struct to then be parsed
    fileHead = (struct bufferNode *)malloc(sizeof(struct bufferNode));
    struct bufferNode * temp = fileHead;
    temp->next = NULL;
    int bytes = 0;
    int totalReadBytes = 0;
    int numLinks = 1;
    while(1){
        bytes = read(fd, (temp->buffer + totalReadBytes), bufferSize - totalReadBytes);
        if(bytes == -1){
            printf("an error has occurred while reading the file...");
            free(fileHead);
            return -1;
        }
        totalReadBytes = totalReadBytes + bytes;
        if(totalReadBytes == bufferSize){
            //need to chain linked list stuff
            temp->length = totalReadBytes;
            bytes = 0;
            totalReadBytes = 0;
            struct bufferNode * ptr = (struct bufferNode *)malloc(sizeof(struct bufferNode));
            temp->next = ptr;
            temp = ptr;
            temp->next=NULL;
            numLinks++;
        }else if(bytes == 0){
            //reached EOF
            temp->length = totalReadBytes;
            break;
        }
    }
    if(temp->length == 0){
        printf("empty file\n");
        return -1;
    }
    HashTableSize = 10*numLinks;
    close(fd);
    return 0;
}

void createMinHeap(){
    struct hashnode * filler = (struct hashnode *)malloc(sizeof(struct hashnode));
    //the hash table is filled out before we get to this point
    minHeap = malloc((uniqueWords + 1)*sizeof(struct heapnode *));
    currentHeapSize = 0;
    minHeap[0] = filler;
    minHeap[0]->frequency = -10; //base or impossible
    int i = 0;
    for ( i = 0; i < HashTableSize; i ++){
        //loop through hash table and insert into minheap!!!
        if(hashtable[i] != NULL){
            //insert chains into minHeap
            struct hashnode * ptr = hashtable[i];
            while(ptr != NULL){
                insertToHeap(ptr);
                ptr = ptr->next;
            }
        }
    }
    free(hashtable);
}

void createHashTable(char flaggy){
    struct bufferNode * head;
    if(flaggy == 'f'){
        //create table for file
        head = fileHead;
    }else{
        //create table for directory
        head = dirHead;
    }
    //now parse word until reach white space
    //check if exists in hashtable, if not add, if yes incriment
    hashtable = malloc(HashTableSize*sizeof(struct heapnode *));
    keyHolder[0] = -1;
    keyHolder[1] = -1;
    keyHolder[2] = -1;
    keyHolder[3] = -1;
    keyHolder[4] = -1;
    keyHolder[5] = -1;

    //create array to hold unique code for control
    int j = 0;
    for ( j = 0; j < HashTableSize; j++){
        hashtable[j] = NULL;
    }
    //controls will be a series of ! followed by designated s n f v etc, will be updated as needed
    controlLength = 2;
    control = (char *)malloc(controlLength*sizeof(char));
    control[0] = '!';
    int size = bufferSize;
    int wordSize = 0; // including terminator, it is like length, 1 longer than index
    char * wordHolder = (char *)malloc(size*sizeof(char));
    int i = 0;
    while(head != NULL){
        if(i == head->length && i != bufferSize){
                //tail case
                if(wordSize > 0){
                    wordHolder[wordSize] = '\0';
                    //inserts at proper place or increments
                    insertToHash(wordHolder, wordSize + 1);
                }
                struct bufferNode * curr = head->next;
                //dont need the buffer node once its done reading it
                free(head);
                head = curr;
        }else if(i == bufferSize){
                //reset buffer
                if(head->next == NULL){
                    //happens to end exactly on
                    char * oneBigger = (char *)malloc((wordSize + 1)*sizeof(char));
                    strcpy(oneBigger, wordHolder);
                    free(wordHolder);
                    wordHolder = oneBigger;
                    wordHolder[wordSize] = '\0';
                    insertToHash(wordHolder, wordSize + 1);
                    struct bufferNode * curr = head->next;
                    free(head);
                    head = curr;
                }else{
                // shouLD ONLY RESIZE WHEN OUT OF SPACE FOR BUFFER
                    struct bufferNode * curr = head->next;
                    free(head);
                    head = curr;
                    i = 0;
                }
        }else if(wordSize == size){
                    size = size + bufferSize;
                    char * x = (char *)malloc(size*sizeof(char));
                    strcpy(x, (const char *)wordHolder);
                    free(wordHolder);
                    wordHolder = x;
        }
        else if(head->buffer[i] == ' '){
            //break the current word and store and search
            if(wordSize > 0){
                wordHolder[wordSize] = '\0';
                //search key insert or incriment
                insertToHash(wordHolder, wordSize + 1);
                //end that
                //store space too
                insertToHash(" ", 2);
            }else{
                insertToHash(" ", 2);
            }
            wordSize = 0;
            i++;
        }else if (head->buffer[i] == '\n'){
            if(wordSize > 0){
                wordHolder[wordSize] = '\0';
                insertToHash(wordHolder, wordSize + 1);
                insertToHash("\n", 2);
            }else{
                insertToHash("\n", 2);
            }
            wordSize = 0;
            i++;
        }else if (head->buffer[i] == '\t'){
            if(wordSize > 0){
                wordHolder[wordSize] = '\0';
                insertToHash(wordHolder, wordSize + 1);
                insertToHash("\t", 2);
            }else{
                insertToHash("\t", 2);
            }
            wordSize = 0;
            i++;
        }else if (head->buffer[i] == '\v'){
            if(wordSize > 0){
                wordHolder[wordSize] = '\0';
                insertToHash(wordHolder, wordSize + 1);
                insertToHash("\v", 2);
            }else{
                insertToHash("\v", 2);
            }
            wordSize = 0;
            i++;
        }else if (head->buffer[i] == '\f'){
            if(wordSize > 0){
                wordHolder[wordSize] = '\0';
                insertToHash(wordHolder, wordSize + 1);
                insertToHash("\f", 2);
            }else{
                insertToHash("\f", 2);
            }
            wordSize = 0;
            i++;
        }else if (head->buffer[i] == '\r'){
            if(wordSize > 0){
                wordHolder[wordSize] = '\0';
                insertToHash(wordHolder, wordSize + 1);
                insertToHash("\r", 2);
            }else{
                insertToHash("\r", 2);
            }
            wordSize = 0;
            i++;
        }else{
            //is not a white space character
            if(head->buffer[i] == '!'){
                //controlLeng is actual length of everything need -1 for length of just !!...!
                //wordSize is actually index but i need length so + 1
                wordHolder[wordSize] = head->buffer[i];
                i++;
                wordSize++;
                int numSame = compareControl(control, controlLength - 1, wordHolder, wordSize);
                //if number returned is = the controlLen then must expand control code
                if(numSame == (controlLength - 1)){
                    //find all n f v and spaces etc. that exist and modify with new control code
                    controlLength++;
                    char * x = (char *)malloc((controlLength)*sizeof(char));
                    strcpy(x, control);
                    free(control);
                    x[controlLength - 2] = '!';
                    control = x;
                    //keep this going to check control char
                    updateEscapes();
                }
            }else{
                wordHolder[wordSize] = head->buffer[i];
                i++;
                wordSize++;
            }
        }
    }
    free(wordHolder);
}
void updateEscapes(){
    int key;

    int i;
    for(i = 0; i < 6; i++){
        if(keyHolder[i] != -1){
            key = keyHolder[i];
            struct hashnode * check = hashtable[key];
            if(check == NULL){
                //lol u crazy no way this would be !
            }else{
                //loop to find and incriment or insert node
                while(check!= NULL){
                    //look for flag else do nothing same as if NULL
                    if(check->id == 's'){
                        char * updated = (char *)malloc((controlLength + 1)*sizeof(char));
                        int i;
                        for(i = 0; i < (controlLength -1); i++){
                            updated[i] = '!';
                        }
                        updated[controlLength -1] = check->word[check->length - 2];
                        updated[controlLength] = '\0';
                        check->length = controlLength + 1;
                        free(check->word);
                        check->word = updated;
                        break;
                    }
                    check = check->next;
                }
            }
        }
    }
}

int storeHuffmanCode(){
    int sizeOfHeap = uniqueWords;
    //huffman building tree
    //special case if only IS one
    if(sizeOfHeap == 1){
        //weird case
        struct hashnode * removed = removeFromHeap();
        struct hashnode * extra = (struct hashnode *)malloc(sizeof(struct hashnode));
        extra->frequency = removed->frequency;
        extra->word = NULL;
        //insert some unlikely char to signal internal node
        extra->left= removed;
    }else{
        while(sizeOfHeap > 1){
            //first is smaller  or equal f to second f
            //smaller goes on left bigger on right
            struct hashnode * first = removeFromHeap();
            struct hashnode * second = removeFromHeap();

            struct hashnode * newNode = (struct hashnode *)malloc(sizeof(struct hashnode));
            newNode->frequency = first->frequency + second->frequency;
            newNode->word = NULL;
            newNode->left = first;
            newNode->right = second;

            insertToHeap(newNode);
            sizeOfHeap--;
        }
    }
    struct hashnode * root = removeFromHeap();
    int * encoding = (int *)malloc((uniqueWords + 1)*sizeof(int));
    int index = 0;
    int fd = open("HuffmanCodeBook", O_CREAT | O_WRONLY , 00600);

    int bytes = 0;
    int totalBytes = 0;
    while (totalBytes != (controlLength - 1)){
          bytes = write(fd, (control + totalBytes), ((controlLength-1) - totalBytes));
          totalBytes = totalBytes + bytes;
    }
    write(fd, "\n", 1);
    printTree(root, encoding, index, fd);
    free(control);
    freeTree(root);
    free(encoding);

    close(fd);
    terminate = -1;
    free(minHeap[0]);
    free(minHeap);
    return 0;
}

void freeTree(struct hashnode * root){
    if(root == NULL){
        return;
    }
    freeTree(root->right);
    freeTree(root->left);

    free(root->word);
    free(root);
}

int isFile(char *path){
    DIR * dir = opendir(path);
    if(dir != NULL){
        closedir(dir);
        return 0;
    }
    if(errno == ENOTDIR){
        return 1;
    }
    return -1;
}

void listFiles(char * startPath){
    //create char * path
    //global
    char path2[1000];
    DIR * dir = opendir(startPath);

    if(dir == NULL){
        return;
    }
    struct dirent * dirStuff;
    terminate = 1;
    while((dirStuff = readdir(dir)) != NULL){
       if(strcmp(dirStuff->d_name, ".") != 0 && strcmp(dirStuff->d_name, "..") != 0){
          //  printf("name of dir or file: %s\n", dirStuff->d_name);
        }
        if(strcmp(dirStuff->d_name, ".") != 0 && strcmp(dirStuff->d_name, "..") != 0 ){
            if(dirStuff->d_type == 8){
                char * gg = malloc(1000*sizeof(char));
                strcpy(gg, startPath);
                strcat(gg, "/");
                char * fileName = dirStuff->d_name;
              int fileLen = strlen(fileName);
                if(fileLen > 4){
                     if(fileName[fileLen - 4] == '.' && fileName[fileLen - 3] == 'h' && fileName[fileLen - 2] == 'c' && fileName[fileLen - 1] == 'z'){
                        //already compressed
                        continue;
                    }
                }

                if(strcmp(fileName, "HuffmanCodeBook") == 0){
                    continue;
                }
                strcat(gg, fileName);
                int fd = open(gg, O_RDONLY);

                if(fd < 0){
                    printf("Fatal Error: file \"%s\" does not exist\n", gg);
                    free(gg);
                    continue;
                }
                free(gg);
                //read into buffer struct to then be parsed
                struct bufferNode * temp;
                int totalReadBytes;
                int numLinks = 0;

                if(dirTail == NULL){
                    dirHead = (struct bufferNode *)malloc(sizeof(struct bufferNode));
                    dirTail = dirHead;
                    totalReadBytes = 0;
                    temp = dirTail;
                    numLinks = 1;
                }else if(dirTail->length != bufferSize){
                    temp = dirTail;
                    totalReadBytes = dirTail->length;
                }else{
                    //when dirTail->length == bufferSize
                    dirTail->next = (struct bufferNode *)malloc(sizeof(struct bufferNode));
                    temp =  dirTail->next;
                    totalReadBytes = 0;
                    numLinks = 1;
                }
                int bytes = 0;

                while(1){
                    bytes = read(fd, (temp->buffer + totalReadBytes), bufferSize - totalReadBytes);
                    if(bytes == -1){
                        printf("an error has occurred while reading the file...");
                        return;
                    }

                    totalReadBytes = totalReadBytes + bytes;
                    if(totalReadBytes == bufferSize){
                        //need to chain linked list stuff
                        temp->length = totalReadBytes;
                        bytes = 0;
                        totalReadBytes = 0;
                        struct bufferNode * ptr = (struct bufferNode *)malloc(sizeof(struct bufferNode));
                        temp->next = ptr;
                        temp = ptr;
                        numLinks++;
                    }else if(bytes == 0){
                        //reached EOF
                        temp->length = totalReadBytes;
                        dirTail = temp;
                        break;
                    }
                }
                HashTableSize = HashTableSize + 10*numLinks;
                close(fd);
                if(temp->length == 0){
                    HashTableSize = 0;
                    continue;
                }
            }

            strcpy(path2, startPath);
            strcat(path2, "/");
            strcat(path2, dirStuff->d_name);
            listFiles(path2);
        }
    }
    closedir(dir);
    dir = NULL;
    return;
}

int compareControl(char * ctrl, int ctrlLen, char * parse, int parLen){
    int max;
    int i;
    int numCommon = 0;

    if(ctrlLen > parLen){
        max = parLen;
    }else{
        max = ctrlLen;
    }
    for(i = 0; i < max; i++){
        if(ctrl[i] ==  parse[i]){
            numCommon++;
        }
    }
    return numCommon;
}

void printTree(struct hashnode * root, int * encoding, int index, int fd){
    // Assign 0 to left edge and recur
    if (root->left != NULL) {
        encoding[index] = 0;
        printTree(root->left, encoding, index + 1, fd);
    }
    // Assign 1 to right edge and recur
    if (root->right != NULL) {
        encoding[index] = 1;
        printTree(root->right, encoding, index + 1, fd);
    }
    // replace this with making a file and placing info inside (writing to it)
    if (root->right == NULL && root->left == NULL) {
        //insertToFile(root, encoding, index);
        char * altered = (char *)malloc((index + 1)*sizeof(char));
        int i;
        for (i = 0; i < index; i++){
            altered[i] = '0' + encoding[i];
        }
        altered[index] = '\0';
        int totalSize = index + 1 + strlen(root->word) + 1;

        char * writeBuffer = (char *)malloc(totalSize*sizeof(char));
        memset(writeBuffer, '\0', totalSize);
        strcpy(writeBuffer, altered);
        writeBuffer[index] = '\t';
        strcat(writeBuffer, root->word);
        writeBuffer[totalSize-1] = '\n';

        int bytes = 0;
        int totalBytes = 0;
        while(totalBytes != totalSize){
            bytes = write(fd, (writeBuffer+ totalBytes), (totalSize - totalBytes));
            if(bytes == -1){
                printf("issues with writing bytes\n");
                return;
            }
            totalBytes = totalBytes + bytes;
        }
        free(altered);
        free(writeBuffer);
    }
}

void insertToHash(char * word, int length){
    int key = hashCode(word);
    //if z then not dealing with escape code
    char escape = 'z';
    if(strcmp(word, " ") == 0){
        keyHolder[0] = key;
        escape = 's';
    }else if(strcmp(word, "\n") == 0){
        keyHolder[1] = key;
        escape = 'n';
    }else if(strcmp(word, "\t") == 0){
        keyHolder[2] = key;
        escape = 't';
    }else if(strcmp(word, "\v") == 0){
        keyHolder[3] = key;
        escape = 'v';
    }else if(strcmp(word, "\f") == 0){
        keyHolder[4] = key;
        escape = 'f';
    }else if(strcmp(word, "\r") == 0){
        keyHolder[5] = key;
        escape = 'r';
    }

    struct hashnode * check = hashtable[key];
    if(check == NULL){
        //create node insert
        check = (struct hashnode *)malloc(sizeof(struct hashnode ));
        if(escape != 'z'){
            check->word = (char *)malloc((controlLength + 1)*sizeof(char));
            int i;
            for(i = 0; i < (controlLength - 1); i++){
                check->word[i] = '!';
            }
            check->id = 's';
            check->word[controlLength - 1] = escape;
            check->word[controlLength] = '\0';
            check->length = (controlLength + 1);
        }else{
            check->id = 'z';
            check->word = (char *)malloc(length*sizeof(char));
            strcpy(check->word, word);
            check->length = length;
        }
        check->frequency = 1;
        check->next = NULL;
        check->left = NULL;
        check->right = NULL;
        hashtable[key] = check;
        uniqueWords++;
    }else{
        //loop to find and incriment or insert node
        while(check!= NULL){
            if(escape != 'z'){
                char * remove = (char *)malloc((controlLength + 1)*sizeof(char));
                int i = 0;
                for (i = 0; i < (controlLength -1); i++){
                    remove[i] = '!';
                }
                remove[controlLength -1] = escape;
                remove[controlLength] = '\0';
                if(strcmp(remove, check->word) == 0){
                    check->frequency++;
                    free(remove);
                    break;
                }
            }else{
                if(strcmp(check->word, word) == 0){
                    check->frequency++;
                    break;
                }
            }
            if(check->next == NULL){
                //means failed to find preexisting one, make one
                check->next = (struct hashnode *)malloc(sizeof(struct hashnode ));
                if(escape != 'z'){
                    check->next->word = (char *)malloc((controlLength + 1)*sizeof(char));
                    int i;
                    for(i = 0; i < (controlLength - 1); i++){
                        check->next->word[i] = '!';
                    }
                    check->next->id = 's';
                    check->next->word[controlLength - 1] = escape;
                    check->next->word[controlLength] = '\0';
                    check->next->length = (controlLength + 1);
                    //printf("%s\n", check->next->word);
                }else{
                    check->next->word = (char *)malloc(length*sizeof(char));
                    strcpy(check->next->word, word);
                    check->next->id = 'z';
                    check->next->length = length;
                    //printf("%s\n", check->next->word);
                }
                check->next->frequency = 1;
                check->next->next = NULL;
                check->next->left = NULL;
                check->next->right = NULL;
                uniqueWords++;
                break;
            }
            check = check->next;
        }
    }
}

void insertToHeap(struct hashnode * hashNode) {
    currentHeapSize++;
    minHeap[currentHeapSize] = hashNode;
    int now = currentHeapSize;

    while (minHeap[now / 2]->frequency > hashNode->frequency) {
        minHeap[now] = minHeap[now / 2];
        now /= 2;
    }
    minHeap[now] = hashNode;
}
//impliment this heap delete tomorrow :DDD
struct hashnode * removeFromHeap(){
    struct  hashnode * minElement;
    struct  hashnode * lastElement;
    int  child;
    int now;
    minElement = minHeap[1];
    lastElement = minHeap[currentHeapSize--];
    for(now = 1; now * 2 <= currentHeapSize; now = child){
        child = now * 2;

        if(child != currentHeapSize && minHeap[child + 1]->frequency < minHeap[child]->frequency){
            child++;
        }
        if(lastElement->frequency > minHeap[child]->frequency){
            minHeap[now] = minHeap[child];
        }else{
            break;
        }
    }
    minHeap[now] = lastElement;
    return minElement;
}

int hashCode(char * str){
    int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;

    if(hash < 0){
        hash = hash * -1;
    }
    //printf("hash num %d\n", hash%HashTableSize);
    return hash%HashTableSize;
}

int compressFile(char * fileName, char * huffmanCodeBook){
  int fd = open(huffmanCodeBook, O_RDONLY);
   // printf("%s\n", fileName);
    if(fd < 0){
        //or it doesnt exist??
        printf("Fatal Error: file \"%s\" does not exit\n", huffmanCodeBook);
        close(fd);
        return -1;
    }
    //read into buffer struct...
    int numLinks = 1;
    fileHead = (struct bufferNode *)malloc(sizeof(struct bufferNode));
    struct bufferNode * temp = fileHead;
    temp->next = NULL;
    int bytes = 0;
    int totalReadBytes = 0;
    while(1){
        bytes = read(fd, (temp->buffer + totalReadBytes), bufferSize - totalReadBytes);
        if(bytes == -1){
            printf("an error has occurred while reading the file...");
            return -1;
        }
        totalReadBytes = totalReadBytes + bytes;
        if(totalReadBytes == bufferSize){
            //need to chain linked list stuff
            temp->length = totalReadBytes;
            bytes = 0;
            totalReadBytes = 0;
            struct bufferNode * ptr = (struct bufferNode *)malloc(sizeof(struct bufferNode));
            temp->next = ptr;
            temp = ptr;
            temp->next = NULL;
            numLinks++;
        }else if(bytes == 0){
            //reached EOF
            temp->length = totalReadBytes;
            break;
        }
    }
    HashTableSize = 10*numLinks;
     if(temp->length == 0){
        printf("huffmancodebook is empty!\n");
        free(fileHead);
        return -1;
    }

    cHashTable = (struct decompNode **)malloc(HashTableSize*sizeof(struct decompNode *));
    int x = 0;
    for(x = 0; x < HashTableSize; x++){
        cHashTable[x] = NULL;
    }
    close(fd);
    return 0;
}

//place read in data into hashtable
void insertToHashCompress(char * word, int wordLen, char * code, int codeLen){
int key = hashCode(word);
struct decompNode * check  = cHashTable[key];
    if(check == NULL){
        //create node insert
        check = (struct decompNode *)malloc(sizeof(struct decompNode));
        check->word = (char *)malloc((wordLen)*sizeof(char));
        check->hashCode= (char *)malloc((codeLen)*sizeof(char));
        strcpy(check->word, word);
        strcpy(check->hashCode, code);
        check->next = NULL;
        cHashTable[key] = check;
    }else{
        //loop to find and incriment or insert node
        while(check!= NULL){
            if(check->next == NULL){
                //means failed to find preexisting one, make one
                check->next = (struct decompNode *)malloc(sizeof(struct decompNode));
                check->next->word = (char *)malloc((wordLen)*sizeof(char));
                check->next->hashCode = (char *)malloc((codeLen)*sizeof(char));
                strcpy(check->next->word, word);
                strcpy(check->next->hashCode, code);
                check->next->next = NULL;
                break;
            }
            check = check->next;
        }
    }
}
void cParseForHash(char flagger){
    // include flag for d or c
    //break up the huffman code to be inserted into table
    int wordSize = 0; // including terminator, it is like length, 1 longer than index
    int i = 0;
    int sizeLoop = 10;
    escapeCodeHold = (char *)malloc(sizeLoop*sizeof(char));
    memset(escapeCodeHold, '\0', sizeLoop);
    while(fileHead != NULL){
        if(i ==fileHead->length && i != sizeLoop){
            struct bufferNode * curr = fileHead->next;
            free(fileHead);
            fileHead = curr;
            i=0;
        }
        if(i == sizeLoop){
            sizeLoop = sizeLoop * 2;
            char * newOne = (char *)malloc(sizeLoop*sizeof(char));
            memset(newOne, '\0', sizeLoop);
            strcpy(newOne, escapeCodeHold);
            free(escapeCodeHold);
            escapeCodeHold = newOne;
        }
        if(fileHead->buffer[i] != '\n'){
            escapeCodeHold[wordSize] = fileHead->buffer[i];
            wordSize++;
            i++;
        }else{
            //it is \n
            escapeCodeHold[wordSize] =  '\0';
            wordSize++;
            i++;
            break;
        }
    }
    // 0 to store code 1 to store word
    int flagToStore = 0;
    int size = bufferSize;
    int size2 = bufferSize;
    wordSize = 0;
    int codeSize = 0;
    char * wordHolder = (char *)malloc(size*sizeof(char));
    char * codeHolder = (char *)malloc(size*sizeof(char));

    while(fileHead != NULL){
     //   printf("%s\n", wordHolder);
        if(i == fileHead->length && i != bufferSize){
                //break out reached EOF
                struct bufferNode * curr = fileHead->next;
                free(fileHead);
                fileHead = curr;
                if(fileHead == NULL){
                    break;
                }
        }else if(i == bufferSize){
                //reset buffer
                if(fileHead->next == NULL){
                    //happens to end exactly on
                    struct bufferNode * curr = fileHead->next;
                    free(fileHead);
                    fileHead = curr;
                }else{
                // shouLD ONLY RESIZE WHEN OUT OF SPACE FOR BUFFER
                    struct bufferNode * curr = fileHead->next;
                    free(fileHead);
                    fileHead = curr;
                    i = 0;
                }
        }
        if(wordSize == size){
            size = size + bufferSize;
            char * x = (char *)malloc(size*sizeof(char));
            strcpy(x, (const char *)wordHolder);
            free(wordHolder);
            wordHolder = x;
        }
        if(codeSize == size2){

            size2 = size2 + bufferSize;
            char * x = (char *)malloc(size*sizeof(char));
            strcpy(x, (const char *)codeHolder);
            free(codeHolder);
            codeHolder = x;
        }
        if (fileHead->buffer[i] == '\n'){
            if(wordSize == 0 || codeSize == 0){
                break;
            }
            //insert to hashtable
            wordHolder[wordSize] = '\0';
            if(flagger == 'c'){
                 insertToHashCompress(wordHolder, wordSize + 1,codeHolder, codeSize + 1);
            }else if(flagger == 'd'){
                //decompress flips what is hash Coded
                 insertToHashCompress(codeHolder, codeSize + 1, wordHolder, wordSize + 1);
            }
            codeSize = 0;
            wordSize = 0;
            i++;
            flagToStore = 0;
        }else if (fileHead->buffer[i] == '\t'){
            codeHolder[codeSize] = '\0';
            i++;
            flagToStore = 1;
        }else{
            //is not /n or /t then store depending on flag
            if(flagToStore == 0){
                codeHolder[codeSize] = fileHead->buffer[i];
                codeSize++;
            }else{
                wordHolder[wordSize] = fileHead->buffer[i];
                wordSize++;
            }
            i++;
        }
    }
    free(codeHolder);
    free(wordHolder);
}

void cMakeFile(char * fileName){
    //open textfile
    //check to see if the file is already an hcz file if yes then return
    int fileLen = strlen(fileName);
    if(fileLen > 4){
         if(fileName[fileLen - 4] == '.' && fileName[fileLen - 3] == 'h' && fileName[fileLen - 2] == 'c' && fileName[fileLen - 1] == 'z'){
            //already compressed
            printf("file: %s, has already been compressed\n", fileName);
            return;
        }
    }
    int fd2 = open(fileName, O_RDONLY);
    if(fd2 < 0){
        //or it doesnt exist??
        printf("Fatal Error: file \"%s\" does not exit\n", fileName);
        return;
    }
    int extLen = strlen(".hcz");
    char * newAlloc = (char *)malloc((fileLen + extLen + 1)*sizeof(char));
    memset(newAlloc, '\0', (fileLen + extLen + 1));
    strcpy(newAlloc, fileName);
    fileName = newAlloc;
    strcat(fileName, ".hcz");
    //maybe free fileName at the end?
    int huff=open(fileName, O_CREAT | O_RDWR, 00600);
    struct bufferNode * head2 = (struct bufferNode *)malloc(sizeof(struct bufferNode));
    head2->next=NULL;
    struct bufferNode * temp2 = head2;
    int bytes2 = 0;
    int totalReadBytes2 = 0;
    while(1){
        bytes2 = read(fd2, (temp2->buffer + totalReadBytes2), bufferSize - totalReadBytes2);
        if(bytes2 == -1){
            printf("an error has occurred while reading the file...\n");
            close(fd2);
            close(huff);
            return;
        }
        totalReadBytes2 = totalReadBytes2 + bytes2;
        if(totalReadBytes2 == bufferSize){
            //need to chain linked list stuff
            temp2->length = totalReadBytes2;
            bytes2 = 0;
            totalReadBytes2 = 0;
            struct bufferNode * ptr = (struct bufferNode *)malloc(sizeof(struct bufferNode));
            temp2->next = ptr;
            temp2 = ptr;
            temp2->next=NULL;
        }else if(bytes2 == 0){
            //reached EOF
            temp2->length = totalReadBytes2;
            break;
        }
    }
    close(fd2);
    free(fileName);
    //finished parsing into bufferNodes
    int tempLen = strlen(escapeCodeHold) + 2;
    char * newTemp = (char *)malloc((tempLen)*sizeof(char));
    strcpy(newTemp, escapeCodeHold);
    free(escapeCodeHold);
    escapeCodeHold = newTemp;
    escapeCodeHold[tempLen - 1] = '\0';
    //the space at tempLen -2 is a place holder for escapecode;

    int size2 = bufferSize;
    int wordSize2 = 0; // including terminator, it is like length, 1 longer than index
    //wordHolder2 is the thing that holds the parsed word
    char * wordHolder2 = (char *)malloc(size2*sizeof(char));
    int j = 0;
    char * huffCode=NULL;
    while(head2 != NULL){
        if(j == head2->length && j != bufferSize){
                //tail case
                if(wordSize2 > 0){
                    wordHolder2[wordSize2] = '\0';
                    huffCode = searchReturn(wordHolder2, 'c');
                    if(huffCode == NULL){
                        return;
                    }
                    int bytes = 0;
                    int totalBytes = 0;
                    while(totalBytes != strlen(huffCode)){
                        bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                        if(bytes == -1){
                            printf("issues with writing bytes\n");
                            return;
                        }
                        totalBytes = totalBytes + bytes;
                    }
                }
                struct bufferNode * curr = head2->next;
                free(head2);
                head2 = curr;
        }else if(j == bufferSize){
                //reset buffer
                if(head2->next == NULL){
                    //happens to end exactly on
                    char * oneBigger = (char *)malloc((wordSize2 + 1)*sizeof(char));
                    strcpy(oneBigger, wordHolder2);
                    free(wordHolder2);
                    wordHolder2 = oneBigger;
                    wordHolder2[wordSize2] = '\0';
                    huffCode = searchReturn(wordHolder2, 'c');
                    if(huffCode == NULL){
                        return;
                    }
					int bytes = 0;
                    int totalBytes = 0;
                    while(totalBytes != strlen(huffCode)){
                        bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                        if(bytes == -1){
                            printf("issues with writing bytes\n");
                            return;
                        }
                        totalBytes = totalBytes + bytes;
                    }
                    struct bufferNode * curr = head2->next;
                    free(head2);
                    head2 = curr;
                }else{
                // shouLD ONLY RESIZE WHEN OUT OF SPACE FOR BUFFER
                    struct bufferNode * curr = head2->next;
                    free(head2);
                    head2 = curr;
                    j = 0;
                }
        }else if(wordSize2 == size2){
                    size2 = size2 + bufferSize;
                    char * x = (char *)malloc(size2*sizeof(char));
                    strcpy(x, (const char *)wordHolder2);
                    free(wordHolder2);
                    wordHolder2 = x;
        }
        else if(head2->buffer[j] == ' '){
            if(wordSize2 > 0){
                wordHolder2[wordSize2] = '\0';
                huffCode = searchReturn(wordHolder2, 'c');
                if(huffCode == NULL){
                    return;
                }
                int bytes = 0;
                int totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
                escapeCodeHold[tempLen - 2] = 's';
                huffCode = searchReturn(escapeCodeHold, 'c');
                if(huffCode == NULL){
                    return;
                }
                bytes = 0;
                totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
            }else{
                escapeCodeHold[tempLen - 2] = 's';
                huffCode = searchReturn(escapeCodeHold, 'c');
                if(huffCode == NULL){
                    return;
                }
                int bytes = 0;
                int totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
            }
            wordSize2 = 0;
            j++;
        }else if (head2->buffer[j] == '\n'){
            if(wordSize2 > 0){
                wordHolder2[wordSize2] = '\0';
                huffCode = searchReturn(wordHolder2, 'c');
                if(huffCode == NULL){
                    return;
                }
                int bytes = 0;
                int totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }

                escapeCodeHold[tempLen - 2] = 'n';
                huffCode = searchReturn(escapeCodeHold, 'c');
                if(huffCode == NULL){
                    return;
                }
                bytes = 0;
                totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
            }else{
                escapeCodeHold[tempLen - 2] = 'n';
                huffCode = searchReturn(escapeCodeHold, 'c');
                if(huffCode == NULL){
                    return;
                }
                int bytes = 0;
                int totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
            }
            wordSize2 = 0;
            j++;
        }else if (head2->buffer[j] == '\t'){
            if(wordSize2 > 0){

                wordHolder2[wordSize2] = '\0';
                huffCode = searchReturn(wordHolder2, 'c');
                if(huffCode == NULL){
                    return;
                }
                int bytes = 0;
                int totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
                escapeCodeHold[tempLen - 2] = 't';
                huffCode = searchReturn(escapeCodeHold, 'c');
                if(huffCode == NULL){
                    return;
                }
                bytes = 0;
                totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
            }else{
                escapeCodeHold[tempLen - 2] = 't';
                huffCode = searchReturn(escapeCodeHold, 'c');
                if(huffCode == NULL){
                    return;
                }
                int bytes = 0;
                int totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
            }
            wordSize2 = 0;
            j++;
        }else if (head2->buffer[j] == '\v'){
           if(wordSize2 > 0){

                wordHolder2[wordSize2] = '\0';
                huffCode = searchReturn(wordHolder2, 'c');
                if(huffCode == NULL){
                    return;
                }
                int bytes = 0;
                int totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
                escapeCodeHold[tempLen - 2] = 'v';
                huffCode = searchReturn(escapeCodeHold, 'c');
                if(huffCode == NULL){
                    return;
                }
                bytes = 0;
                totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
            }else{
                escapeCodeHold[tempLen - 2] = 'v';
                huffCode = searchReturn(escapeCodeHold, 'c');
                if(huffCode == NULL){
                    return;
                }
                int bytes = 0;
                int totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
            }
            wordSize2 = 0;
            j++;
        }else if (head2->buffer[j] == '\f'){
           if(wordSize2 > 0){

                wordHolder2[wordSize2] = '\0';
                huffCode = searchReturn(wordHolder2, 'c');
                if(huffCode == NULL){
                    return;
                }
                int bytes = 0;
                int totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
                escapeCodeHold[tempLen - 2] = 'f';
                huffCode = searchReturn(escapeCodeHold, 'c');
                if(huffCode == NULL){
                    return;
                }
                bytes = 0;
                totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
            }else{
                escapeCodeHold[tempLen - 2] = 'f';
                huffCode = searchReturn(escapeCodeHold, 'c');
                if(huffCode == NULL){
                    return;
                }
                int bytes = 0;
                int totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
            }
            wordSize2 = 0;
            j++;
        }else if (head2->buffer[j] == '\r'){
            if(wordSize2 > 0){

                wordHolder2[wordSize2] = '\0';
                huffCode = searchReturn(wordHolder2, 'c');
                if(huffCode == NULL){
                    return;
                }
                int bytes = 0;
                int totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
                escapeCodeHold[tempLen - 2] = 'r';
                huffCode = searchReturn(escapeCodeHold, 'c');
                if(huffCode == NULL){
                    return;
                }
                bytes = 0;
                totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
            }else{
                escapeCodeHold[tempLen - 2] = 'r';
                huffCode = searchReturn(escapeCodeHold, 'c');
                if(huffCode == NULL){
                    return;
                }
                int bytes = 0;
                int totalBytes = 0;
                while(totalBytes != strlen(huffCode)){
                    bytes = write(huff, huffCode + totalBytes, strlen(huffCode) - totalBytes);
                    if(bytes == -1){
                        printf("issues with writing bytes\n");
                        return;
                    }
                    totalBytes = totalBytes + bytes;
                }
            }
            wordSize2 = 0;
            j++;
        }else{
            //is not a white space character
            wordHolder2[wordSize2] = head2->buffer[j];
            j++;
            wordSize2++;
        }
    }
    escapeCodeHold[tempLen - 2] = '\0';
    free(wordHolder2);
}

char * searchReturn(char * word, char flagger){
    //flip these around
    int key = hashCode(word);
    struct decompNode * check = cHashTable[key];
    if(check == NULL){
        return NULL;
    }

    while(check!= NULL){
        if(strcmp(check->word, word) == 0){
            //check for escape code crap only if numerical
            if(check->word[0] == '0' ||check->word[0] == '1' ){
                int tempLen = strlen(escapeCodeHold) + 2;
                char * newTemp = (char *)malloc((tempLen)*sizeof(char));
                strcpy(newTemp, escapeCodeHold);
                free(escapeCodeHold);
                escapeCodeHold = newTemp;
                char escape = check->hashCode[strlen(check->hashCode) - 1];
                escapeCodeHold[tempLen - 2] = escape;
                escapeCodeHold[tempLen - 1] = '\0';
                if(strcmp(escapeCodeHold, check->hashCode) == 0){
                    escapeCodeHold[tempLen - 2] = '\0';
                    if(escape == 's'){
                        return " ";
                    }else if(escape == 'n'){
                        return "\n";
                    }else if(escape == 'f'){
                        return "\f";
                    }else if (escape == 'v'){
                        return "\v";
                    }else if(escape == 'r'){
                        return "\r";
                    }else if(escape == 't'){
                        return "\t";
                    }else{
                        return "";
                    }
                }else{
                    escapeCodeHold[tempLen - 2] = '\0';
                    return check->hashCode;
                }
            }else{
                return check->hashCode;
            }
        }
        check = check->next;
    }
    return NULL;
}

void dirCompressFiles(char * startPath, char flagger){
    //create char * path
    //global
    char path2[1000];
    memset(path2, '\0', 1000);
    DIR * dir = opendir(startPath);
    if(dir == NULL){
        return;
    }
    struct dirent * dirStuff;
    terminate = 1;

    while((dirStuff = readdir(dir)) != NULL){
        if(strcmp(dirStuff->d_name, ".") != 0 && strcmp(dirStuff->d_name, "..") != 0 ){
            if(dirStuff->d_type == 8){
                //insert file to make shit for
                char * gg = malloc(1000*sizeof(char));
                strcpy(gg, startPath);
                strcat(gg, "/");
               // printf("has been opened and stored; ");
                //printf("%s\n", dirStuff->d_name);
                char * fileName = dirStuff->d_name;
                if(strcmp(fileName, "HuffmanCodeBook") == 0){
                    continue;
                }
                strcat(gg, fileName);
                //clean up these paths i think.. makes -1 for some reason
                if(flagger == 'c'){
                      cMakeFile(gg);
                }else if (flagger == 'd'){
                    //make the find 10101 function for decomrpess
                    reconstructFiles(gg);
                }
                free(gg);
            }//here
            strcpy(path2, startPath);
            strcat(path2, "/");
            strcat(path2, dirStuff->d_name);
            dirCompressFiles(path2, flagger);
        }
    }
    closedir(dir);
    dir = NULL;
    return;
}

void reconstructFiles(char * fileName){
    int fileLen = strlen(fileName);
    if(fileLen > 4){
        if(fileName[fileLen - 4] != '.' || fileName[fileLen - 3] != 'h' || fileName[fileLen - 2] != 'c' || fileName[fileLen - 1] != 'z'){
            //already decompressed
            printf("file: %s, has already been decompressed\n", fileName);
            return;
        }
    }else{
        printf("file: %s, has already been decompressed\n", fileName);
        return;
    }
    int fd2 = open(fileName, O_RDONLY);
    if(fd2 < 0){
        //or it doesnt exist??
        printf("Fatal Error: file \"%s\" does not exit\n", fileName);
        return;
    }
    char * newAlloc = (char *)malloc((fileLen)*sizeof(char));
    memset(newAlloc, '\0', (fileLen));
    strcpy(newAlloc, fileName);
    fileName = newAlloc;
    fileName[fileLen-4] = '\0';
    //should clear the hcz
    // revise for later

    int decompText = open(fileName, O_CREAT | O_RDWR, 00600);
    struct bufferNode * head2 = (struct bufferNode *)malloc(sizeof(struct bufferNode));
    head2->next=NULL;
    struct bufferNode * temp2 = head2;
    int bytes2 = 0;
    int totalReadBytes2 = 0;
    while(1){
        bytes2 = read(fd2, (temp2->buffer + totalReadBytes2), bufferSize - totalReadBytes2);
        if(bytes2 == -1){
            printf("an error has occurred while reading the file...");
            return;
        }
        totalReadBytes2 = totalReadBytes2 + bytes2;
        if(totalReadBytes2 == bufferSize){
            //need to chain linked list stuff
            temp2->length = totalReadBytes2;
            bytes2 = 0;
            totalReadBytes2 = 0;
            struct bufferNode * ptr = (struct bufferNode *)malloc(sizeof(struct bufferNode));
            temp2->next = ptr;
            temp2 = ptr;
            temp2->next=NULL;
        }else if(bytes2 == 0){
            //reached EOF
            temp2->length = totalReadBytes2;
            break;
        }
    }
    close(fd2);

    //done reading in file into nodes
    //preparing to parse  file
    int size2 = bufferSize;
    int wordSize2 = 0; // including terminator, it is like length, 1 longer than index
    char * wordHolder2 = (char *)malloc(size2*sizeof(char));
    int j = 0;
    char * word=NULL;
    while(head2 != NULL){
        if(j == head2->length && j != bufferSize){
                //tail case
                if(wordSize2 > 0){
                    wordHolder2[wordSize2] = '\0';
                    word = searchReturn(wordHolder2, 'd');
                    if(word != NULL){
                        //nor sure if this would ever be true
                        int bytes = 0;
                        int totalBytes = 0;
                        while(totalBytes != strlen(word)){

                            bytes = write(decompText, word + totalBytes, strlen(word) - totalBytes);
                            if(bytes == -1){
                                printf("issues with writing bytes\n");
                                return;
                            }
                            totalBytes = totalBytes + bytes;
                        }
                    }
                }
                struct bufferNode * curr = head2->next;
                free(head2);
                head2 = curr;
        }else if(j == bufferSize){
                //reset buffer
                if(head2->next == NULL){
                    //happens to end exactly on
                    char * oneBigger = (char *)malloc((wordSize2 + 1)*sizeof(char));
                    strcpy(oneBigger, wordHolder2);
                    free(wordHolder2);
                    wordHolder2 = oneBigger;
                    wordHolder2[wordSize2] = '\0';
                    word = searchReturn(wordHolder2, 'd');
                    if(word != NULL){
                        int bytes = 0;
                        int totalBytes = 0;
                        while(totalBytes != strlen(word)){

                            bytes = write(decompText, word + totalBytes, strlen(word) - totalBytes);
                            if(bytes == -1){
                                printf("issues with writing bytes\n");
                                return;
                            }
                            totalBytes = totalBytes + bytes;
                        }
					}
                    struct bufferNode * curr = head2->next;
                    free(head2);
                    head2 = curr;
                }else{
                // shouLD ONLY RESIZE WHEN OUT OF SPACE FOR BUFFER
                    struct bufferNode * curr = head2->next;
                    free(head2);
                    head2 = curr;
                    j = 0;
                }
        }else if(wordSize2 == size2){

                    size2 = size2 + bufferSize;
                    char * x = (char *)malloc(size2*sizeof(char));
                    strcpy(x, (const char *)wordHolder2);
                    free(wordHolder2);
                    wordHolder2 = x;

        }else if(head2->buffer[j]=='1'||head2->buffer[j]=='0'){
                if(wordSize2>0){
                   word = searchReturn(wordHolder2, 'd');
                    if(word != NULL){
                        int bytes = 0;
                        int totalBytes = 0;
                        while(totalBytes != strlen(word)){

                            bytes = write(decompText, word + totalBytes, strlen(word) - totalBytes);
                            if(bytes == -1){
                                printf("issues with writing bytes\n");
                                return;
                            }
                            totalBytes = totalBytes + bytes;
                        }
                        //memset(wordHolder2, 0, strlen(wordHolder2));
                        wordSize2=0;
					}
                }
                wordHolder2[wordSize2] = head2->buffer[j];
                //insert null terminator to seach correctly
                if(wordSize2 == size2){
                    size2 = size2 + bufferSize;
                    char * x = (char *)malloc(size2*sizeof(char));
                    strcpy(x, (const char *)wordHolder2);
                    free(wordHolder2);
                    wordHolder2 = x;

                    wordHolder2[wordSize2 + 1] = '\0';
                }else{
                    wordHolder2[wordSize2 + 1] = '\0';
                }
                wordSize2++;
                j++;
        }else if(head2->buffer[j]!='1'||head2->buffer[j]!='0'){
            j++;
        }else{
            //is not a white space character
            wordHolder2[wordSize2] = head2->buffer[j];
            j++;
            wordSize2++;
        }
    }
    free(wordHolder2);
    free(fileName);
    return;
}
