/***********************************************************
Copyright 1991-1995 by Stichting Mathematisch Centrum, Amsterdam,
The Netherlands.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the names of Stichting Mathematisch
Centrum or CWI or Corporation for National Research Initiatives or
CNRI not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior
permission.

While CWI is the initial source for this software, a modified version
is made available by the Corporation for National Research Initiatives
(CNRI) at the Internet address ftp://ftp.python.org.

STICHTING MATHEMATISCH CENTRUM AND CNRI DISCLAIM ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL STICHTING MATHEMATISCH
CENTRUM OR CNRI BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.

******************************************************************/

/* Python interpreter main program */

#include "Python.h"
#include "osdefs.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef MS_WINDOWS
#include <fcntl.h>
#endif

#if defined(PYOS_OS2) || defined(MS_WINDOWS)
#define PYTHONHOMEHELP "<prefix>\\lib"
#else
#define PYTHONHOMEHELP "<prefix>/python1.5"
#endif

/* Interface to getopt(): */
extern int optind;
extern char *optarg;
extern int getopt(); /* PROTO((int, char **, char *)); -- not standardized */


/* For Py_GetArgcArgv(); set by main() */
static char **orig_argv;
static int  orig_argc;

/* Short usage message (with %s for argv0) */
static char *usage_line =
"usage: %s [-d] [-i] [-O] [-S] [-u] [-v] [-x] [-X] [-c cmd | file | -] [arg] ...\n";

/* Long usage message, split into parts < 512 bytes */
static char *usage_top = "\
Options and arguments (and corresponding environment variables):\n\
-d     : debug output from parser (also PYTHONDEBUG=x)\n\
-i     : inspect interactively after running script, (also PYTHONINSPECT=x)\n\
         and force prompts, even if stdin does not appear to be a terminal\n\
-O     : optimize generated bytecode (a tad.\n\
-S     : don't imply 'import site' on initialization\n\
-t     : issue warnings about inconsistent tab usage (-tt: issue errors)\n\
";
static char *usage_mid = "\
-u     : unbuffered binary stdout and stderr (also PYTHONUNBUFFERED=x)\n\
-v     : verbose (trace import statements) (also PYTHONVERBOSE=x)\n\
-x     : skip first line of source, allowing use of non-Unix forms of #!cmd\n\
-X     : disable class based built-in exceptions\n\
-c cmd : program passed in as string (terminates option list)\n\
file   : program read from script file\n\
-      : program read from stdin (default; interactive mode if a tty)\n\
";
static char *usage_bot = "\
arg ...: arguments passed to program in sys.argv[1:]\n\
Other environment variables:\n\
PYTHONSTARTUP: file executed on interactive startup (no default)\n\
PYTHONPATH   : '%c'-separated list of directories prefixed to the\n\
               default module search path.  The result is sys.path.\n\
PYTHONHOME   : alternate <prefix> directory (or <prefix>%c<exec_prefix>).\n\
               The default module search path uses %s.\n\
";


/* Main program */

