
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#define		DEFAULT_STRING_ALLOCATION_SIZE	500
#define		MAX_RULES 100

typedef uint8_t U8;

#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS
    #include <windows.h>
#elif defined(__linux__) || defined(__APPLE__) && defined(__MACH__)
	#if defined(__linux__)
		#define PLATFORM_LINUX
	#elif defined(__APPLE__) && defined(__MACH__)
		#define PLATFORM_MACOS
	#endif

    #define PLATFORM_UNIX
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
#else
    #error "Unsupported platform"
#endif


typedef struct String
{
	char *c_str;              // Pointer to the character array
	unsigned int length;      // Current length of the string
	unsigned int size;        // Allocated size of the string buffer
} String;

typedef String Command;

typedef void(*RuleFunction)(void);
typedef struct Rule
{
	char			*name;
	RuleFunction	function;
} Rule;

Rule Rules[MAX_RULES];
int	RuleCount;


void CreateRule(const char *ruleName, RuleFunction function)
{
	if (RuleCount >= MAX_RULES)
		return;
	Rules[RuleCount].name = strdup(ruleName);
	Rules[RuleCount].function = function;
	RuleCount++;
}

U8 ExecuteRule(const char *rule)
{
	for (int i = 0; i < RuleCount; i++)
	{
		if (strcmp(Rules[i].name, rule) == 0)
		{
			printf("Executing rule %s\n", Rules[i].name);
			Rules[i].function();
			return 1;
		}
	}
	return 0;
}


String *CreateString(void)
{
	String *string = (String *)malloc(sizeof(String)); // Allocate memory for String structure
	string->c_str = (char *)malloc(DEFAULT_STRING_ALLOCATION_SIZE * sizeof(char));
	string->length = 0;
	string->size = DEFAULT_STRING_ALLOCATION_SIZE;
	string->c_str[0] = '\0';

	return string;
}
#define CreateCommand CreateString


void AppendToString(String *str, const char *element)
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

void AppendToCommandNULL(Command *command, ...)
{
    if (!command) return;

    va_list args;
    va_start(args, command);

    const char *arg = va_arg(args, const char *);
    while (arg != NULL)
    {
        if (command->length > 0)
        {
            AppendToString(command, " ");
        }

        AppendToString(command, arg);
        arg = va_arg(args, const char *);
    }
    va_end(args);
}
#define AppendToCommand(command, ...) AppendToCommandNULL(command, __VA_ARGS__, NULL)


int ExecuteCommand(const Command *command)
{
    if (!command || !command->c_str)
    {
        fprintf(stderr, "Invalid command: NULL pointer\n");
        return -1;
    }

	printf("[CMD] : %s\n", command->c_str);
#ifdef PLATFORM_WINDOWS
    // On Windows, use CreateProcess
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si. = sizeof(si);
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
        fprintf(stderr, "Command execution failed: %lu\n", GetLastError());
        free(cmdBuffer);
        return -1;
    }

    // Wait for the process to complete
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Clean up
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    free(cmdBuffer);

#elif defined(PLATFORM_UNIX)
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


void RebuildSelf(void)
{
    Command *selfBuildCmd = CreateCommand();

#ifdef PLATFORM_WINDOWS
    AppendToCommand(selfBuildCmd, "gcc", "-o", "CBuilder.exe", "CBuilder.c");
#else
    AppendToCommand(selfBuildCmd, "gcc", "-o", "CBuilder", "CBuilder.c");
#endif
    printf("Rebuilding CBuilder: %s\n", selfBuildCmd->c_str);

    int result = ExecuteCommand(selfBuildCmd);
    if (result != 0)
    {
        fprintf(stderr, "Failed to rebuild CBuilder with code: %d\n", result);
        exit(EXIT_FAILURE);
    }

    printf("CBuilder rebuilt successfully.\n");
    free(selfBuildCmd->c_str);
    free(selfBuildCmd);
}

void RunSelf(String *args)
{
	Command *cmd = CreateCommand();
#ifdef PLATFORM_WINDOWS
	AppendToCommand(cmd, "CBuilder.exe");
#else
	AppendToCommand(cmd, "./CBuilder");
#endif
	AppendToCommand(cmd, args->c_str);
	printf("CMD %s\n", cmd->c_str);
	ExecuteCommand(cmd);
}

