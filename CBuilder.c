
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#define	cbDEFAULT_STRING_ALLOCATION_SIZE	500
#define	cbMAX_RULES 100

typedef uint8_t U8;

#if defined(_WIN32) || defined(_WIN64)
    #define cbPLATFORM_WINDOWS
    #include <windows.h>
#elif defined(__linux__) || defined(__APPLE__) && defined(__MACH__)
	#if defined(__linux__)
		#define cbPLATFORM_LINUX
	#elif defined(__APPLE__) && defined(__MACH__)
		#define cbPLATFORM_MACOS
	#endif

    #define cbPLATFORM_UNIX
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
#else
    #error "Unsupported platform"
#endif


typedef struct cbString
{
	char *c_str;              // Pointer to the character array
	unsigned int length;      // Current length of the string
	unsigned int size;        // Allocated size of the string buffer
} cbString;

typedef cbString cbCommand;

typedef void(*cbRuleFunction)(void);
typedef struct cbRule
{
	char			*name;
	cbRuleFunction	function;
} cbRule;

cbRule cbRules[cbMAX_RULES];
int	cbRuleCount;


void cbCreateRule(const char *ruleName, cbRuleFunction function)
{
	if (cbRuleCount >= cbMAX_RULES)
		return;
	cbRules[cbRuleCount].name = strdup(ruleName);
	cbRules[cbRuleCount].function = function;
	cbRuleCount++;
}

U8 cbExecuteRule(const char *rule)
{
	for (int i = 0; i < cbRuleCount; i++)
	{
		if (strcmp(cbRules[i].name, rule) == 0)
		{
			printf("Executing rule %s\n", cbRules[i].name);
			cbRules[i].function();
			return 1;
		}
	}
	return 0;
}


cbString *cbCreateString(void)
{
	cbString *string = (cbString *)malloc(sizeof(cbString)); // Allocate memory for cbString structure
	string->c_str = (char *)malloc(cbDEFAULT_STRING_ALLOCATION_SIZE * sizeof(char));
	string->length = 0;
	string->size = cbDEFAULT_STRING_ALLOCATION_SIZE;
	string->c_str[0] = '\0';

	return string;
}
#define cbCreateCommand cbCreateString


void cbAppendToString(cbString *str, const char *element)
{
	if (!str || !element) return;

	unsigned long elementLength = strlen(element);
	unsigned long requiredSize = str->length + elementLength + 1;

	if (requiredSize > str->size)
	{
		unsigned int newSize = str->size * 2;
		while (newSize < requiredSize)
		{
			newSize *= 2; 
		}
		char *newBuffer = (char *)realloc(str->c_str, newSize);
		str->c_str = newBuffer;
		str->size = newSize;
	}

	memcpy(str->c_str + str->length, element, elementLength);
	str->length += elementLength;
	str->c_str[str->length] = '\0';
}

void cbAppendToCommandNULL(cbCommand *command, ...)
{
    if (!command) return;

    va_list args;
    va_start(args, command);

    const char *arg = va_arg(args, const char *);
    while (arg != NULL)
    {
        if (command->length > 0)
        {
            cbAppendToString(command, " ");
        }

        cbAppendToString(command, arg);
        arg = va_arg(args, const char *);
    }
    va_end(args);
}
#define cbAppendToCommand(command, ...) cbAppendToCommandNULL(command, __VA_ARGS__, NULL)


