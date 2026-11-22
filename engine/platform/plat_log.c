/*
 * ASCENT platform layer: the session log and the crash handler.
 *
 * This file is new. Each run of the game writes a log file in the "logs"
 * directory of the user (see platform.h and docs/debug_logs.md). The log
 * keeps what is needed to look into a failure that a player reports: the
 * system, the settings, the main events of the game, the errors and, after
 * a crash, the place of the crash, the registers and the stack. On Windows
 * a crash also writes a minidump next to the log.
 *
 * Each line goes to the file with fflush at once, so that the end of the
 * log is kept when the program stops.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>

#include <SDL3/SDL.h>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>		// the types of MiniDumpWriteDump. dbghelp.dll is loaded at run time.
#else
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
#include <cpuid.h>
#endif

#include "platform.h"

#define LOG_MAX_FILES	256
#define LOG_MAX_PATH	600

static FILE *Log_file;
static char Log_dir[LOG_MAX_PATH];
static char Log_path[LOG_MAX_PATH];
static char Dump_path[LOG_MAX_PATH];
static SDL_SpinLock Log_lock;
static Uint64 Log_t0;		// SDL_GetPerformanceCounter at the start (it does not need SDL_Init)
static int Gl_errors_reported;

// ----------------------------------------------------------------------
// Writing

// Writes the text as lines with the time. The caller holds the lock (or
// is the crash handler).
static void write_lines(const char *text)
{
	double t = (double) (SDL_GetPerformanceCounter() - Log_t0) / (double) SDL_GetPerformanceFrequency();
	const char *p = text;
	int first = 1;

	if (!Log_file)
		return;
	while (*p) {
		const char *e = strchr(p, '\n');
		int n = e ? (int) (e - p) : (int) strlen(p);
		if (n > 0 && p[n - 1] == '\r')
			n--;
		if (n > 0 || !first) {
			if (first)
				fprintf(Log_file, "[%9.3f] %.*s\n", t, n, p);
			else
				fprintf(Log_file, "            %.*s\n", n, p);
		}
		first = 0;
		if (!e)
			break;
		p = e + 1;
	}
	fflush(Log_file);
}

static void log_text(const char *text)
{
	if (!Log_file)
		return;
	SDL_LockSpinlock(&Log_lock);
	write_lines(text);
	SDL_UnlockSpinlock(&Log_lock);
}

void plat_log(const char *format, ...)
{
	char buf[2048];
	va_list args;

	if (!Log_file)
		return;
	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	log_text(buf);
}

void plat_log_error(const char *format, ...)
{
	char buf[2048];
	va_list args;

	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	fputs(buf, stderr);
	if (buf[0] && buf[strlen(buf) - 1] != '\n')
		fputc('\n', stderr);
	log_text(buf);
}

const char *plat_log_path(void)
{
	return Log_file ? Log_path : NULL;
}

// The messages of SDL (SDL_Log, SDL_LogError, ...) go to the log too.
static SDL_LogOutputFunction Sdl_default_output;
static void *Sdl_default_data;

static void sdl_log_output(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
	static const char *names[] = { "", "trace", "verbose", "debug", "info", "warning", "error", "critical" };
	const char *name = (priority > 0 && priority < (int) SDL_arraysize(names)) ? names[priority] : "?";

	plat_log("SDL %s: %s", name, message);
	if (Sdl_default_output)
		Sdl_default_output(Sdl_default_data, category, priority, message);
}

// ----------------------------------------------------------------------
// Opening, rotation

static int make_dir(const char *dir)
{
	return SDL_CreateDirectory(dir);
}

static int name_compare(const void *a, const void *b)
{
	return strcmp(*(char * const *) a, *(char * const *) b);
}

// The names of the logs in dir (ascent-*.log), sorted from the oldest.
// The names sort by the start time. Returns the number.
static int list_logs(const char *dir, char **names, int max)
{
	int n = 0;
#ifdef _WIN32
	char pattern[LOG_MAX_PATH + 16];
	WIN32_FIND_DATAA fd;
	HANDLE h;

	snprintf(pattern, sizeof(pattern), "%s/ascent-*.log", dir);
	h = FindFirstFileA(pattern, &fd);
	if (h == INVALID_HANDLE_VALUE)
		return 0;
	do {
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && n < max)
			names[n++] = SDL_strdup(fd.cFileName);
	} while (FindNextFileA(h, &fd));
	FindClose(h);
#else
	DIR *d = opendir(dir);
	struct dirent *e;

	if (!d)
		return 0;
	while ((e = readdir(d)) != NULL && n < max) {
		size_t len = strlen(e->d_name);
		if (!strncmp(e->d_name, "ascent-", 7) && len > 4 && !strcmp(e->d_name + len - 4, ".log"))
			names[n++] = SDL_strdup(e->d_name);
	}
	closedir(d);
#endif
	qsort(names, n, sizeof(names[0]), name_compare);
	return n;
}

// How the session of the log ended: 1 an end line, 2 a crash report,
// 0 neither (the program stopped without a report, or it still runs).
static int log_end_state(const char *path)
{
	char buf[8193];
	FILE *f = fopen(path, "rb");
	long size;
	size_t n;

	if (!f)
		return 1;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, size > 8192 ? size - 8192 : 0, SEEK_SET);
	n = fread(buf, 1, 8192, f);
	fclose(f);
	buf[n] = 0;
	// the file can have a 0 byte at the end after a power loss
	for (size_t i = 0; i < n; i++)
		if (buf[i] == 0)
			buf[i] = ' ';
	if (strstr(buf, "Session end:"))
		return 1;
	if (strstr(buf, "CRASH"))
		return 2;
	return 0;
}

int plat_log_open(const char *dir, int keep)
{
	char *names[LOG_MAX_FILES];
	char base[64], prev_note[LOG_MAX_PATH + 200];
	time_t now = time(NULL);
	struct tm tmv;
	int n, i;

	if (Log_file)
		return 1;
	Log_t0 = SDL_GetPerformanceCounter();
	if (!dir || !dir[0] || strlen(dir) > LOG_MAX_PATH - 80)
		return 0;
	snprintf(Log_dir, sizeof(Log_dir), "%s", dir);
	make_dir(Log_dir);

	// The newest log before this one, and the rotation
	prev_note[0] = 0;
	n = list_logs(Log_dir, names, LOG_MAX_FILES);
	if (n > 0) {
		char path[LOG_MAX_PATH + 80];
		int state;
		snprintf(path, sizeof(path), "%s/%s", Log_dir, names[n - 1]);
		state = log_end_state(path);
		if (state == 0)
			snprintf(prev_note, sizeof(prev_note), "Note: the previous session (%s) did not end cleanly: its log has no end line. "
				"The program was stopped, the system stopped, or the game still runs.", names[n - 1]);
		else if (state == 2)
			snprintf(prev_note, sizeof(prev_note), "Note: the previous session (%s) ended with a crash.", names[n - 1]);
	}
	for (i = 0; i < n; i++) {
		if (i < n - (keep - 1)) {
			char path[LOG_MAX_PATH + 80];
			size_t len;
			snprintf(path, sizeof(path), "%s/%s", Log_dir, names[i]);
			remove(path);
			len = strlen(path);
			if (len > 4) {
				strcpy(path + len - 4, ".dmp");		// the crash dump of the log
				remove(path);
			}
		}
		SDL_free(names[i]);
	}

#ifdef _WIN32
	localtime_s(&tmv, &now);
#else
	localtime_r(&now, &tmv);
#endif
	strftime(base, sizeof(base), "ascent-%Y-%m-%d_%H-%M-%S", &tmv);
	// Two programs can start in the same second (tests): a number is added.
	for (i = 1; i < 100; i++) {
		FILE *f;
		if (i == 1)
			snprintf(Log_path, sizeof(Log_path), "%s/%s.log", Log_dir, base);
		else
			snprintf(Log_path, sizeof(Log_path), "%s/%s_%d.log", Log_dir, base, i);
		f = fopen(Log_path, "rb");
		if (!f)
			break;
		fclose(f);
	}
	Log_file = fopen(Log_path, "w");
	if (!Log_file)
		return 0;
	snprintf(Dump_path, sizeof(Dump_path), "%.*s.dmp", (int) strlen(Log_path) - 4, Log_path);

	{
		char date[64];
		strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", &tmv);
		plat_log("Session start: %s (local time)", date);
		plat_log("Log file: %s", Log_path);
	}
	if (prev_note[0])
		plat_log("%s", prev_note);

	SDL_GetLogOutputFunction(&Sdl_default_output, &Sdl_default_data);
	SDL_SetLogOutputFunction(sdl_log_output, NULL);
	return 1;
}

void plat_log_close(const char *how)
{
	if (!Log_file)
		return;
	plat_log_gl_errors("at the end");
	plat_log("Session end: %s", how ? how : "normal");
	SDL_LockSpinlock(&Log_lock);
	fclose(Log_file);
	Log_file = NULL;
	SDL_UnlockSpinlock(&Log_lock);
	SDL_SetLogOutputFunction(Sdl_default_output, Sdl_default_data);
}

// ----------------------------------------------------------------------
// The system

void plat_log_system(void)
{
	int v = SDL_GetVersion();
	char cpu[64] = "";

#ifdef _WIN32
	{
		typedef LONG (WINAPI *rtl_get_version_t)(OSVERSIONINFOW *);
		rtl_get_version_t rtl_get_version = (rtl_get_version_t) (void *) GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
		OSVERSIONINFOW vi;
		SYSTEM_INFO si;
		memset(&vi, 0, sizeof(vi));
		vi.dwOSVersionInfoSize = sizeof(vi);
		GetNativeSystemInfo(&si);
		if (rtl_get_version && rtl_get_version(&vi) == 0)
			plat_log("System: Windows %lu.%lu build %lu, %s", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber,
				si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? "x64" :
				si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64 ? "ARM64" : "x86");
	}
#else
	plat_log("System: %s", SDL_GetPlatform());
#endif

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
	{
		unsigned int r[12], k;
		if (__get_cpuid(0x80000000, &r[0], &r[1], &r[2], &r[3]) && r[0] >= 0x80000004) {
			for (k = 0; k < 3; k++)
				__get_cpuid(0x80000002 + k, &r[k * 4], &r[k * 4 + 1], &r[k * 4 + 2], &r[k * 4 + 3]);
			memcpy(cpu, r, 48);
			cpu[48] = 0;
		}
	}
#endif
	{
		char *p = cpu, *e;
		while (*p == ' ')
			p++;
		e = p + strlen(p);
		while (e > p && e[-1] == ' ')
			*--e = 0;
		plat_log("CPU: %s, %d logical cores, %d MB memory", *p ? p : "unknown", SDL_GetNumLogicalCPUCores(), SDL_GetSystemRAM());
	}
	plat_log("SDL: %d.%d.%d (%s)", SDL_VERSIONNUM_MAJOR(v), SDL_VERSIONNUM_MINOR(v), SDL_VERSIONNUM_MICRO(v), SDL_GetRevision());
}

#ifndef APIENTRY
#define APIENTRY
#endif
typedef const unsigned char *(APIENTRY *gl_get_string_t)(unsigned int);
typedef unsigned int (APIENTRY *gl_get_error_t)(void);

void plat_log_video(void)
{
	SDL_DisplayID *displays;
	SDL_Window **windows;
	int n, i;

	plat_log("Video driver: %s", SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "none");
	displays = SDL_GetDisplays(&n);
	for (i = 0; displays && i < n; i++) {
		const SDL_DisplayMode *m = SDL_GetDesktopDisplayMode(displays[i]);
		plat_log("Display %d: %s, %dx%d at %.0f Hz, content scale %.2f%s", i + 1,
			SDL_GetDisplayName(displays[i]) ? SDL_GetDisplayName(displays[i]) : "?",
			m ? m->w : 0, m ? m->h : 0, m ? m->refresh_rate : 0.0f, SDL_GetDisplayContentScale(displays[i]),
			displays[i] == SDL_GetPrimaryDisplay() ? " (primary)" : "");
	}
	SDL_free(displays);

	windows = SDL_GetWindows(&n);
	if (windows && n > 0) {
		SDL_Window *w = windows[0];
		int ww, wh, pw, ph;
		SDL_WindowFlags f = SDL_GetWindowFlags(w);
		SDL_GetWindowSize(w, &ww, &wh);
		SDL_GetWindowSizeInPixels(w, &pw, &ph);
		plat_log("Window: %dx%d (%dx%d pixels)%s%s%s", ww, wh, pw, ph,
			(f & SDL_WINDOW_FULLSCREEN) ? ", full screen" : "",
			(f & SDL_WINDOW_BORDERLESS) ? ", borderless" : "",
			(f & SDL_WINDOW_HIDDEN) ? ", hidden" : "");
		if (!(f & SDL_WINDOW_OPENGL)) {
			SDL_Renderer *r = SDL_GetRenderer(w);
			plat_log("Renderer: SDL renderer %s (no OpenGL 3.3: the GPU renderer is off)", r && SDL_GetRendererName(r) ? SDL_GetRendererName(r) : "?");
		}
	}
	SDL_free(windows);

	if (SDL_GL_GetCurrentContext()) {
		gl_get_string_t get_string = (gl_get_string_t) SDL_GL_GetProcAddress("glGetString");
		if (get_string) {
			const char *vendor = (const char *) get_string(0x1F00);
			const char *renderer = (const char *) get_string(0x1F01);
			const char *version = (const char *) get_string(0x1F02);
			const char *glsl = (const char *) get_string(0x8B8C);
			plat_log("OpenGL: %s, %s", vendor ? vendor : "?", renderer ? renderer : "?");
			plat_log("OpenGL version: %s, GLSL %s", version ? version : "?", glsl ? glsl : "?");
		}
		plat_log("GPU renderer available: %s", plat_gpu_available() ? "yes" : "no");
	}
}

void plat_log_gl_errors(const char *where)
{
	gl_get_error_t get_error;
	unsigned int e;
	int k;

	if (!Log_file || !SDL_GL_GetCurrentContext())
		return;
	get_error = (gl_get_error_t) SDL_GL_GetProcAddress("glGetError");
	if (!get_error)
		return;
	for (k = 0; k < 8 && (e = get_error()) != 0; k++) {
		if (Gl_errors_reported < 50)
			plat_log("OpenGL error 0x%04X (%s)", e, where);
		else if (Gl_errors_reported == 50)
			plat_log("OpenGL: more errors, not reported");
		Gl_errors_reported++;
	}
}

void plat_log_tick(void)
{
	static Uint64 last;
	Uint64 now = SDL_GetTicks();

	if (!Log_file || now - last < 5000)
		return;
	last = now;
	plat_log_gl_errors("check every 5 seconds");
}

// ----------------------------------------------------------------------
// The crash handler

static int Crash_quiet;

// Writes one line without the lock: the crash handler must not wait for
// a thread that stopped while it had the lock.
static void crash_line(const char *format, ...)
{
	static char buf[1024];
	va_list args;

	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	if (Log_file)
		write_lines(buf);
	fputs(buf, stderr);
	fputc('\n', stderr);
}

#ifdef _WIN32

typedef BOOL (WINAPI *mini_dump_write_dump_t)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
	PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

static mini_dump_write_dump_t Mini_dump_write_dump;
static LPTOP_LEVEL_EXCEPTION_FILTER Prev_filter;
static volatile LONG In_crash;
static ULONGLONG Exe_file_base;		// the image base in the file of the executable (for addr2line)
static HMODULE Exe_module;

// The image base of the executable as the file gives it. The loader can
// put the image at another address (ASLR); addr2line needs the address of
// the file: file base + (address - load address).
static ULONGLONG read_file_image_base(void)
{
	char path[MAX_PATH];
	unsigned char hdr[1024];
	FILE *f;
	size_t n;
	unsigned int pe;

	if (!GetModuleFileNameA(NULL, path, sizeof(path)))
		return 0;
	f = fopen(path, "rb");
	if (!f)
		return 0;
	n = fread(hdr, 1, sizeof(hdr), f);
	fclose(f);
	if (n < 0x40 || hdr[0] != 'M' || hdr[1] != 'Z')
		return 0;
	memcpy(&pe, hdr + 0x3C, 4);
	if (pe + 24 + 32 > n || memcmp(hdr + pe, "PE\0\0", 4))
		return 0;
	{
		unsigned short magic;
		memcpy(&magic, hdr + pe + 24, 2);
		if (magic == 0x20B) {		// PE32+: ImageBase is 8 bytes at 24 of the optional header
			ULONGLONG b;
			memcpy(&b, hdr + pe + 24 + 24, 8);
			return b;
		} else {						// PE32: 4 bytes at 28
			unsigned int b;
			memcpy(&b, hdr + pe + 24 + 28, 4);
			return b;
		}
	}
}

// The module of an address: its file name (no directory) and its load address.
static int address_module(ULONG_PTR addr, char *name, int size, ULONG_PTR *base)
{
	HMODULE m = NULL;
	char path[MAX_PATH];
	char *p;

	name[0] = 0;
	*base = 0;
	if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR) addr, &m) || !m)
		return 0;
	if (!GetModuleFileNameA(m, path, sizeof(path)))
		strcpy(path, "?");
	p = strrchr(path, '\\');
	snprintf(name, size, "%s", p ? p + 1 : path);
	*base = (ULONG_PTR) m;
	return 1;
}

static void crash_address_line(const char *label, ULONG_PTR addr)
{
	char mod[MAX_PATH];
	ULONG_PTR base;

	if (address_module(addr, mod, sizeof(mod), &base)) {
		if ((HMODULE) base == Exe_module && Exe_file_base)
			crash_line("%s 0x%016llX  %s+0x%llX  (addr2line 0x%llX)", label, (unsigned long long) addr, mod,
				(unsigned long long) (addr - base), (unsigned long long) (Exe_file_base + (addr - base)));
		else
			crash_line("%s 0x%016llX  %s+0x%llX", label, (unsigned long long) addr, mod, (unsigned long long) (addr - base));
	} else
		crash_line("%s 0x%016llX  (no module)", label, (unsigned long long) addr);
}

static int readable(const void *p, size_t n)
{
	MEMORY_BASIC_INFORMATION mi;

	if (!VirtualQuery(p, &mi, sizeof(mi)) || mi.State != MEM_COMMIT)
		return 0;
	if (mi.Protect & (PAGE_NOACCESS | PAGE_GUARD))
		return 0;
	return (const char *) p + n <= (const char *) mi.BaseAddress + mi.RegionSize;
}

static void crash_registers(const CONTEXT *c)
{
#if defined(_M_X64) || defined(__x86_64__)
	crash_line("Registers:");
	crash_line("  RAX %016llX  RBX %016llX  RCX %016llX  RDX %016llX", c->Rax, c->Rbx, c->Rcx, c->Rdx);
	crash_line("  RSI %016llX  RDI %016llX  RBP %016llX  RSP %016llX", c->Rsi, c->Rdi, c->Rbp, c->Rsp);
	crash_line("  R8  %016llX  R9  %016llX  R10 %016llX  R11 %016llX", c->R8, c->R9, c->R10, c->R11);
	crash_line("  R12 %016llX  R13 %016llX  R14 %016llX  R15 %016llX", c->R12, c->R13, c->R14, c->R15);
	crash_line("  RIP %016llX  EFLAGS %08lX", c->Rip, (unsigned long) c->EFlags);
#elif defined(_M_IX86) || defined(__i386__)
	crash_line("Registers:");
	crash_line("  EAX %08lX  EBX %08lX  ECX %08lX  EDX %08lX", c->Eax, c->Ebx, c->Ecx, c->Edx);
	crash_line("  ESI %08lX  EDI %08lX  EBP %08lX  ESP %08lX", c->Esi, c->Edi, c->Ebp, c->Esp);
	crash_line("  EIP %08lX  EFLAGS %08lX", c->Eip, c->EFlags);
#endif
}

// The stack trace: the unwind tables of x64 (RtlVirtualUnwind). GCC
// writes them for each function (.pdata, .xdata).
static void crash_stack(const CONTEXT *ctx)
{
#if defined(_M_X64) || defined(__x86_64__)
	static CONTEXT c;
	static DWORD64 pcs[1024];
	int i, n = 0;

	c = *ctx;
	while (n < 1024) {
		DWORD64 pc = c.Rip, image_base;
		PRUNTIME_FUNCTION f;

		if (pc == 0 && n > 0)
			break;
		pcs[n++] = pc;
		f = pc ? RtlLookupFunctionEntry(pc, &image_base, NULL) : NULL;
		if (f) {
			PVOID handler_data;
			DWORD64 establisher;
			RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, pc, f, &c, &handler_data, &establisher, NULL);
		} else {
			// a leaf function, or a call to a bad address: the return
			// address is at the top of the stack
			if (!readable((void *) c.Rsp, 8))
				break;
			c.Rip = *(DWORD64 *) c.Rsp;
			c.Rsp += 8;
		}
	}
	// A deep stack (a recursion): the first 40 frames and the last 16.
	crash_line("Stack (%d frames):", n);
	for (i = 0; i < n; i++) {
		char label[16];
		if (n > 56 && i == 40) {
			crash_line("  ... %d frames", n - 56);
			i = n - 16;
		}
		snprintf(label, sizeof(label), "  #%-3d", i);
		crash_address_line(label, (ULONG_PTR) pcs[i]);
	}
#else
	crash_line("Stack: no stack trace in this build");
#endif
}

typedef struct {
	EXCEPTION_POINTERS *ep;
	DWORD thread_id;
	BOOL ok;
} dump_job;

// The minidump is written by another thread: the thread of the crash can
// have no stack left (a stack overflow).
static DWORD WINAPI dump_thread(LPVOID param)
{
	dump_job *job = (dump_job *) param;
	MINIDUMP_EXCEPTION_INFORMATION mei;
	HANDLE f;

	f = CreateFileA(Dump_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE)
		return 0;
	mei.ThreadId = job->thread_id;
	mei.ExceptionPointers = job->ep;
	mei.ClientPointers = FALSE;
	job->ok = Mini_dump_write_dump(GetCurrentProcess(), GetCurrentProcessId(), f,
		(MINIDUMP_TYPE) (MiniDumpNormal | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules),
		job->ep ? &mei : NULL, NULL, NULL);
	CloseHandle(f);
	return 0;
}

static void crash_dump(EXCEPTION_POINTERS *ep)
{
	static dump_job job;
	HANDLE t;

	if (!Mini_dump_write_dump || !Log_file) {
		crash_line("Minidump: not written (no dbghelp.dll or no log)");
		return;
	}
	job.ep = ep;
	job.thread_id = GetCurrentThreadId();
	job.ok = FALSE;
	t = CreateThread(NULL, 256 * 1024, dump_thread, &job, 0, NULL);
	if (t) {
		WaitForSingleObject(t, 30000);
		CloseHandle(t);
	}
	crash_line(job.ok ? "Minidump: %s" : "Minidump: cannot write %s", Dump_path);
}

static const char *exception_name(DWORD code)
{
	switch (code) {
	case EXCEPTION_ACCESS_VIOLATION:		return "access violation";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:	return "array bounds exceeded";
	case EXCEPTION_BREAKPOINT:				return "breakpoint";
	case EXCEPTION_DATATYPE_MISALIGNMENT:	return "data misalignment";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:		return "float divide by zero";
	case EXCEPTION_FLT_INVALID_OPERATION:	return "float invalid operation";
	case EXCEPTION_FLT_OVERFLOW:			return "float overflow";
	case EXCEPTION_FLT_STACK_CHECK:			return "float stack check";
	case EXCEPTION_ILLEGAL_INSTRUCTION:		return "illegal instruction";
	case EXCEPTION_IN_PAGE_ERROR:			return "in page error";
	case EXCEPTION_INT_DIVIDE_BY_ZERO:		return "integer divide by zero";
	case EXCEPTION_INT_OVERFLOW:			return "integer overflow";
	case EXCEPTION_PRIV_INSTRUCTION:		return "privileged instruction";
	case EXCEPTION_STACK_OVERFLOW:			return "stack overflow";
	case 0xC0000409:						return "stack buffer overrun or fast fail";
	case 0xC0000374:						return "heap corruption";
	default:								return "exception";
	}
}

static void crash_message_box(void)
{
	char msg[LOG_MAX_PATH * 2 + 300];

	if (Crash_quiet)
		return;
	snprintf(msg, sizeof(msg), "ASCENT-DTU stopped because of an error.\n\n"
		"A report is in the log folder:\n%s\n\n"
		"Please send the files\n%s\n%s", Log_dir, Log_path, Dump_path);
	MessageBoxA(NULL, msg, "ASCENT-DTU", MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
}

static void crash_report(const char *what, EXCEPTION_POINTERS *ep, const CONTEXT *ctx)
{
	crash_line("CRASH: %s in thread %lu", what, (unsigned long) GetCurrentThreadId());
	if (ep && ep->ExceptionRecord) {
		EXCEPTION_RECORD *r = ep->ExceptionRecord;
		crash_line("Exception 0x%08lX (%s)", (unsigned long) r->ExceptionCode, exception_name(r->ExceptionCode));
		crash_address_line("Address", (ULONG_PTR) r->ExceptionAddress);
		if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r->NumberParameters >= 2)
			crash_line("The code %s the address 0x%016llX",
				r->ExceptionInformation[0] == 0 ? "read" : r->ExceptionInformation[0] == 1 ? "wrote" : "executed",
				(unsigned long long) r->ExceptionInformation[1]);
	}
	if (ctx) {
		crash_registers(ctx);
		crash_stack(ctx);
	}
	crash_dump(ep);
	crash_line("CRASH report end");
	crash_message_box();
}

static LONG WINAPI crash_filter(EXCEPTION_POINTERS *ep)
{
	char what[64];

	if (InterlockedExchange(&In_crash, 1))
		return EXCEPTION_EXECUTE_HANDLER;		// a crash in the report: stop
	snprintf(what, sizeof(what), "unhandled exception");
	crash_report(what, ep, ep ? ep->ContextRecord : NULL);
	// The handler of the C runtime calls the handlers of signal() (the
	// soak test has one).
	if (Prev_filter) {
		LONG r;
		In_crash = 0;
		r = Prev_filter(ep);
		if (r == EXCEPTION_CONTINUE_EXECUTION)
			return r;
	}
	return EXCEPTION_EXECUTE_HANDLER;		// end the program, no system dialog
}

static void crash_from_here(const char *what)
{
	static CONTEXT c;
	static EXCEPTION_RECORD r;
	static EXCEPTION_POINTERS ep;

	if (InterlockedExchange(&In_crash, 1))
		return;
	RtlCaptureContext(&c);
	memset(&r, 0, sizeof(r));
	r.ExceptionCode = 0xE0000001;		// not a system code: the report of abort() or a signal
#if defined(_M_X64) || defined(__x86_64__)
	r.ExceptionAddress = (PVOID) c.Rip;
#endif
	ep.ExceptionRecord = &r;
	ep.ContextRecord = &c;
	crash_report(what, &ep, &c);
}

static void signal_handler(int sig)
{
	crash_from_here(sig == SIGABRT ? "abort() (signal SIGABRT)" : "fatal signal");
	_exit(3);
}

static void invalid_parameter(const wchar_t *expr, const wchar_t *func, const wchar_t *file, unsigned int line, uintptr_t reserved)
{
	crash_from_here("invalid parameter of a C runtime function");
	TerminateProcess(GetCurrentProcess(), 3);
}

void plat_crash_install(int quiet)
{
	static int installed;
	HMODULE dbghelp;

	Crash_quiet = quiet;
	if (!installed) {
		installed = 1;
		Exe_module = GetModuleHandleA(NULL);
		Exe_file_base = read_file_image_base();
		dbghelp = LoadLibraryA("dbghelp.dll");
		if (dbghelp)
			Mini_dump_write_dump = (mini_dump_write_dump_t) (void *) GetProcAddress(dbghelp, "MiniDumpWriteDump");
		{
			// the stack that is left for the handler after a stack overflow
			ULONG guarantee = 64 * 1024;
			SetThreadStackGuarantee(&guarantee);
		}
		signal(SIGABRT, signal_handler);
		_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
		_set_invalid_parameter_handler(invalid_parameter);
		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	}
	// A second call (after plat_init) installs the filter again, in case a
	// library replaced it. The filter of the C runtime stays the previous one.
	{
		LPTOP_LEVEL_EXCEPTION_FILTER prev = SetUnhandledExceptionFilter(crash_filter);
		if (prev != crash_filter)
			Prev_filter = prev;
	}
}

#else		// not Windows: the signals only

static void signal_handler(int sig)
{
	crash_line("CRASH: signal %d", sig);
	crash_line("CRASH report end");
	_exit(3);
}

void plat_crash_install(int quiet)
{
	Crash_quiet = quiet;
	signal(SIGSEGV, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGABRT, signal_handler);
#ifdef SIGBUS
	signal(SIGBUS, signal_handler);
#endif
}

#endif
