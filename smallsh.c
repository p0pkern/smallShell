#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sys/resource.h>


// Initiate the shell
void startShell();

// Sanitize and gather user input
int verifyUserInput(char *userInput);
char* getUserInput();
struct command;
struct command *createCommand(char userCommand[]);
struct command *createCommandList(char userInput[]);
struct status;

// Free and clean up user inputs
void cleanMemoryAndReturnToShell();
void freeList(struct command *list);

// Functions for program
void activateCommands(struct command *list);
void changeDirectory(struct command *list);
int verifyBackgroundProcessRequest(struct command *list);
int verifyIfChildRedirect(struct command *list);
void redirectProcess(struct command *list, int type);
char* expandVariablesToPid(char variable[]);
void runProcess(struct command *list, int type);
int getLengthOfPID(pid_t pid);
void toggleForegroundMode();
void handleSignals(int signo);
void addPidToBackgroundList(pid_t pid);

// Status
void getStatus();
void checkPid();

// MAIN PROGRAM
int main() {
    startShell();
    return 0;
};

// STATUS TRACKING
// Keep track of exit status of the last foreground
// process, background pid's and their exit status, and toggles foregroundOnlyMode
// backgrounds are limited to 200 which is the number of processes os1 is allowed.
struct status{
    int lastStatus;
    int foregroundOnlyMode;
    pid_t backgrounds[200];
};

static struct status currStatus = {0, 0};

// Print the status of the last foreground process to exit.
void getStatus(){
    printf("exit value %d\n", currStatus.lastStatus);
}

// LINKED LIST FOR STORING VARIABLES
/* I chose a linked list as my data structure for the following reasons
 * 1. I don't have to prepare for the 512 argument at 2048 bytes limit as
 *     each node will be allocated at runtime.
 * 2. Since each command is calloc to each node, I am not worried about white
 *    space or other issues.
 * 3. Certain commands (such as expanding $$) are simple since I just need to 
 *    create the new command string and reallocate the memory to the new string.
 * 4. The only issue is that every search is O(N) and I do have to repeatedly 
 *    search to prepare my commands. I think this is an okay tradeoff for the
 *    benefits it allows me.
 */

// Linked list node. Holds one command and a reference to the next
// command.
struct command{
    char *command;
    struct command *next;
};

// Creating the linked list node
/* Citation for the following function: createCommand()
   Date: 01/28/2022
   Adapted from: Example program from Module 1 to create a linked list
   Source URL: https://replit.com/@cs344/studentsc#main.c
*/
struct command *createCommand(char userCommand[]) {
    /* This creates the linked list node for each command
       before applying the command to the node in the linked
       list, it will first verify of any expansion variables
       and process those first.
    */

    struct command *currCommand = malloc(sizeof(struct command));
    
    // Check for expansion variables $$
    char *formattedCommand;
    formattedCommand = expandVariablesToPid(userCommand);

    // Apply the formatted string to the node.
    currCommand->command = calloc(strlen(formattedCommand)+1, sizeof(char));
    strcpy(currCommand->command, formattedCommand);
    currCommand->command[strlen(currCommand->command)] = '\0';

    currCommand->next = NULL;

    return currCommand;

};

// Creating the linked list.
/* Citation for the following function: createCommandList()
   Date: 01/28/2022
   Adapted from: Example program from Module 1 to create a linked list
   Source URL: https://replit.com/@cs344/studentsc#main.c
*/
struct command *createCommandList(char userInput[]) {
    /* This is a linked list structure for the nodes */

    // List head
    struct command *head = NULL;
    // List tail
    struct command *tail = NULL;

    char *savePtr;
    char *token = strtok_r(userInput, " ", &savePtr);

    while(token != NULL) {
        struct command *newCommand = createCommand(token);

        if(head == NULL) {
            head = newCommand;
            tail = newCommand;
        } else {
            tail->next = newCommand;
            tail = newCommand;
        }

        token = strtok_r(NULL, " ", &savePtr);
    }

    free(userInput);
    fflush(stdin);
    fflush(stdout);

    return head;
};

// CLEANING MEMORY AND PREPARING FOR THE NEXT COMMAND
/* Cleaning a linked list is not as simple as freeing the head.
 * in order to ensure that my memory links for a command list are
 * taken care of. I need to go through each node and free the command string
 * before freeing the actual node.
 */
