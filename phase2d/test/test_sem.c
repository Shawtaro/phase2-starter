/*
 * Basic test that semaphores work correctly.
 *
 */

#include <string.h>
#include <stdlib.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <assert.h>
#include <libuser.h>
#include <libdisk.h>

#include "tester.h"
#include "phase2Int.h"

static int passed = FALSE;

static int flag = 0;

/*
 * Sleeper
 *
 * Sleeps for 10 seconds then V's the provided semaphore.
 */
int 
Sleeper(void *arg) 
{
    int sid = (int) arg;
    int rc;

    USLOSS_Console("Sleeping.\n");
    rc = Sys_Sleep(10);
    assert(rc == P1_SUCCESS);

    flag = 1;

    rc = Sys_SemV(sid);
    TEST(rc, P1_SUCCESS);

    return 0;
}

/*
 * Waiter
 * 
 * Waits on the provided semaphore.
 */
int 
Waiter(void *arg) 
{
    int sid = (int) arg;
    int rc;

    USLOSS_Console("Waiting.\n");
    rc = Sys_SemP(sid);
    TEST(rc, P1_SUCCESS);
    // make sure we actually waited for Sleeper.
    TEST(flag, 1);
    return 0;
}

#define MSG "This is a test!"

/*
 * Writer
 * 
 * Writes to the disk then V's the provided semaphore.
 */
int 
Writer(void *arg) 
{
    int sid = (int) arg;
    int rc;
    int bytesPerSector;
    int sectorsPerTrack;
    int tracks;
    char *buffer;

    rc = Sys_DiskSize(0, &bytesPerSector, &sectorsPerTrack, &tracks);
    assert(rc == P1_SUCCESS);
    buffer = malloc(bytesPerSector);
    strncpy(buffer, MSG, bytesPerSector);

    USLOSS_Console("Writing disk.\n");
    rc = Sys_DiskWrite(buffer, 0, 0, 1, 0);
    assert(rc == P1_SUCCESS);
    
    rc = Sys_SemV(sid);
    TEST(rc, P1_SUCCESS);
    return 0;
}
/*
 * Reader
 * 
 * P's the provided semaphore then reads from the disk.
 */
int 
Reader(void *arg) 
{
    int sid = (int) arg;
    int rc;
    int bytesPerSector;
    int sectorsPerTrack;
    int tracks;
    char *buffer;

    rc = Sys_DiskSize(0, &bytesPerSector, &sectorsPerTrack, &tracks);
    assert(rc == P1_SUCCESS);

    rc = Sys_SemP(sid);
    TEST(rc, P1_SUCCESS);

    buffer = malloc(bytesPerSector);

    USLOSS_Console("Reading disk.\n");
    rc = Sys_DiskRead(buffer, 0, 0, 1, 0);
    assert(rc == P1_SUCCESS);

    TEST(strncmp(buffer, MSG, bytesPerSector), 0);
    return 0;
}

int P3_Startup(void *arg) {

    int rc;
    int pid;
    int status;
    int sids[2];
    char buffer[P1_MAXNAME+1];
    char *name;

    name = "Sleeper/Waiter";
    rc = Sys_SemCreate(name, 0, &sids[0]);
    TEST(rc, P1_SUCCESS);

    rc = Sys_SemName(sids[0], buffer);
    TEST(rc, P1_SUCCESS);
    TEST(strncmp(name, buffer, sizeof(buffer)), 0);

    rc = Sys_Spawn("Waiter", Waiter, (void *) sids[0], 
                  USLOSS_MIN_STACK, 1, &pid);
    assert(rc == P1_SUCCESS);
    rc = Sys_Spawn("Sleeper", Sleeper, (void *) sids[0], 
                  USLOSS_MIN_STACK, 2, &pid);

    name = "Reader/Writer";
    rc = Sys_SemCreate(name, 0, &sids[1]);
    TEST(rc, P1_SUCCESS);

    rc = Sys_SemName(sids[1], buffer);
    TEST(rc, P1_SUCCESS);
    TEST(strncmp(name, buffer, sizeof(buffer)), 0);

    rc = Sys_Spawn("Reader", Reader, (void *) sids[1], 
                  USLOSS_MIN_STACK, 1, &pid);
    assert(rc == P1_SUCCESS);
    rc = Sys_Spawn("Writer", Writer, (void *) sids[1], 
                  USLOSS_MIN_STACK, 2, &pid);

    for (int i = 0; i < 4; i++) {
        rc = Sys_Wait(&pid, &status);
        assert(rc == P1_SUCCESS);
        assert(status == 0);
    }
    PASSED();
    return 0;
}

void test_setup(int argc, char **argv) {
    int rc;

    DeleteAllDisks();
    rc = Disk_Create(NULL, 0, 1);
    assert(rc == 0);
}

void test_cleanup(int argc, char **argv) {
    DeleteAllDisks();
    if (passed) {
        USLOSS_Console("TEST PASSED.\n");
    } else {
        USLOSS_Console("TEST FAILED!!\n");
    }
}
