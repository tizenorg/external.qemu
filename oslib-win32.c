/*
 * os-win32.c
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 * Copyright (c) 2010 Red Hat, Inc.
 *
 * QEMU library functions for win32 which are shared between QEMU and
 * the QEMU tools.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <windows.h>
#include "config-host.h"
#include "sysemu.h"
#include "trace.h"
#include "qemu_socket.h"

#ifdef CONFIG_MARU
#include "../tizen/src/skin/maruskin_client.h"

#ifdef CONFIG_WIN32
typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;

int is_wow64_temp(void)
{
    int result = 0;

    //IsWow64Process is not available on all supported versions of Windows.
    //Use GetModuleHandle to get a handle to the DLL that contains the function
    //and GetProcAddress to get a pointer to the function if available.

    fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(
        GetModuleHandle(TEXT("kernel32")),"IsWow64Process");

    if(NULL != fnIsWow64Process)
    {
        if (!fnIsWow64Process(GetCurrentProcess(),&result))
        {
            //handle error
			//INFO("Can not find 'IsWow64Process'\n");
        }
    }
    return result;
}

int get_java_path_temp(char** java_path)
{
	HKEY hKeyNew;
	HKEY hKey;
	//char strJavaRuntimePath[JAVA_MAX_COMMAND_LENGTH] = {0};
	char strChoosenName[JAVA_MAX_COMMAND_LENGTH] = {0};
	char strSubKeyName[JAVA_MAX_COMMAND_LENGTH] = {0};
	char strJavaHome[JAVA_MAX_COMMAND_LENGTH] = {0};
	int index;
	DWORD dwSubKeyNameMax = JAVA_MAX_COMMAND_LENGTH;
	DWORD dwBufLen = JAVA_MAX_COMMAND_LENGTH;

    RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\JavaSoft\\Java Runtime Environment", 0,
                                     KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS | MY_KEY_WOW64_64KEY, &hKey);
    RegEnumKeyEx(hKey, 0, (LPSTR)strSubKeyName, &dwSubKeyNameMax, NULL, NULL, NULL, NULL);
    strcpy(strChoosenName, strSubKeyName);

  	index = 1;
  	while (ERROR_SUCCESS == RegEnumKeyEx(hKey, index, (LPSTR)strSubKeyName, &dwSubKeyNameMax,
			NULL, NULL, NULL, NULL)) {
        if (strcmp(strChoosenName, strSubKeyName) < 0) {
            strcpy(strChoosenName, strSubKeyName);
        }
        index++;
  	}

  	RegOpenKeyEx(hKey, strChoosenName, 0, KEY_QUERY_VALUE | MY_KEY_WOW64_64KEY, &hKeyNew);
  	RegQueryValueEx(hKeyNew, "JavaHome", NULL, NULL, (LPBYTE)strJavaHome, &dwBufLen);
  	RegCloseKey(hKey);
	if (strJavaHome[0] != '\0') {
        sprintf(*java_path, "\"%s\\bin\\java\"", strJavaHome);
  		//strcpy(*java_path, strJavaHome);
  		//strcat(*java_path, "\\bin\\java");
    } else {
		return 0;
	}
    return 1;
}
#endif
#endif // CONFIG_MARU

void *qemu_oom_check(void *ptr)
{
    if (ptr == NULL) {
        fprintf(stderr, "Failed to allocate memory: %lu\n", GetLastError());

#ifdef CONFIG_MARU
        char _msg[] = "Failed to allocate memory in qemu.";
        char cmd[JAVA_MAX_COMMAND_LENGTH] = { 0, };

#ifdef CONFIG_WIN32
        char* JAVA_EXEFILE_PATH = malloc(JAVA_MAX_COMMAND_LENGTH);
        memset(JAVA_EXEFILE_PATH, 0, JAVA_MAX_COMMAND_LENGTH);
        if (is_wow64_temp()) {            
            if (!get_java_path_temp(&JAVA_EXEFILE_PATH)) {
				strcpy(JAVA_EXEFILE_PATH, "java");
		    }
        } else {
            strcpy(JAVA_EXEFILE_PATH, "java");
        }
#endif
        int len = strlen(JAVA_EXEFILE_PATH) + strlen(JAVA_EXEOPTION) + strlen(JAR_SKINFILE_PATH) +
           strlen(JAVA_SIMPLEMODE_OPTION) + strlen(_msg) + 7;
        if (len > JAVA_MAX_COMMAND_LENGTH) {
            len = JAVA_MAX_COMMAND_LENGTH;
        }

        snprintf(cmd, len, "%s %s %s %s=\"%s\"",
            JAVA_EXEFILE_PATH, JAVA_EXEOPTION, JAR_SKINFILE_PATH, JAVA_SIMPLEMODE_OPTION, _msg);
        int ret = WinExec(cmd, SW_SHOW);
#ifdef CONFIG_WIN32
	    // for 64bit windows
	    free(JAVA_EXEFILE_PATH);
	    JAVA_EXEFILE_PATH=0;
#endif
#endif

        abort();
    }
    return ptr;
}

void *qemu_memalign(size_t alignment, size_t size)
{
    void *ptr;

    if (!size) {
        abort();
    }
    ptr = qemu_oom_check(VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE));
    trace_qemu_memalign(alignment, size, ptr);
    return ptr;
}

void *qemu_vmalloc(size_t size)
{
    void *ptr;

    /* FIXME: this is not exactly optimal solution since VirtualAlloc
       has 64Kb granularity, but at least it guarantees us that the
       memory is page aligned. */
    if (!size) {
        abort();
    }
    ptr = qemu_oom_check(VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE));
    trace_qemu_vmalloc(size, ptr);
    return ptr;
}

void qemu_vfree(void *ptr)
{
    trace_qemu_vfree(ptr);
    VirtualFree(ptr, 0, MEM_RELEASE);
}

void socket_set_block(int fd)
{
    unsigned long opt = 0;
    ioctlsocket(fd, FIONBIO, &opt);
}

void socket_set_nonblock(int fd)
{
    unsigned long opt = 1;
    ioctlsocket(fd, FIONBIO, &opt);
}

int inet_aton(const char *cp, struct in_addr *ia)
{
    uint32_t addr = inet_addr(cp);
    if (addr == 0xffffffff) {
	return 0;
    }
    ia->s_addr = addr;
    return 1;
}

void qemu_set_cloexec(int fd)
{
}

/* Offset between 1/1/1601 and 1/1/1970 in 100 nanosec units */
#define _W32_FT_OFFSET (116444736000000000ULL)

int qemu_gettimeofday(qemu_timeval *tp)
{
  union {
    unsigned long long ns100; /*time since 1 Jan 1601 in 100ns units */
    FILETIME ft;
  }  _now;

  if(tp) {
      GetSystemTimeAsFileTime (&_now.ft);
      tp->tv_usec=(long)((_now.ns100 / 10ULL) % 1000000ULL );
      tp->tv_sec= (long)((_now.ns100 - _W32_FT_OFFSET) / 10000000ULL);
  }
  /* Always return 0 as per Open Group Base Specifications Issue 6.
     Do not set errno on error.  */
  return 0;
}