/* Citation for the following function: freeList()
   Date: 01/28/2022
   Adapted from: How to fix memory links of a linked list
   Source URL: https://stackoverflow.com/questions/6417158/c-how-to-free-nodes-in-the-linked-list/6417182
*/
void freeList(struct command *list) {
    /* 
     * This will iterate through the linked list
     * and free all the memory allocation.
     * It works by visiting each node and freeing the allocation
     * of the individual commands, and then releasing the linked list
     * node.
     */
    struct command *temp;

    while(list != NULL){
        temp = list;
        list = list->next;
        free(temp->command);
        free(temp);
    }
}

// This is responsible for flushing the buffers
// and returning to the starting shell.
// This process was repeated a lot so it made sense
// to isolate it to its own function.
void cleanMemoryAndReturnToShell(struct command *list) {
    freeList(list);
    fflush(stdin);
    fflush(stdout);
    startShell();
}


// SHELL START AND VERIFICATION
void startShell() {
    /* Starts the user shell and requests input. All
       user processes start at this function. */

    /* In order to ensure my signal interrupts are
     * handled correctly. I need to invoke them at
     * startShell, runProcess, and redirectProcess.
     * I spent some time exploring global variables but
     * for me it just didn't work correctly. With some
     * future refactoring I think I can discover a way
     * to handle these correctly.
     */
    struct sigaction SIGINT_action = {{0}};

    sigfillset(&SIGINT_action.sa_mask);

    SIGINT_action.sa_flags = SA_RESTART;

    SIGINT_action.sa_handler = SIG_IGN;
    sigaction(SIGINT, &SIGINT_action, NULL);

    SIGINT_action.sa_handler = toggleForegroundMode;
    sigaction(SIGTSTP, &SIGINT_action, NULL);

    fflush(stdin);
    fflush(stdout);

    checkPid(); // Used to track background pid's exit status.

    /* Start the interactive shell */
    char* userInput;
    int verified;

    userInput = getUserInput();
    // This verifies whether a user inputs a comment or a blank space.
    verified = verifyUserInput(userInput);

    if(verified == -1){
        free(userInput);
        fflush(stdin);
        fflush(stdout);
        startShell();
    } else {
       struct command *list = createCommandList(userInput);
       activateCommands(list);
    }
}

/* Gathers the user input as a solid string for parsing */
char* getUserInput(){
    /* 
     * Get the line of user commands 
     * returns the entire line as a
     * string
     */

    fflush(stdin);
    fflush(stdout);

    char *input = NULL;
    size_t len = 0;

    printf(":");
    int read = getline(&input, &len, stdin);
    input[read-1] = '\0';

    return input;
}

/* Verifies whether the initial value is a #
 * denoting a comment, or a \0 which verifies
 * a blank line
 */
int verifyUserInput(char userInput[]) {
    /* 
     * Verify the the user input is either 
     * a blank line or a comment
     */

    if(userInput[0] == '\0' || userInput[0] == '#') return -1;
    else return 1;
}

/* Activate commands handles both the built in processes and routes
 * the executable processes.
 */
void activateCommands(struct command *list) {
    /* Examines the first command of the linked list
       if the first command is one of the built in
       commands, it will perform the built in action
       other commands will be sent to be parsed by 
       the execution functions */

    if(strcmp(list->command, "exit") == 0) {
        freeList(list);
        fflush(stdout);
        exit(0);
    } else if (strcmp(list->command, "cd") == 0){
        changeDirectory(list);
        cleanMemoryAndReturnToShell(list);
    } else if(strcmp(list->command, "status") == 0){
        getStatus();
        cleanMemoryAndReturnToShell(list);
    } else {
        // Verify first if there is a & at the end of the list
        int checkBackground = verifyBackgroundProcessRequest(list);
        // Verify if any < or > are present to signify a redirect
        int checkRedirect = verifyIfChildRedirect(list);

        // Follows a global variable
        if(currStatus.foregroundOnlyMode == 1) {
            checkBackground = 0;
        }

        if(checkRedirect == 0){
            runProcess(list, checkBackground);
        } else if(checkRedirect == 1) {
            redirectProcess(list, checkBackground);
        }
        cleanMemoryAndReturnToShell(list);
    }
}