int cbExecuteCommand(const cbCommand *command)
{
    if (!command || !command->c_str)
    {
        fprintf(stderr, "Invalid command: NULL pointer\n");
        return -1;
    }

	printf("[CMD] : %s\n", command->c_str);
#ifdef cbPLATFORM_WINDOWS
    // On Windows, use CreateProcess
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Create a mutable buffer for the command string
    char *cmdBuffer = strdup(command->c_str);
    if (!cmdBuffer)
    {
        fprintf(stderr, "Memory allocation failed for command buffer\n");
        return -1;
    }

    // Execute the command
    if (!CreateProcess(NULL, cmdBuffer, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        fprintf(stderr, "cbCommand execution failed: %lu\n", GetLastError());
        free(cmdBuffer);
        return -1;
    }

    // Wait for the process to complete
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Clean up
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    free(cmdBuffer);

#elif defined(cbPLATFORM_UNIX)
    // On Unix-like systems, use fork and exec
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("Fork failed");
        return -1;
    }

    if (pid == 0)
    {
        // In child process
        execl("/bin/sh", "sh", "-c", command->c_str, (char *)NULL);
        // If execl fails
        perror("execl failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        // In parent process
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }
#endif

    return 0;
}


void cbRebuildSelf(void)
{
    cbCommand *selfBuildCmd = cbCreateCommand();

#ifdef cbPLATFORM_WINDOWS
    cbAppendToCommand(selfBuildCmd, "gcc", "-o", "CBuilder.exe", "CBuilder.c");
#else
    cbAppendToCommand(selfBuildCmd, "gcc", "-o", "CBuilder", "CBuilder.c");
#endif
    printf("Rebuilding CBuilder: %s\n", selfBuildCmd->c_str);

    int result = cbExecuteCommand(selfBuildCmd);
    if (result != 0)
    {
        fprintf(stderr, "Failed to rebuild CBuilder with code: %d\n", result);
        exit(EXIT_FAILURE);
    }

    printf("CBuilder rebuilt successfully.\n");
    free(selfBuildCmd->c_str);
    free(selfBuildCmd);
}

void cbRunSelf(cbString *args)
{
	cbCommand *cmd = cbCreateCommand();
#ifdef cbPLATFORM_WINDOWS
	cbAppendToCommand(cmd, "CBuilder.exe");
#else
	cbAppendToCommand(cmd, "./CBuilder");
#endif
	cbAppendToCommand(cmd, args->c_str);
	printf("CMD %s\n", cmd->c_str);
	cbExecuteCommand(cmd);
}

void cbManageRebuild(int argc, char **argv)
{
	int rebuild = 1;
	cbString *args = cbCreateString();
	for (int i = 1; i < argc; i++)
	{
		cbAppendToCommand(args, argv[i]);
		if (strcmp(argv[i], "NOREBUILD") == 0)
			rebuild = 0;
	}
	if (rebuild)
	{
		cbRebuildSelf();
		cbAppendToCommand(args, "NOREBUILD");
		cbRunSelf(args);
		exit(0);
	}
}
void cbManageRules(int argc, char **argv)
{
	U8 retValue;
	cbManageRebuild(argc, argv);
	for (int i = 1; i < argc; i++)
	{
		retValue = cbExecuteRule(argv[i]);
	}
	if (!retValue && cbRuleCount)
		cbRules[0].function();
}


void cbAppendCompiler(cbCommand *cmd)
{
	#if defined(cbPLATFORM_WINDOWS)
		cbAppendToCommand(cmd, "msvc");
	#elif defined(cbPLATFORM_MACOS)
		cbAppendToCommand(cmd, "clang");
	#elif defined(cbPLATFORM_LINUX)
		cbAppendToCommand(cmd, "gcc");
	#endif
}

void cbAppendCFLAGS(cbCommand *cmd)
{
	cbAppendToCommand(cmd, "-framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL");
	// cbAppendToCommand(cmd, "-Wextra");
	// cbAppendToCommand(cmd, "-Werror");
}

void cbAppendLDFLAGS(cbCommand *cmd)
{
	cbAppendToCommand(cmd, "-L src/include");
}

void cbAppendOutput(cbCommand *cmd)
{
	#if defined(cbPLATFORM_WINDOWS)
		cbAppendToCommand(cmd, "-o", "TimelineMaker.exe");
	#elif defined(cbPLATFORM_MACOS)
		cbAppendToCommand(cmd, "-o", "TimelineMaker");
	#elif defined(cbPLATFORM_LINUX)
		cbAppendToCommand(cmd, "-o", "TimelineMaker");
	#endif

}

void BuildRaylib(void)
{
	cbCommand *buildRaylibCommand = cbCreateCommand();
	cbAppendToCommand(buildRaylibCommand, "cd src/libs/raylib-5.5/src && make PLATFORM=PLATFORM_DESKTOP");
	cbExecuteCommand(buildRaylibCommand);
}

void BuildRule(void)
{
	BuildRaylib();
	cbCommand *buildCommand = cbCreateCommand();
	cbAppendCompiler(buildCommand);
	cbAppendToCommand(buildCommand, "src/*.c");
	cbAppendToCommand(buildCommand, "src/libs/raylib-5.5/src/libraylib.a");
	cbAppendCFLAGS(buildCommand);
	cbAppendLDFLAGS(buildCommand);
	cbAppendOutput(buildCommand);
	cbExecuteCommand(buildCommand);
}

void ExecRule(void)
{
	BuildRule();
	cbCommand *execCommand = cbCreateCommand();
	#if defined(cbPLATFORM_WINDOWS)
		cbAppendToCommand(execCommand, "TimelineMaker.exe");
	#else
		cbAppendToCommand(execCommand, "./TimelineMaker");
	#endif
	cbExecuteCommand(execCommand);
}


int main(int argc, char **argv)
{
	cbCreateRule("build", BuildRule);
	cbCreateRule("exec", ExecRule);

	cbManageRules(argc, argv);
    return 0;
}

