#include <stdlib.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <stdio.h>
#include <assert.h>
#include <libuser.h>
#include <usyscall.h>

#include "phase2Int.h"

#define TAG_KERNEL 0
#define TAG_USER 1

static void SpawnStub(USLOSS_Sysargs *sysargs);
static void GetTimeOfDayStub(USLOSS_Sysargs *sysargs);
static void TerminateStub(USLOSS_Sysargs *sysargs);
static void WaitStub(USLOSS_Sysargs *sysargs);
static void ProcInfoStub(USLOSS_Sysargs *sysargs);
static void GetPidStub(USLOSS_Sysargs *sysargs);
void (*syscallTable[USLOSS_MAX_SYSCALLS])(USLOSS_Sysargs *args);

/*
 * IllegalHandler
 *
 * Handler for illegal instruction interrupts.
 *
 */

static void 
IllegalHandler(int type, void *arg) 
{
    P1_ProcInfo info;
    assert(type == USLOSS_ILLEGAL_INT);
    int pid = P1_GetPid();
    int rc = P1_GetProcInfo(pid, &info);
    assert(rc == P1_SUCCESS);
    if (info.tag == TAG_KERNEL) {
        P1_Quit(1024);
    } else {
        P2_Terminate(2048);
    }
}

/*
 * SyscallHandler
 *
 * Handler for system call interrupts.
 *
 */

static void 
SyscallHandler(int type, void *arg) 
{
    USLOSS_Sysargs* sa = (USLOSS_Sysargs*) arg;
    //USLOSS_Console("%d\n",sa->number);
    syscallTable[sa->number-1](sa);
}


/*
 * P2ProcInit
 *
 * Initialize everything.
 *
 */

void
P2ProcInit(void) 
{
    int rc;

    USLOSS_IntVec[USLOSS_ILLEGAL_INT] = IllegalHandler;
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = SyscallHandler;

    // call P2_SetSyscallHandler to set handlers for all system calls
    rc = P2_SetSyscallHandler(SYS_SPAWN, SpawnStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_GETTIMEOFDAY, GetTimeOfDayStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_WAIT, WaitStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_TERMINATE, TerminateStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_GETPID, GetPidStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_GETPROCINFO, ProcInfoStub);
    assert(rc == P1_SUCCESS);
}

/*
 * P2_SetSyscallHandler
 *
 * Set the system call handler for the specified system call.
 *
 */

int
P2_SetSyscallHandler(unsigned int number, void (*handler)(USLOSS_Sysargs *args))
{
    // check kernel mode
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0){
        USLOSS_IllegalInstruction();
    }

    if(number <= 0 || number > USLOSS_MAX_SYSCALLS){
        return P2_INVALID_SYSCALL;
    }
    syscallTable[number-1]=handler;
    return P1_SUCCESS;
}

// a wrapper function to do quit
int (*currentFunc)(void*);
int wrapper(void* arg){
    int rc;
    int status;
    rc=USLOSS_PsrSet(USLOSS_PsrGet()&~USLOSS_PSR_CURRENT_MODE);
    status = currentFunc(arg);
    Sys_Terminate(status);
    return 0;
}

/*
 * P2_Spawn
 *
 * Spawn a user-level process.
 *
 */
int 
P2_Spawn(char *name, int(*func)(void *arg), void *arg, int stackSize, int priority, int *pid) 
{
    // check kernel mode
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0){
        USLOSS_IllegalInstruction();
    }
    int rc;
    currentFunc = func;
    rc = P1_Fork(name,wrapper,arg,stackSize,priority,TAG_USER,pid);
    if(rc!=P1_SUCCESS){
        return rc;
    }
    return P1_SUCCESS;
}

/*
 * P2_Wait
 *
 * Wait for a user-level process.
 *
 */

int 
P2_Wait(int *pid, int *status) 
{
    // check kernel mode
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0){
        USLOSS_IllegalInstruction();
    }
    int rc;
    rc= P1_Join(TAG_USER,pid,status);
    if(rc!=P1_SUCCESS){
        return rc;
    }
    return P1_SUCCESS;
}

/*
 * P2_Terminate
 *
 * Terminate a user-level process.
 *
 */

void 
P2_Terminate(int status) 
{
    // check kernel mode
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0){
        USLOSS_IllegalInstruction();
    }
    P1_Quit(status);
}

/*
 * SpawnStub
 *
 * Stub for Sys_Spawn system call. 
 *
 */
static void 
SpawnStub(USLOSS_Sysargs *sysargs) 
{
    int (*func)(void *) = sysargs->arg1;
    void *arg = sysargs->arg2;
    int stackSize = (int) sysargs->arg3;
    int priority = (int) sysargs->arg4;
    char *name = sysargs->arg5;
    int pid;
    int rc = P2_Spawn(name, func, arg, stackSize, priority, &pid);
    if (rc == P1_SUCCESS) {
        sysargs->arg1 = (void *) pid;
    }
    sysargs->arg4 = (void *) rc;
}

static void 
TerminateStub(USLOSS_Sysargs *sysargs)
{
    P2_Terminate((int) sysargs->arg1);
}

static void 
WaitStub(USLOSS_Sysargs *sysargs)
{
    int pid;
    int status;
    int rc;
    rc=P2_Wait(&pid,&status);
    sysargs->arg1=(void *) pid;
    sysargs->arg2=(void *) status;
    sysargs->arg4=(void *) rc;
}

static void 
ProcInfoStub(USLOSS_Sysargs *sysargs)
{
    int pid = (int) sysargs->arg1;
    P1_ProcInfo *info = (P1_ProcInfo*) sysargs->arg2;
    sysargs->arg4=(void *) P1_GetProcInfo(pid,info);

}

static void 
GetPidStub(USLOSS_Sysargs *sysargs)
{
    sysargs->arg1 = (void *) P1_GetPid();
}

/*
 * GetTimeOfDayStub
 *
 * Stub for Sys_GetTimeOfDay system call. 
 *
 */
static void 
GetTimeOfDayStub(USLOSS_Sysargs *sysargs) 
{
    int status;
    int rc;
    rc=USLOSS_DeviceInput(USLOSS_CLOCK_DEV,0,&status);
    sysargs->arg1 = (void *) status;
}