// VERIFICATION BEFORE FORKING
/* Using the strength of my linked list. I know that if I 
 * look at the last node (where currList->next == NULL) and
 * it is a &, then this is supposed to be a background process.
 */
int verifyBackgroundProcessRequest(struct command *list) {
    /* Verifies whether a process has been
     * signaled as a foreground or background process
     */
    struct command *currList = list;

    while(currList->next != NULL) currList = currList->next;
    if(strcmp(currList->command, "&") == 0) return 1;
    return 0;
}

/* Similar to the background process. I search to find any instances
 * of < or > which triggers a redirect.
 */
int verifyIfChildRedirect(struct command *list) {
    /* Verfies if there is a redirect command > or <.
     * it will also verify that immediately after the 
     * redirect command, that there is a filename to 
     * redirect to.
     */
    
    struct command *currList = list;

    while(currList != NULL) {
        if(strcmp(currList->command, "<") == 0 || strcmp(currList->command, ">") == 0) {
            if(currList->next != NULL) {
                return 1;
            }
        }
        currList = currList->next;
    }
    return 0;
}

// USER COMMANDS AFTER VERIFICATION

/* Citation for the following function: changeDirectory()
   Date: 01/28/2022
   Adapted from: clarification on how to find a path of unknown value
   Source URL: https://stackoverflow.com/questions/298510/how-to-get-the-current-directory-in-a-c-program
*/
void changeDirectory(struct command *list) {
    /* Depending on the context of the command
     * this will change the directory.
     * cd -> will go to $HOME
     * cd /[directory] -> will search and go to the directory
     * cd [directory] -> will sanitize the entry and go to the directory
     * if the directory doesn't exist an error will print.
     */
    char *envPath;
    int changeDir;

    // Path into the $HOME directory
    if(list->next == NULL){
        envPath = getenv("HOME");
        changeDir = chdir(envPath);
        if(changeDir == -1){
            perror("chdir() failure: \n");
        } 
    } else {
        list = list->next;
        // Path into an absolute path startin with /
        if(list->command[0] == '/') {
            changeDir = chdir(list->command);
            if(changeDir == -1){
                perror("chdir() failure: \n");
            }
        } else {
            // Path into a relative directory.
            envPath = calloc(strlen(list->command) + 3, sizeof(char));
            sprintf(envPath, "./%s", list->command);
            changeDir = chdir(envPath);
            if(changeDir == -1){
                perror("chdir() failure: \n");
            }
            free(envPath);
        }   
    }
}

/* I needed the length of the PID to know how much memory to allocate
 * this was a helpful function to accomplish this.
 */
/* Citation for the following function: getLengthOfPID()
   Date: 01/29/2022
   Adapted from: How to get the length of an integer
   Source URL: https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c
   */
int getLengthOfPID(pid_t pid){
    return snprintf(NULL, 0, "%d", pid);
}

/* This will search the command for any instances of $
 * if there are two in a row, then it will replace that
 * with the pid. Otherwise it will return the command.
 * Due to some issues with parsing the userCommand with 
 * a for loop to the sizeof the command. It would bleed into
 * the memory of the next command. I ended up choosing
 * to parse through using a pointer which fixed the issue.
 */
