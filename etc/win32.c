/* x86s running MS Windows NT 4.0 */

#include <string.h>

static char rcsid[] = "$Id$";

#ifndef LCCDIR
#define LCCDIR ""
#endif

char *suffixes[] = { ".c;.C", ".i;.I", ".asm;.ASM;.s;.S", ".obj;.OBJ", ".exe", 0 };
char inputs[256] = ".;\"/program files/devstudio/vc/include\";/msdev/include";
char *cpp[] = { LCCDIR "cpp", "-D__STDC__=1", "-Dwin32", "-D_WIN32", "$1", "$2", "$3", 0 };
char *include[] = { "-I" LCCDIR "include", 0 };
char *com[] = { LCCDIR "rcc", "-target=x86/win32", "$1", "$2", "$3", 0 };
char *as[] = { "ml", "-nologo", "-c", "-Cp", "-coff", "-Fo$3", "$1", "$2", 0 };
char *ld[] = { "link", "-nologo", 
	"-align:0x1000", "-subsystem:console", "-entry:mainCRTStartup",
	"$2", "-OUT:$3", "$1", LCCDIR "liblcc.lib", "libc.lib", "kernel32.lib", 0 };

extern char *concat(char *, char *);
extern int access(const char *, int);
extern char *replace(const char *, int, int);

int option(char *arg) {
	if (strncmp(arg, "-lccdir=", 8) == 0) {
		arg = replace(arg + 8, '/', '\\');
		if (arg[strlen(arg)-1] == '\\')
			arg[strlen(arg)-1] = '\0';
		cpp[0] = concat(arg, "\\cpp.exe");
		include[0] = concat("-I", concat(arg, "\\include"));
		com[0] = concat(arg, "\\rcc.exe");
		ld[8] = concat(arg, "\\liblcc.lib");
	} else if (strcmp(arg, "-g") == 0)
		ld[9] = "libcd.lib";
	else if (strcmp(arg, "-b") == 0)
		;
	else if (strncmp(arg, "-ld=", 4) == 0)
		ld[0] = &arg[4];
	else
		return 0;
	return 1;
}
