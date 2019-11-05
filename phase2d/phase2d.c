
#include <string.h>
#include <stdlib.h>
#include <usloss.h>
#include <phase1.h>
#include <assert.h>
#include <libuser.h>
#include <libdisk.h>

#include "phase2Int.h"

static void     CreateStub(USLOSS_Sysargs *sysargs);
static void     PStub(USLOSS_Sysargs *sysargs);
static void     VStub(USLOSS_Sysargs *sysargs);
static void     FreeStub(USLOSS_Sysargs *sysargs);
static void     NameStub(USLOSS_Sysargs *sysargs);

/*
 * I left this useful function here for you to use for debugging. If you add -DDEBUG to CFLAGS
 * this will produce output, otherwise it won't. The message is printed including the function
 * and line from which it was called. I don't remember why it is called "debug2".
 */

static void debug2(char *fmt, ...)
{
    #ifdef DEBUG
    va_list ap;
    va_start(ap, fmt);
    USLOSS_Console("[%s:%d] ", __PRETTY_FUNCTION__, __LINE__);   \
    USLOSS_VConsole(fmt, ap);
    #endif
}

int P2_Startup(void *arg)
{
    int rc, pid;
    int waitPid,status;
    // initialize clock and disk drivers
    debug2("starting\n");
    P2ClockInit();
    P2DiskInit();
    rc = P2_SetSyscallHandler(SYS_SEMCREATE, CreateStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_SEMP, PStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_SEMV, VStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_SEMFREE, FreeStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_SEMNAME, NameStub);
    assert(rc == P1_SUCCESS);
    // ...
    rc = P2_Spawn("P3_Startup", P3_Startup, NULL, 4*USLOSS_MIN_STACK, 3, &pid);
    assert(rc == P1_SUCCESS);
    // ...
    rc = P2_Wait(&waitPid, &status);
    // shut down clock and disk drivers
    P2DiskShutdown();
    P2ClockShutdown();
    return 0;
}

static void
CreateStub(USLOSS_Sysargs *sysargs)
{
    sysargs->arg4 = (void *) P1_SemCreate((char *) sysargs->arg2, (int) sysargs->arg1, 
                                          (void *) &sysargs->arg1);
}

static void
PStub(USLOSS_Sysargs *sysargs)
{
    sysargs->arg4=(void *) P1_P((int) sysargs->arg1);
}

static void
VStub(USLOSS_Sysargs *sysargs)
{
    sysargs->arg4=(void *) P1_V((int) sysargs->arg1);
}

static void
FreeStub(USLOSS_Sysargs *sysargs)
{
    sysargs->arg4=(void *) P1_SemFree((int) sysargs->arg1);
}

static void
NameStub(USLOSS_Sysargs *sysargs)
{
    sysargs->arg4=(void *) P1_SemName((int) sysargs->arg1,(char*) sysargs->arg2);
}