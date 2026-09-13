#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <signal.h>
#include <errno.h>
#include "../include/parser.h"
#include "../include/lexer.h"
#include "../include/hop.h"
#include "../include/reveal.h"
#include "../include/peek.h"
#include "../include/locate.h"
#include "../include/executor.h"
#include "../include/jobs.h"

int main(){
    char home_dir[1024], host_name[1024];
    getcwd(home_dir, sizeof(home_dir));
    gethostname(host_name, sizeof(host_name));
    struct passwd *profile = getpwuid(getuid());
    char *username = profile->pw_name;
    char prev_dir[1024] = "";
    
    // ignore SIGTTOU initially so setpgid / tcsetpgrp works
    signal(SIGTTOU, SIG_IGN);
    
    // Put shell in its own process group
    pid_t shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);
    
    init_all_signals();
    
    int last_was_eof = 0;
    while(1){
        check_bg_jobs();
        char curr_dir[1024];
        getcwd(curr_dir, sizeof(curr_dir));
        if(strncmp(home_dir, curr_dir, strlen(home_dir)) == 0){
            printf("<%s@%s:~%s>", username, host_name, curr_dir+strlen(home_dir));
        }else{
            printf("<%s@%s:%s>", username, host_name, curr_dir);
        }
        fflush(stdout);

        char input[1024];
        errno = 0;
        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (was_sigint() || (!feof(stdin) && errno == EINTR)) {
                clearerr(stdin);
                printf("\n");
                continue;
            }
            if (has_stopped_jobs()) {
                if (!last_was_eof) {
                    printf("\ncshell: there are stopped jobs\n");
                    last_was_eof = 1;
                    clearerr(stdin);
                    continue;
                }
            }
            send_sighup_all();
            printf("\n");
            break;
        }
        
        last_was_eof = 0;
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