int
Py_Main(argc, argv)
	int argc;
	char **argv;
{
	int c;
	int sts;
	char *command = NULL;
	char *filename = NULL;
	FILE *fp = stdin;
	char *p;
	int inspect = 0;
	int unbuffered = 0;
	int skipfirstline = 0;
	int stdin_is_interactive = 0;

	orig_argc = argc;	/* For Py_GetArgcArgv() */
	orig_argv = argv;

	if ((p = getenv("PYTHONINSPECT")) && *p != '\0')
		inspect = 1;
	if ((p = getenv("PYTHONUNBUFFERED")) && *p != '\0')
		unbuffered = 1;

	while ((c = getopt(argc, argv, "c:diOStuvxX")) != EOF) {
		if (c == 'c') {
			/* -c is the last option; following arguments
			   that look like options are left for the
			   the command to interpret. */
			command = malloc(strlen(optarg) + 2);
			if (command == NULL)
				Py_FatalError(
				   "not enough memory to copy -c argument");
			strcpy(command, optarg);
			strcat(command, "\n");
			break;
		}
		
		switch (c) {

		case 'd':
			Py_DebugFlag++;
			break;

		case 'i':
			inspect++;
			Py_InteractiveFlag++;
			break;

		case 'O':
			Py_OptimizeFlag++;
			break;

		case 'S':
			Py_NoSiteFlag++;
			break;

		case 't':
			Py_TabcheckFlag++;
			break;

		case 'u':
			unbuffered++;
			break;

		case 'v':
			Py_VerboseFlag++;
			break;

		case 'x':
			skipfirstline = 1;
			break;

		case 'X':
			Py_UseClassExceptionsFlag = 0;
			break;

		/* This space reserved for other options */

		default:
			fprintf(stderr, usage_line, argv[0]);
			fprintf(stderr, usage_top);
			fprintf(stderr, usage_mid);
			fprintf(stderr, usage_bot,
				DELIM, DELIM, PYTHONHOMEHELP);
			exit(2);
			/*NOTREACHED*/

		}
	}

	if (command == NULL && optind < argc &&
	    strcmp(argv[optind], "-") != 0)
	{
		filename = argv[optind];
		if (filename != NULL) {
			if ((fp = fopen(filename, "r")) == NULL) {
				fprintf(stderr, "%s: can't open file '%s'\n",
					argv[0], filename);
				exit(2);
			}
			else if (skipfirstline) {
				char line[256];
				fgets(line, sizeof line, fp);
			}
		}
	}

	stdin_is_interactive = Py_FdIsInteractive(stdin, (char *)0);

	if (unbuffered) {
#ifdef MS_WINDOWS
		_setmode(fileno(stdin), O_BINARY);
		_setmode(fileno(stdout), O_BINARY);
#endif
#ifndef MPW
#ifdef HAVE_SETVBUF
		setvbuf(stdin,  (char *)NULL, _IONBF, BUFSIZ);
		setvbuf(stdout, (char *)NULL, _IONBF, BUFSIZ);
		setvbuf(stderr, (char *)NULL, _IONBF, BUFSIZ);
#else /* !HAVE_SETVBUF */
		setbuf(stdin,  (char *)NULL);
		setbuf(stdout, (char *)NULL);
		setbuf(stderr, (char *)NULL);
#endif /* !HAVE_SETVBUF */
#else /* MPW */
		/* On MPW (3.2) unbuffered seems to hang */
		setvbuf(stdin,  (char *)NULL, _IOLBF, BUFSIZ);
		setvbuf(stdout, (char *)NULL, _IOLBF, BUFSIZ);
		setvbuf(stderr, (char *)NULL, _IOLBF, BUFSIZ);
#endif /* MPW */
	}
	else if (Py_InteractiveFlag) {
#ifdef MS_WINDOWS
		/* Doesn't have to have line-buffered -- use unbuffered */
		setvbuf(stdin,  (char *)NULL, _IONBF, BUFSIZ);
		setvbuf(stdout, (char *)NULL, _IONBF, BUFSIZ);
#else /* !MS_WINDOWS */
#ifdef HAVE_SETVBUF
		setvbuf(stdin,  (char *)NULL, _IOLBF, BUFSIZ);
		setvbuf(stdout, (char *)NULL, _IOLBF, BUFSIZ);
#endif /* HAVE_SETVBUF */
#endif /* !MS_WINDOWS */
		/* Leave stderr alone - it should be unbuffered anyway. */
  	}

	Py_SetProgramName(argv[0]);
	Py_Initialize();

	if (Py_VerboseFlag ||
	    (command == NULL && filename == NULL && stdin_is_interactive))
		fprintf(stderr, "Python %s on %s\n%s\n",
			Py_GetVersion(), Py_GetPlatform(), Py_GetCopyright());
	
	
	if (command != NULL) {
		/* Backup optind and force sys.argv[0] = '-c' */
		optind--;
		argv[optind] = "-c";
	}

	PySys_SetArgv(argc-optind, argv+optind);

	if ((inspect || (command == NULL && filename == NULL)) &&
	    isatty(fileno(stdin))) {
		PyObject *v;
		v = PyImport_ImportModule("readline");
		if (v == NULL)
			PyErr_Clear();
		else
			Py_DECREF(v);
	}

	if (command) {
		sts = PyRun_SimpleString(command) != 0;
		free(command);
	}
	else {
		if (filename == NULL && stdin_is_interactive) {
			char *startup = getenv("PYTHONSTARTUP");
			if (startup != NULL && startup[0] != '\0') {
				FILE *fp = fopen(startup, "r");
				if (fp != NULL) {
					(void) PyRun_SimpleFile(fp, startup);
					PyErr_Clear();
					fclose(fp);
				}
			}
		}
		sts = PyRun_AnyFile(
			fp,
			filename == NULL ? "<stdin>" : filename) != 0;
		if (filename != NULL)
			fclose(fp);
	}

	if (inspect && stdin_is_interactive &&
	    (filename != NULL || command != NULL))
		sts = PyRun_AnyFile(stdin, "<stdin>") != 0;

	Py_Finalize();
	return sts;
}


/* Make the *original* argc/argv available to other modules.
   This is rare, but it is needed by the secureware extension. */

void
Py_GetArgcArgv(argc, argv)
	int *argc;
	char ***argv;
{
	*argc = orig_argc;
	*argv = orig_argv;
}