/* Citation for the following function: expandVariablesToPid()
   Date: 01/29/2022
   Adapted from: How to turn an integer into a string
   Source URL: https://stackoverflow.com/questions/8257714/how-to-convert-an-int-to-string-in-c 
*/
// Expand all instances of $$ to the pid of the parent
char* expandVariablesToPid(char *userCommand) {
    /* Any instance of $$ in a variable is transformed into
     * the pid of the parent
     */
    char *formattedChar;
    pid_t parentPID = getpid();
    int lengthOfParentPID = getLengthOfPID(parentPID);
    char *ptr;
    int count = 0;

    // Counts the number of $ in the command
    ptr = userCommand;

    while(*ptr != '\0') {
        if(*ptr == '$') count++;
        ptr++;
    }

    // If we have at least 2 $ then perform the variable expansion.
    ptr = userCommand;
    
    if(count >= 2) {
        int flag = 0; // This flag is used to determine if 2 $ are next to one another.

        // Turn the pid into a string for concatenation
        char *tempPID = calloc(lengthOfParentPID + 1, sizeof(char)); 
        sprintf(tempPID, "%d", parentPID);
        
        char *tempString = calloc(strlen(userCommand) + ((count * lengthOfParentPID)) + 1, sizeof(char));

        while(*ptr != '\0'){
            // We have found a char '$' in the string
            if (*ptr == '$') {
                // If the previous char was '$' concat the pid to the string.
                if(flag == 1) {
                    sprintf(tempString, "%s%s", tempString, tempPID);
                    flag = 0;
                // Otherwise we increment our flag and check the next char
                } else {
                    flag = 1;
                }
            // If the next char is not a $
            } else {
                // Verify if the previous char was a $, if so, we concat a $ and add the next char
                if(flag == 1) {
                    sprintf(tempString, "%s%c", tempString, '$');
                    flag = 0;
                }
                    sprintf(tempString, "%s%c", tempString, *ptr);
            }
           ptr++;
        }
        // This is here to catch a trailing $ which could be discarded if a pid was
        // put in before it appeared. It's a small but annoying edge case.
        if(flag == 1) {
            sprintf(tempString, "%s%c", tempString, '$');
        }
        // Free the allocated memory of command and replace it with the new string
        formattedChar = calloc(strlen(tempString) + 3,sizeof(char));
        sprintf(formattedChar, "%s", tempString);
        free(tempPID);
        free(tempString);
        return formattedChar;
    }

    return userCommand;
}

// FILE REDIRECTION FUNCTIONS
/* Citation for the following function: redirectProcess()
   Date: 01/30/2022
   Adapted from: How to perform file redirection in modules
   Source URL: https://canvas.oregonstate.edu/courses/1884946/pages/exploration-processes-and-i-slash-o?module_item_id=21835982
   */
