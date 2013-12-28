#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pwd.h>
#include <limits.h>

#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

#define EXEC_LS     "/bin/ls"
#define EXEC_CAT    "/bin/cat"

static char *default_env[]={"PATH=/bin;/usr/bin;", "IFD= \t\n", 0};

int main(int argc, char **argv) {
    clearenv();
    if (argc < 2) {
        printf("sume ls [-sm] [file ...]\nsume cat [-benstuv] [file ...]\n");
        exit(1);
    }

    char dest[PATH_MAX];
    if (readlink("/proc/self/exe", dest, PATH_MAX) == -1) {
        printf("Error retrieving executable path\n");
        exit(errno);
    }

    if (strlen(dest) + 4 > PATH_MAX) {
        printf("Error path for config longer then maximum allowed path length\n");
        exit(1);
    }
    char *path = malloc(sizeof(char) * (strlen(dest) + 4));
    if (path == NULL) {
        printf("Error mallocing space. Please try again.\n");
        exit(1);
    }
    memset(path, 0, strlen(path));
    dest[(strlen(dest) - 4)] = '\0';
    sprintf(path, "%s.sumeCfg", dest);

    /* Periodic clears of the environment */
    clearenv();
    int fdTwo;
    struct stat fstatInfo;

    /* Statically declaring permissions */
    static uid_t ruid, euid;
    ruid = getuid();
    euid = geteuid();

    /* Increase permissions to read configuration file */
    if (setuid(euid) == -1) {
        printf("Failed to modify process permissions\n");
        free(path);
        exit(errno);
    }
    /* Open the config file, fstat, and then prepare file for parsing. Then reduce permissions. */
    if ((fdTwo = open(path, O_RDONLY | O_NOFOLLOW)) == -1) {
        printf("Configuration could not be opened\n");
        free(path);
        exit(errno);
    }
    if (fstat(fdTwo, &fstatInfo) == -1) {
        printf("Configuration could not be read\n");
        free(path);
        close(fdTwo);
        exit(errno);
    }
    FILE *file = fdopen(fdTwo, "r");
    if (file == NULL) {
        printf("Configuration could not be opened\n");
        free(path);
        close(fdTwo);
        exit(errno);
    }
    /* Reduce permissions */
    if (setuid(ruid) == -1) {
        printf("Failed to modify process permissions\n");
        free(path);
        fclose(file);
        exit(errno);
    }
    /* Check file for userid */
    struct passwd *user = getpwuid(ruid);
    if (user == NULL) {
        printf("Failed to get username\n");
        free(path);
        fclose(file);
        exit(errno);
    }
    /* Find home directory for path */
    path[(strlen(path) - 9)] = '\0';
    if (fstatInfo.st_uid != euid) {
        printf("Incorrect owner of sume configuration.\n");
        free(path);
        fclose(file);
        exit(1);
    }
    if (fstatInfo.st_mode & S_IWGRP || fstatInfo.st_mode & S_IWOTH || !(fstatInfo.st_mode & S_IWUSR)) {
        printf("Sume configuration has been modified\n");
        free(path);
        fclose(file);
        exit(2);
    }

    int l = 0, foundUser = 0;
    size_t nread;
    char bufUser[50];
    while ((nread = fread(&bufUser[l], 1, sizeof(char), file)) >= 0) {
        if (strncmp(" ", &bufUser[l], 1) == 0 || strncmp("\n", &bufUser[l], 1) == 0 || strncmp("\t", &bufUser[l], 1) == 0 || nread == 0) {
            if (l > 0) {
                bufUser[l] = '\0';
                if (strncmp(bufUser, user->pw_name, (l - 1)) == 0) {
                    foundUser = 1;
                    break;
                }
                l = 0;
            }
        } else { l++; }
        if (nread <= 0) { break; }
    }
    /* After parsing file close it */
    fclose(file);
    if (!foundUser) {
        printf("User not found in sume configuration.\n");
        free(path);
        exit(1);
    }

    /* HARDCODED WHITELIST FOR LS AND CAT */
    if (strncmp(argv[1], "ls", 2) == 0 && strnlen(argv[1], 3) == 2) {
        if (argc > 5) {
            /* Checking for too many ls arguments */
            printf("Unsupported number of arguments for ls\n");
            free(path);
            exit(1);
        }
        clearenv();

        /* PRIOR TO SUBMIT */
        int argPath = argc - 1;
        argc -= 1;
        char *tempArg = malloc(sizeof(char) * strlen(argv[argPath]) + 1);
        if (tempArg == NULL) {
            printf("Failed to get username\n");
            free(path);
            exit(1);
        }
        memset(tempArg, 0, strlen(tempArg));
        strncpy(tempArg, argv[argPath], strlen(argv[argPath]));
        tempArg[strlen(argv[argPath])] = '\0';

        /* Check file for userid */
        struct passwd *host_user = getpwuid(euid);
        if (host_user == NULL) {
            printf("Failed to get username\n");
            free(path);
            exit(errno);
        }
        int hExists = 0;
        char *pch = strtok(tempArg,".~/");
        while (pch != NULL) {
            if (strncmp(pch, host_user->pw_name, strlen(host_user->pw_name)) == 0) {
                hExists = 1;
            } else if (hExists) {
                char *temp = realloc(path, strlen(path) + strlen(pch) + 1);
                if (temp == NULL) {
                    free(path);
                    free(tempArg);
                    exit(1);
                }
                path = temp;
                sprintf(path, "%s/%s", path, pch);
            }
            pch = strtok (NULL, ".~/");
        }
        free(tempArg);
        if (!hExists) {
            printf("Invalid argument detected.");
            free(path);
            exit(1);
        }
        /* END PRIOR TO SUBMIT */
	
        /* Attempt to open directory prior to permission escalation */
        int fd;
        if ((fd = open(path, O_RDONLY | O_NOFOLLOW)) == -1) {
            /* Attempt to escalate permissions */
            if (setuid(euid) == -1) {
                printf("Failed to modify process permissions\n");
                free(path);
                exit(errno);
            }
            if ((fd = open(path, O_RDONLY | O_NOFOLLOW)) == -1) {
	    	if (errno == ENOENT) {
             	   printf("File doesn't exist in home directory\n");
             	   free(path);
            	   exit(errno);
           	}
                printf("Invalid file permissions\n");
                free(path);
                exit(errno);
            }
        }
        /* After fstat reduce permissions and then close the file */
        if (fstat(fd, &fstatInfo) == -1) {
            printf("File could not be read\n");
            free(path);
            close(fd);
            exit(errno);
        }
        if (setuid(ruid) == -1) {
            printf("Failed to modify process permissions\n");
            free(path);
            close(fd);
            exit(errno);
        }
        close(fd);
        /* Check for correct ownership */
        if (!(fstatInfo.st_mode & S_IRUSR || fstatInfo.st_mode & S_IXUSR)) {
            printf("Host does not have permission to ls file or directory\n");
            free(path);
            exit(1);
        }
        char **execPath;
        execPath = malloc(sizeof(char *) * (argc + 1));
        if (execPath == NULL) {
            printf("Failed to allocate enough memory. Please try again.\n");
            free(path);
            exit(1);
        }
        execPath[0] = malloc(sizeof(char) * 2);
        if (execPath[0] == NULL) {
            printf("Failed to allocate enough memory. Please try again.\n");
            free(path);
            free(execPath);
            exit(1);
        } else {
            memset(execPath[0], 0, strlen(execPath[0]));
            sprintf(execPath[0], "ls");
        }
        execPath[1] = malloc(sizeof(char) * (((argc == 2) ? (strlen(path) + 1) : 2)));
        if (execPath[1] == NULL) {
            printf("Failed to allocate enough memory. Please try again.\n");
            free(path);
            free(execPath[0]);
            exit(1);
        } else {
            memset(execPath[1], 0, strlen(execPath[1]));
            if (argc == 2) {
                sprintf(execPath[1], "%s", path);
                execPath[2] = malloc(sizeof(char));
                if (execPath[2] == NULL) {
                    printf("Failed to allocate enough memory. Please try again.\n");
                    free(path);
                    free(execPath[0]);
                    free(execPath[1]);
                    exit(1);
                }
                memset(execPath[2], 0, strlen(execPath[2]));
                execPath[2] = '\0';
            } else {
                if ((strncmp(argv[2], "-s", 2) == 0 && strnlen(argv[2], 3) == 2) || (strncmp(argv[2], "-m", 2) == 0 && strnlen(argv[2], 3) == 2)) {
                    sprintf(execPath[1], "%s", argv[2]);
                } else {
                    printf("Invalid argument recieved.\n");
                    free(path);
                    free(execPath[0]);
                    free(execPath[1]);
                    exit(1);
                }   
            }
        }
        if (argc > 2) {
            execPath[2] = malloc(sizeof(char) * (((argc == 3) ? (strlen(path) + 1) : 2)));
            if (execPath[2] == NULL) {
                printf("Failed to allocate enough memory. Please try again.\n");
                free(path);
                free(execPath[0]);
                free(execPath[1]);
                exit(1);
            } else {
                memset(execPath[2], 0, strlen(execPath[2]));
                if (argc == 3) {
                    sprintf(execPath[2], "%s", path);
                    execPath[3] = malloc(sizeof(char));
                    if (execPath[3] == NULL) {
                        printf("Failed to allocate enough memory. Please try again.\n");
                        free(path);
                        free(execPath[0]);
                        free(execPath[1]);
                        free(execPath[2]);
                        exit(1);
                    }
                    memset(execPath[3], 0, strlen(execPath[3]));
                    execPath[3] = '\0';
                } else {
                    if ((strncmp(argv[3], "-s", 2) == 0 && strnlen(argv[3], 3) == 2) || (strncmp(argv[3], "-m", 2) == 0 && strnlen(argv[3], 3) == 2)) {
                        sprintf(execPath[2], "%s", argv[3]);
                    } else {
                        printf("Invalid argument recieved.\n");
                        free(path);
                        free(execPath[0]);
                        free(execPath[1]);
                        free(execPath[2]);
                        exit(1);
                    }
                }
            }
        }
        if (argc == 4) {
            execPath[3] = malloc(sizeof(char) * (strlen(path) + 1));
            if (execPath[3] == NULL) {
                printf("Failed to allocate enough memory. Please try again.\n");
                free(path);
                free(execPath[0]);
                free(execPath[1]);
                free(execPath[2]);
                exit(1);
            }
            memset(execPath[3], 0, strlen(execPath[3]));
            sprintf(execPath[3], "%s", path);
            execPath[4] = malloc(sizeof(char));
            if (execPath[4] == NULL) {
                printf("Failed to allocate enough memory. Please try again.\n");
                free(path);
                free(execPath[0]);
                free(execPath[1]);
                free(execPath[2]);
                free(execPath[3]);
                exit(1);
            }
            memset(execPath[4], 0, strlen(execPath[4]));
            execPath[4] = '\0';
        }
        free(path);
        int status;
        if (fork() == 0) {
            if (setuid(euid) == -1) {
                printf("Failed to modify process permissions\n");
                int i;
                for (i = 0; i < argc + 1; i++) {
                    free(execPath[i]);
                }
                exit(errno);
            }
            if (execve(EXEC_LS, execPath, default_env) < 0) {
                printf("Failed to execute ls\n");
                int i;
                for (i = 0; i < argc + 1; i++) {
                    free(execPath[i]);
                }
                exit(errno);
            }
        }
        wait(&status);
        int i;
        for (i = 0; i < argc; i++) {
            free(execPath[i]);
        }
    } else if (strncmp(argv[1], "cat", 3) == 0 && strnlen(argv[1], 4) == 3) {
        char *tempPath = malloc(sizeof(char) * strlen(path));
        if (tempPath == NULL) {
            printf("Failed to allocate enough memory. Please try again.\n");
            free(path);
            exit(1);
        }
        memset(tempPath, 0, strlen(tempPath));
        strncpy(tempPath, path, strlen(path));

        char **execPath = malloc(sizeof(char *) * argc);
        if (execPath == NULL) {
            printf("Failed to allocate enough memory. Please try again.\n");
            free(path);
            free(tempPath);
            exit(1);
        }
        execPath[0] = malloc(sizeof(char) * 3);
        if (execPath[0] == NULL) {
            printf("Failed to allocate enough memory. Please try again.\n");
            free(path);
            free(tempPath);
            free(execPath);
            exit(1);
        } else {
            sprintf(execPath[0], "cat");
        }

        int i = 2, phaseOne = 1;
        while (i < argc) {
            if (strncmp(argv[i], "-", 1) == 0 && (strnlen(argv[i], 3) == 2 || strnlen(argv[i], 11) == 10 
                || strnlen(argv[i], 18) == 17 || strnlen(argv[i], 12) == 11 || strnlen(argv[i], 9) == 8 
                || strnlen(argv[i], 16) == 15 || strnlen(argv[i], 19) == 18 || strnlen(argv[i], 7) == 6 
                || strnlen(argv[i], 10) == 9)) {
                // arguments phase 1
                if (phaseOne) {
                    if (!(strncmp(argv[i], "-A", 2) == 0 || strncmp(argv[i], "--show-all", 10) == 0 
                        || strncmp(argv[i], "-b", 2) == 0 || strncmp(argv[i], "--number-nonblank", 17) == 0 
                        || strncmp(argv[i], "-e", 2) == 0 || strncmp(argv[i], "-E", 2) == 0 
                        || strncmp(argv[i], "--show-ends", 11) == 0 || strncmp(argv[i], "-n", 2) == 0
                        || strncmp(argv[i], "--number", 8) == 0 || strncmp(argv[i], "-s", 2) == 0
                        || strncmp(argv[i], "--squeeze-blank", 15) == 0 || strncmp(argv[i], "-t", 2) == 0 
                        || strncmp(argv[i], "-T", 2) == 0 || strncmp(argv[i], "--show-tabs", 11) == 0 
                        || strncmp(argv[i], "-u", 2) == 0 || strncmp(argv[i], "-v", 2) == 0 
                        || strncmp(argv[i], "--show-nonprinting", 18) == 0 || strncmp(argv[i], "--help", 6) == 0
                        || strncmp(argv[i], "--version", 9) == 0)) {
                        /* Incompatible cat arguments */
                        printf("sume ls [-sm] [file ...]\nsume cat [-benstuv] [file ...]\n");
                        free(path);
                        free(tempPath);
                        int x;
                        for (x = 0; x < i; x++) {
                            free(execPath[x]);
                        }
                        exit(1);
                    } else {
                        execPath[i - 1] = malloc(sizeof(char) * strlen(argv[i]));
                        if (execPath[i - 1] == NULL) {
                            printf("Failed to allocate enough memory. Please try again.\n");
                            int x;
                            free(path);
                            free(tempPath);
                            for (x = 0; x < i; x++) {
                                free(execPath[x]);
                            }
                            exit(1);
                        } else {
                            sprintf(execPath[i - 1], "%s", argv[i]);
                        }
                    }
                } else {
                    printf("sume ls [-sm] [file ...]\nsume cat [-benstuv] [file ...]\n");
                    int x;
                    free(path);
                    free(tempPath);
                    for (x = 0; x < i; x++) {
                        free(execPath[x]);
                    }
                    exit(1);
                }
            } else if (strncmp(argv[i], "-", 1) == 0 && strnlen(argv[i], 2) == 1) {
                // stdin phase 2
                phaseOne = 0;
                execPath[i - 1] = malloc(sizeof(char));
                if (execPath[i - 1] == NULL) {
                    int x;
                    printf("Failed to allocate enough memory. Please try again.\n");
                    free(path);
                    free(tempPath);
                    for (x = 0; x < i; x++) {
                        free(execPath[x]);
                    }
                    exit(1);
                } else {
                    sprintf(execPath[i - 1], "-");
                }
            } else if (strncmp(argv[i], "~", 1) == 0 || strncmp(argv[i], "/", 1) == 0 || strncmp(argv[i], ".", 1) == 0) {
                // other files phase 2
                phaseOne = 0;
                char *tempArg = malloc(sizeof(char) * strlen(argv[i]) + 1);
                if (tempArg == NULL) {
                    printf("Failed to get username\n");
                    free(path);
                    exit(1);
                }
                memset(tempArg, 0, strlen(tempArg));
                strncpy(tempArg, argv[i], strlen(argv[i]));
                tempArg[strlen(argv[i])] = '\0';

                /* Check file for userid */
                struct passwd *host_user = getpwuid(euid);
                if (host_user == NULL) {
                    printf("Failed to get username\n");
                    free(path);
                    exit(errno);
                }

                int hExists = 0;
                char *pch = strtok(tempArg,".~/");
                while (pch != NULL) {
                    if (strncmp(pch, host_user->pw_name, strlen(host_user->pw_name)) == 0) {
                        hExists = 1;
                    } else if (hExists) {
                        char *temp = realloc(path, strlen(path) + strlen(pch) + 1);
                        path = temp;
                        if (temp == NULL) {
                            free(path);
                            free(tempArg);
                            exit(1);
                        }
                        sprintf(path, "%s/%s", path, pch);
			//path[strlen(path)] = '\0';
                    }
                    pch = strtok (NULL, ".~/");
                }
                free(tempArg);
                if (!hExists) {
                    printf("Invalid argument detected.");
                    free(path);
                    exit(1);
                }

                int fd;
                if ((fd = open(path, O_RDONLY | O_NOFOLLOW)) == -1) {
                    // Attempt to escalate privleges
                    if (setuid(euid) == -1) {
                        printf("Failed to modify process permissions\n");
                        free(path);
                        int x;
                        free(tempPath);
                        for (x = 0; x < i; x++) {
                            //free(execPath[x]);
                        }
                        close(fd);
                        exit(errno);
                    }
                    if ((fd = open(path, O_RDONLY | O_NOFOLLOW)) == -1) {
		    	if (errno == ENOENT) {
                        	printf("File doesn't exist in home directory\n");
                        	int x;
                        	free(path);
                        	free(tempPath);
                        	for (x = 0; x < i; x++) {
                            		//free(execPath[x]);
                        	}
                        	exit(errno);
                    	}
                        printf("Invalid file permissions\n");
                        free(path);
                        free(tempPath);
                        int x;
                        for (x = 0; x < i; x++) {
                            //free(execPath[x]);
                        }
                        close(fd);
                        exit(errno);
                    }
                }
                
                if (fstat(fd, &fstatInfo) == -1) {
                    printf("File could not be read\n");
                    free(path);
                    free(tempPath);
                    int x;
                    for (x = 0; x < i; x++) {
                        free(execPath[x]);
                    }
                    close(fd);
                    exit(errno);
                }
                if (setuid(ruid) == -1) {
                    printf("Failed to modify process permissions\n");
                    free(path);
                    free(tempPath);
                    int x;
                    for (x = 0; x < i; x++) {
                        free(execPath[x]);
                    }
                    close(fd);
                    exit(errno);
                }
                close(fd);
                if (!(fstatInfo.st_mode & S_IRUSR || fstatInfo.st_mode & S_IXUSR)) {
                    printf("Host does not have permission to ls file or directory\n");
                    free(path);
                    free(tempPath);
                    int x;
                    for (x = 0; x < i; x++) {
                        free(execPath[x]);
                    }
                    exit(1);
                }
                execPath[i - 1] = malloc(sizeof(char) * strlen(path));
                if (execPath[i - 1] == NULL) {
                    int x;
                    printf("Failed to allocate enough memory. Please try again.\n");
                    free(path);
                    free(tempPath);
                    for (x = 0; x < i; x++) {
                        free(execPath[x]);
                    }
                    exit(1);
                } else {
                    sprintf(execPath[i - 1], "%s", path);
                    if (strlen(path) > strlen(tempPath)) {
                        path[strlen(tempPath)] = '\0';
                    }
                }
            } else {
                printf("Host does not have permission to cat file or directory\n");
                free(path);
                free(tempPath);
                int x;
                for (x = 0; x < argc; x++) {
                    free(execPath[x]);
                }
                exit(1);
            }
            i++;
        }

        execPath[argc - 1] = malloc(sizeof(char));
        if (execPath[argc - 1] == NULL) {
            printf("Failed to allocate enough memory. Please try again.\n");
            free(path);
            free(tempPath);
            for (i = 0; i < argc; i++) {
            free(execPath[i]);
        }
            exit(1);
        } else {
            execPath[argc - 1] = '\0';
        }

        free(path);
        free(tempPath);
        int status;
        if (fork() == 0) {
            if (setuid(euid) == -1) {
                printf("Failed to modify process permissions\n");
                for (i = 0; i < argc; i++) {
                    free(execPath[i]);
                }
                exit(errno);
            }
            if (execve(EXEC_CAT, execPath, default_env) == -1) {
                printf("Failed to execute cat\n");
                for (i = 0; i < argc; i++) {
                    free(execPath[i]);
                }
                exit(errno);
            }
        }
        wait(&status);
        for (i = 0; i < argc; i++) {
            //free(execPath[i]);
        }
    } else {
        printf("sume ls [-sm] [file ...]\nsume cat [-benstuv] [file ...]\n");
        free(path);
        exit(1);
    }
    return 0;
}
