#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include "../include/parser.h"
#include "../include/lexer.h"
#include "../include/hop.h"
#include "../include/reveal.h"
#include "../include/peek.h"
#include "../include/locate.h"
#include "../include/executor.h"

int main(){
    char home_dir[1024], host_name[1024];
    getcwd(home_dir, sizeof(home_dir));
    gethostname(host_name, sizeof(host_name));
    struct passwd *profile = getpwuid(getuid());
    char *username = profile->pw_name;
    char prev_dir[1024] = "";
    while(1){
        char curr_dir[1024];
        getcwd(curr_dir, sizeof(curr_dir));
        if(strncmp(home_dir, curr_dir, strlen(home_dir)) == 0){
            printf("<%s@%s:~%s>", username, host_name, curr_dir+strlen(home_dir));
        }else{
            printf("<%s@%s:%s>", username, host_name, curr_dir);
        }
        fflush(stdout);

        char input[1024];
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n");
            break;
        }
        int idx = strcspn(input, "\n");
        input[idx] = '\0';

        token tokens[1000];
        int count = lexer(input, tokens);

        if(count == -1) {
            continue;
        }
        if(count == 0) {
            continue;
        }
        if(parse(tokens, count) == -1){
            printf("cshell: invalid syntax\n");
            continue;
        }

        execute_pipeline(tokens, count, home_dir, prev_dir);
    }
    return 0;
} 