/* Citation for the following function: redirectProcess()
   Date: 01/30/2022
   Adapted from: how to check status on child failure/completion
   Source URL: https://stackoverflow.com/questions/13735501/fork-exec-waitpid-issue
*/
void redirectProcess(struct command *list, int type) {
    /* This is for process redirect of an input file or output file or both.
     * It works by first pulling the command from the first node in the linked
     * list, then inspecting the other nodes for an instance of the redirect command
     * and the file following it. Once it is processed, depending on the flags
     * (inputFlag will trigger if there is an input file and outputFlag will trigger)
     * if there is an output file. The program will fork and perform the redirection 
     * with dup2().
     */

    char *input;
    char *output;
    char *commands[513];
    int i = 0;
    int inputFlag = 0;
    int outputFlag = 0;
    int hasInput = 0;
    int hasOutput = 0;

    /* The signal commands needed to be re-done due to them being local variables*/
    struct command *currList = list;

    struct sigaction SIGINT_action = {{0}};

    sigfillset(&SIGINT_action.sa_mask);

    SIGINT_action.sa_flags = SA_RESTART;

    // Handles toggling of foreground only mode.
    SIGINT_action.sa_handler = toggleForegroundMode;
    sigaction(SIGTSTP, &SIGINT_action, NULL);

    // I use the same command for both background and foreground redirects
    // So I pass a flag variable 1 or 0 that denotes whether the command
    // is a foreground 0 or background 1 process.
    if(type == 0) {
        // Allows foreground process to be terminated with ctrl-c
        SIGINT_action.sa_handler = handleSignals;
        sigaction(SIGINT, &SIGINT_action, NULL);
    } else {
        // ignores ctrl-c
        SIGINT_action.sa_handler = SIG_IGN;
        sigaction(SIGINT, &SIGINT_action, NULL);
    }

    // First I harvest the commands. I ignore any < or > characters as well as the
    // background & (we already know it's a background command by the flag).
    // These commands are added to a list for input into the execute variable
    while(currList != NULL) {
        if (strcmp(currList->command, "<") == 0 || strcmp(currList->command, ">") == 0) {
            currList = currList->next;
        } else {
            if(currList->next != NULL) {
                commands[i] = calloc(strlen(list->command) + 1, sizeof(char));
                sprintf(commands[i], "%s", list->command);
                i++;
            } else {
                if(strcmp(currList->command, "&") != 0) {
                    commands[i] = calloc(strlen(list->command) + 1, sizeof(char));
                    sprintf(commands[i], "%s", list->command);
                    i++;
                }
            }
        }
        currList = currList->next;
    }
    // This is for an edge case where the first time I input a command, it fails because
    // it thinks there is a file for a blank space.
    commands[i] = NULL;

    // This will search the list for any instance of <. It will trigger the flags to let
    // the program know to start a input file redirect. It will also put the file handle 
    // into a variable if it exists. Otherwise when the program is run it will reference
    // /dev/null
    currList = list;
    // Assign the input file for processing to input and trigger the flag that input was found
    while(currList != NULL) {
        if(strcmp(currList->command, "<") == 0) {
            hasInput = 1;
            if(currList->next != NULL && strcmp(currList->command, ">") != 0) {
                currList = currList->next;
                inputFlag = 1;
                input = calloc(strlen(currList->command) + 1, sizeof(char));
                sprintf(input, "%s", currList->command);
                break;
            }
        }
        currList = currList->next;
    }

    // This will search the list for any instance of >. It will trigger the flags to let
    // the program know to start a output file redirect. It will also put the file handle 
    // into a variable if it exists. Otherwise when the program is run it will reference
    // /dev/null
    currList = list;
    // Assign the output file for processing to output and trigger the flag that output was found
    while(currList != NULL) {
        if(strcmp(currList->command, ">") == 0) {
            hasOutput = 1;
            if(currList->next != NULL && strcmp(currList->command, "<") != 0) {
                currList = currList->next;
                outputFlag = 1;
                output = calloc(strlen(currList->command) + 1, sizeof(char));
                sprintf(output, "%s", currList->command);
                break;
            }
       }
       currList = currList->next;
    }

    int childStatus;
    pid_t childID = fork();
    switch(childID){
        case -1:
            currStatus.lastStatus = -1;
            perror("fork() error: \n");
            break;
        case 0:
            // If we have an input file and output file with both redirect flags
            if(inputFlag == 1 && outputFlag == 1) {

                // Open source file
                int sourceFD = open(input, O_RDONLY);
                if(sourceFD == - 1) {
                    perror("source open() \n");
                    exit(1);
                }

                // redirect stdin to source file
                int result = dup2(sourceFD, 0);
                if(result == -1) {
                    perror("source dup2()");
                    exit(1);
                }

                // open target file
                int targetFD = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(targetFD == -1) {
                    perror("target open()");
                    exit(1);
                }

                // Redirect standard out to target file
                result = dup2(targetFD, 1);
                if(result == -1){
                    perror("target dup2()");
                    exit(1);
                }

                // Executes command on the target files.
                // Set file controls to close files when execution
                // is finished
                fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
                fcntl(targetFD, F_SETFD, FD_CLOEXEC);
                execvp(commands[0], commands);
                perror("execv");
                exit(1);
                break;
            // If we only have an input flag and input source
            } else if(inputFlag == 1 && outputFlag == 0 && hasOutput == 0) {
                
                // Open source file
                int sourceFD = open(input, O_RDONLY);
                if(sourceFD == -1) {
                    perror("source open(): ");
                    exit(1);
                }
                // Change stdout to command line
                int result = dup2(sourceFD, 0);
                if(result == -1){
                    perror("dup2 error on source: ");
                    exit(1);
                }
                // Close file on finish of execution
                fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
                execvp(commands[0], commands);
                perror("execv");
                exit(1);
                break;

            // Has an output flag and file, but no input flag or file
            } else if(inputFlag == 0 && outputFlag == 1 && hasInput == 0) {
                
                // open target file
                int targetFD = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(targetFD == -1) {
                    perror("target open()");
                    exit(1);
                }

                // Redirect output into target file
                int result = dup2(targetFD, 1);
                if(result == -1){
                    perror("target dup2()");
                    exit(1);
                }

                // Close file on completion of execution
                fcntl(targetFD, F_SETFD, FD_CLOEXEC);
                execvp(commands[0], commands);
                perror("execv");
                exit(1);
                break;

            // If there is a redirection input and output flag but no files. Everything is
            // routed to /dev/null
            } else if(inputFlag == 0 && outputFlag == 0 && hasInput == 1 && hasOutput == 1) {
                
                // Open /dev/null as source
                int sourceFD = open("/dev/null", O_RDONLY);
                if(sourceFD == -1) {
                    perror("source open(): ");
                    exit(1);
                }
                // Redirect stdin to /dev/null
                int result = dup2(sourceFD, 0);
                if(result == -1){
                    perror("dup2 error on source: ");
                    exit(1);
                }
                
                // open destination as /dev/null
                int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(targetFD == -1) {
                    perror("target open()");
                    exit(1);
                }

                // Redirect output to /dev/null
                result = dup2(targetFD, 1);
                if(result == -1){
                    perror("target dup2()");
                    exit(1);
                }   

                //Close upon execution
                fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
                fcntl(targetFD, F_SETFD, FD_CLOEXEC);
                execvp(commands[0], commands);
                perror("execv");
                exit(1);
                break;

            // Source flag only, no input file
            } else if(inputFlag == 0 && outputFlag == 0 && hasInput == 1 && hasOutput == 0) {
                
                // open /dev/null as source
                int sourceFD = open("/dev/null", O_RDONLY);
                if(sourceFD == -1) {
                    perror("source open(): ");
                    exit(1);
                }
                // Redirec standard input to /dev/null
                int result = dup2(sourceFD, 0);
                if(result == -1){
                    perror("dup2 error on source: ");
                    exit(1);
                }
                // Close files upon execution
                fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
                execvp(commands[0], commands);
                perror("execv");
                exit(1);
                break;
            // Output flag only, and no target file
            } else if(inputFlag == 0 && outputFlag == 0 && hasInput == 0 && hasOutput == 1) {

                // open target file as /dev/null
                int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(targetFD == -1) {
                    perror("target open()");
                    exit(1);
                }

                // Redirect stdout to /dev/null
                int result = dup2(targetFD, 1);
                if(result == -1){
                    perror("target dup2()");
                    exit(1);
                }
                // Close upon execution
                fcntl(targetFD, F_SETFD, FD_CLOEXEC);
                execvp(commands[0], commands);
                perror("execv");
                exit(1);
                break;
            }

        default:
            // Restore signal handler to SIG_IGN upon child exit.
            SIGINT_action.sa_handler = SIG_IGN;
            sigaction(SIGINT, &SIGINT_action, NULL);
            fflush(stdin);
            fflush(stdout);
            // Signals if a child process has been terminated to command line.
            // Otherwise performs the child process as a foreground process.
            if(type == 0) {
                childID = waitpid(childID, &childStatus, 0);
                if (WIFSIGNALED(childStatus)) {
                    printf("pid %d terminated: signal %d\n", childID, childStatus);
                }
                currStatus.lastStatus = WEXITSTATUS(childStatus) == 0 ? 0 : 1;
                cleanMemoryAndReturnToShell(list);
            fflush(stdout);
            fflush(stdin);
            // Adds child pid the background pid tracking list
            } else if(type == 1) {
                char *backgroundMessage = calloc(47, sizeof(char));
                sprintf(backgroundMessage, "Starting Background Process for id: %d\n", childID);
                write(STDOUT_FILENO, backgroundMessage, 47);
                fflush(stdout);
                free(backgroundMessage);
                addPidToBackgroundList(childID);
                cleanMemoryAndReturnToShell(list);
                break;
            } 
        }

    }

