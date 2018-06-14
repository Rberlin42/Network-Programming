#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

//returns 0 if its invalid, 1 if it is
//updates str to be all caps and removes surrounding whitespace
int isValid(char * string){
    //trim leading
    int i = 0;
    int j;
    while(i < strlen(string) && isspace(string[i]))
        i++;
    for(j = i; j < strlen(string); j++)
        string[j-i] = string[j];
    string[j-i] = '\0';

    //trim trailing
    i = strlen(string)-1;
    while(i >= 0 && isspace(string[i]))
        i--;
    string[i+1] = '\0';

    //check if its empty
    if(strlen(string) <= 0)
        return 0;

    //capitalize
    for(i = 0; i < strlen(string); i++)
        string[i] = toupper(string[i]);

    return 1;
}

struct mystruct {
    int foo;
    char s[10];
};

void doStuff(struct mystruct * s){
    s->foo = 0;
    strcpy(s->s, "yo");
}

int main(int argc, char * argv[]) {

    struct mystruct fuck[2];
    fuck[0].foo = 5;
    strcpy(fuck[0].s, "nigger");
    doStuff(&(fuck[0]));
    printf("%d\n", fuck[0].foo);
    printf("%s\n", fuck[0].s);

    return EXIT_SUCCESS;
}