void ManageRebuild(int argc, char **argv)
{
	int rebuild = 1;
	String *args = CreateString();
	for (int i = 1; i < argc; i++)
	{
		AppendToCommand(args, argv[i]);
		if (strcmp(argv[i], "NOREBUILD") == 0)
			rebuild = 0;
	}
	if (rebuild)
	{
		RebuildSelf(); AppendToCommand(args, "NOREBUILD");
		RunSelf(args);
		exit(0);
	}
}
void ManageRules(int argc, char **argv)
{
	U8 retValue;
	ManageRebuild(argc, argv);
	for (int i = 1; i < argc; i++)
		retValue = ExecuteRule(argv[i]);
	if (!retValue && RuleCount)
		Rules[0].function();
}


void AppendCompiler(Command *cmd)
{
	#if defined(PLATFORM_WINDOWS)
		AppendToCommand(cmd, "msvc");
	#elif defined(PLATFORM_MACOS)
		AppendToCommand(cmd, "clang");
	#elif defined(PLATFORM_LINUX)
		AppendToCommand(cmd, "gcc");
	#endif
}

void AppendCFLAGS(Command *cmd)
{
	AppendToCommand(cmd, "-framework CoreVideo -framework CoreAudio ");
	AppendToCommand(cmd,
				   "-framework Cocoa",
				   "-framework IOKit",
				   "-framework GLUT",
				   "-framework OpenGL",
				   "-framework GameController",
				   "-framework AudioToolbox",
				   "-framework Metal",
				   "-framework CoreHaptics",
				   "-framework ForceFeedback",
				   "-framework Carbon",
				   "-framework AVFoundation");
	
	AppendToCommand(cmd, "-I extern/SDL-release-2.30.11/include/ ");
	AppendToCommand(cmd, "-I extern/SDL_mixer-release-2.8.0/include/ ");
	AppendToCommand(cmd, "-I extern/fftw-3.3.10/api/ ");

	// AppendToCommand(cmd, "-Wextra");
	// AppendToCommand(cmd, "-Werror");
}

void AppendLDFLAGS(Command *cmd)
{
	AppendToCommand(cmd, "-L extern/SDL-release-2.30.11/build/ ");
	AppendToCommand(cmd, "-L extern/SDL_mixer-release-2.8.0/build/.libs/ ");
	AppendToCommand(cmd, "-L extern/fftw-3.3.10/.libs/ ");

}

void AppendOutput(Command *cmd)
{
	#if defined(PLATFORM_WINDOWS)
		AppendToCommand(cmd, "-o", "TileTimeRythm.exe");
	#elif defined(PLATFORM_MACOS)
		AppendToCommand(cmd, "-o", "TileTimeRythm");
	#elif defined(PLATFORM_LINUX)
		AppendToCommand(cmd, "-o", "TileTimeRythm");
	#endif

}

void BuildRule(void)
{
	/* BuildFFTW(); */
	/* BuildSDL(); */
	/* BuildSDL_mixer(); */
	Command *buildCommand = CreateCommand();
	AppendCompiler(buildCommand);
	AppendToCommand(buildCommand, "extern/SDL-release-2.30.11/build/libSDL2.a");
	AppendToCommand(buildCommand, "extern/SDL_mixer-release-2.8.0/build/.libs/libSDL2_mixer.a");
	AppendToCommand(buildCommand, "extern/fftw-3.3.10/.libs/libfftw3.a");
	AppendToCommand(buildCommand, "src/*.c");
	AppendCFLAGS(buildCommand);
	AppendLDFLAGS(buildCommand);
	AppendOutput(buildCommand);
	ExecuteCommand(buildCommand);
}

void ExecRule(void)
{
	BuildRule();
	Command *execCommand = CreateCommand();
	#if defined(PLATFORM_WINDOWS)
		AppendToCommand(execCommand, "TileTimeRythm.exe");
	#else
		AppendToCommand(execCommand, "./TileTimeRythm");
	#endif
	ExecuteCommand(execCommand);
}

int main(int argc, char **argv)
{
	CreateRule("exec", ExecRule);
	CreateRule("build", BuildRule);

	ManageRules(argc, argv);
    return 0;
}