/* adds the background pids to the list at the first available opening 
 * the list is populated by deafault by 0's*/
void addPidToBackgroundList(pid_t pid) {
    /* Adds the background PID's the a list for verifying
       when a background process has exited */
    
    for (int i = 0; i < sizeof(currStatus.backgrounds)/sizeof(currStatus.backgrounds[0]); i++){
        if(currStatus.backgrounds[i] == 0) {
            currStatus.backgrounds[i] = pid;
            break;
        } 
    }
}

/* This will iterate through the list of background pid's and verify its completion
 * status. If it exited normally or was terminated by a signal input.
 */
void checkPid() {
    /* Iterates through the pid list to see if a background process
      has completed. This will be ran when startShell is run so it 
      will not activate until a new command is entered. */

    for (int i = 0; i < sizeof(currStatus.backgrounds)/sizeof(currStatus.backgrounds[0]); i++) {
        if(currStatus.backgrounds[i] > 1) {
            int childStatus;
            pid_t pid = currStatus.backgrounds[i];
            while((waitpid(pid, &childStatus, WNOHANG) > 0)){
                if(WIFEXITED(pid)) {
                    printf("background pid %d is done: exit value %d\n", pid, childStatus);
                } else if (WIFSIGNALED(pid)) {
                    printf("background pid %d terminated: signal %d\n", pid, childStatus);
                } 
            }
        }
    }
}

