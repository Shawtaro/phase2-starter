#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <usloss.h>
#include <phase1.h>

#include "phase2Int.h"


static int      ClockDriver(void *);
static void     SleepStub(USLOSS_Sysargs *sysargs);
typedef struct{
    int pid;
    int sid;
    int wakeTime;
}Sleeper;

Sleeper sleepers[P1_MAXPROC];

/*
 * P2ClockInit
 *
 * Initialize the clock data structures and fork the clock driver.
 */
void 
P2ClockInit(void) 
{
    int rc;
    int pid;

    P2ProcInit();

    // initialize data structures here
    for (int i = 0; i < P1_MAXPROC; ++i){
        sleepers[i].pid=-1;
        sleepers[i].sid=-1;
        sleepers[i].wakeTime=0;
    }

    rc = P2_SetSyscallHandler(SYS_SLEEP, SleepStub);
    assert(rc == P1_SUCCESS);

    // fork the clock driver here
    rc = P1_Fork("Clock_Driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2 , 0, &pid);
    assert(rc == P1_SUCCESS);
}

/*
 * P2ClockShutdown
 *
 * Clean up the clock data structures and stop the clock driver.
 */

void 
P2ClockShutdown(void) 
{
    int rc;
    int status;
    // clean up
    for (int i = 0; i < P1_MAXPROC; ++i){
        if(sleepers[i].pid!=-1){
            rc = P1_SemFree(sleepers[i].sid);
            assert(rc ==P1_SUCCESS);
        }
        sleepers[i].pid=-1;
        sleepers[i].sid=-1;
        sleepers[i].wakeTime=0;
    }
    rc = USLOSS_DeviceInput(USLOSS_CLOCK_DEV,0,&status);
    //assert(rc ==P1_SUCCESS);
    // stop clock driver
    rc=P1_WakeupDevice(USLOSS_CLOCK_DEV, 0,status,TRUE);
    //assert(rc ==P1_SUCCESS);
}

/*
 * ClockDriver
 *
 * Kernel process that manages the clock device and wakes sleeping processes.
 */
static int 
ClockDriver(void *arg) 
{

    while(1) {
        int rc;
        int now;

        // wait for the next interrupt
        rc = P1_WaitDevice(USLOSS_CLOCK_DEV, 0, &now);
        if (rc == P1_WAIT_ABORTED) {
            break;
        }
        assert(rc == P1_SUCCESS);
        // wakeup any sleeping processes whose wakeup time has arrived
        for (int i = 0; i < P1_MAXPROC; ++i){
            if(sleepers[i].pid!=-1){
                // free sem after waking up
                if(sleepers[i].wakeTime<=now){
                    rc = P1_V(sleepers[i].sid);
                    //assert(rc ==P1_SUCCESS);
                    rc = P1_SemFree(sleepers[i].sid);
                    //assert(rc ==P1_SUCCESS);
                    sleepers[i].wakeTime=0;
                    sleepers[i].pid=-1;
                    sleepers[i].sid = -1;
                }
            }
        }
    }
    return P1_SUCCESS;
}

/*
 * P2_Sleep
 *
 * Causes the current process to sleep for the specified number of seconds.
 */
int 
P2_Sleep(int seconds) 
{
    int rc = P1_SUCCESS;
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0){
        USLOSS_IllegalInstruction();
    }
    if(seconds<0){
        return P2_INVALID_SECONDS;
    }
    // add current process to data structure of sleepers
    // wait until sleep is complete
    int pid = P1_GetPid();
    char name[P1_MAXNAME];
    sleepers[pid].pid=pid;
    snprintf(name, sizeof(name), "Sem %d", pid);
    rc = P1_SemCreate(name,0,&sleepers[pid].sid);
    //assert(rc == P1_SUCCESS);
    int time;
    rc = USLOSS_DeviceInput(USLOSS_CLOCK_DEV,0,&time);
    //assert(rc ==P1_SUCCESS);
    sleepers[pid].wakeTime=time+seconds*1000000;
    rc = P1_P(sleepers[pid].sid);
    //assert(rc ==P1_SUCCESS);
    return P1_SUCCESS;
}

/*
 * SleepStub
 *
 * Stub for the Sys_Sleep system call.
 */
static void 
SleepStub(USLOSS_Sysargs *sysargs) 
{
    int seconds = (int) sysargs->arg1;
    int rc = P2_Sleep(seconds);
    sysargs->arg4 = (void *) rc;
}