/* Signal function to toggle foreground only mode */
void toggleForegroundMode(int signo) {
    if(currStatus.foregroundOnlyMode == 0) {
        char *message = "Foreground mode activated.\n";
        write(STDOUT_FILENO, message, 28);
        currStatus.foregroundOnlyMode = 1;
    } else {
        write(STDOUT_FILENO, "Foreground mode disabled.\n", 27);
        currStatus.foregroundOnlyMode = 0;
    }
}

/* handles the SIGINT interrupt for the foreground process */
void handleSignals(int signo) {
    _exit(0);
}

/* This executes normal processes with no redirect flags 
 * similarly it uses a flag to run both foreground and background
 * processes.
 */
void runProcess(struct command *list, int type){

    /* Reset the signal interrupts */
    struct sigaction SIGINT_action = {{0}};

    sigfillset(&SIGINT_action.sa_mask);

    SIGINT_action.sa_flags = SA_RESTART;

    // Toggle forergound only mode
    SIGINT_action.sa_handler = toggleForegroundMode;
    sigaction(SIGTSTP, &SIGINT_action, NULL);

    if(type == 0) {
        // Foreground process can be interrupted.
        SIGINT_action.sa_handler = handleSignals;
        sigaction(SIGINT, &SIGINT_action, NULL);
    } else {
        // All other processes ignore SIGINT
        SIGINT_action.sa_handler = SIG_IGN;
        sigaction(SIGINT, &SIGINT_action, NULL);
    }
    
    // Parses the linked list for commands and 
    // puts them into an array for input into execvp
    // since the background process is run by a flag
    // it ignores that command.
    struct command *currList = list;

    char *newargv[513];
    int i = 0;

    while(currList != NULL) {
        if(strcmp(currList->command, "&") != 0) {
            newargv[i] = currList->command;
            i++;
        } 
        currList = currList->next;
    }
    // Stops edge cases of the first command not being formatted right.
    newargv[i] = NULL;

    int childStatus;
    pid_t childID = fork();
    switch(childID){
        case -1:
            currStatus.lastStatus = -1;
            perror("fork() error: \n");
            break;
        case 0:
            execvp(newargv[0], newargv);
            perror("execv error: \n");
            exit(1);
            break;
        default:
            // Restores ignore upon return to parent process
            SIGINT_action.sa_handler = SIG_IGN;
            sigaction(SIGINT, &SIGINT_action, NULL);
            fflush(stdin);
            fflush(stdout);
            // If the child process was terminated by a signal interrupt
            // it will print the signal to the screen.
            // Otherwise it will hold the parent process until completed.
            if(type == 0) {
                childID = waitpid(childID, &childStatus, 0);
                if (WIFSIGNALED(childStatus)) {
                    printf("pid %d terminated: signal %d\n", childID, childStatus);
                }
                currStatus.lastStatus = WEXITSTATUS(childStatus) == 0 ? 0 : 1;
                cleanMemoryAndReturnToShell(list);
            fflush(stdout);
            fflush(stdin);
            } else if(type == 1) {
                char *backgroundMessage = calloc(47, sizeof(char));
                sprintf(backgroundMessage, "Starting Background Process for id: %d\n", childID);
                write(STDOUT_FILENO, backgroundMessage, 47);
                fflush(stdout);
                free(backgroundMessage);
                // Adds the pid to the backgroujnd pid list for procesing.
                addPidToBackgroundList(childID);
                break;
            }       
        }
    cleanMemoryAndReturnToShell(list);
    
}

