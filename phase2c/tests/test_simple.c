/*
 *
 *  Created on: Mar 8, 2015
 *      Author: jeremy
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <usloss.h>
#include <phase1.h>
#include <assert.h>
#include <libuser.h>
#include <libdisk.h>

#include "tester.h"
#include "phase2Int.h"

static int passed = FALSE;

#define MSG1 "This is a test1."
#define MSG2 "This is a test2."
#define MSG3 "This is a test3."

int P3_Startup(void *arg) {
    char buffer[USLOSS_DISK_SECTOR_SIZE];
    strncpy(buffer, MSG1, sizeof(buffer));

    USLOSS_Console("Write to the disk.\n");
    int rc = Sys_DiskWrite(buffer, 0, 0, 1, 0);
    USLOSS_Console("Verify that the disk write was successful.\n");
    assert(rc == P1_SUCCESS);
    USLOSS_Console("Wrote \"%s\".\n", buffer);

    strncpy(buffer, MSG2, sizeof(buffer));

    USLOSS_Console("Write to the disk.\n");
    rc = Sys_DiskWrite(buffer, 0, 1, 1, 0);
    USLOSS_Console("Verify that the disk write was successful.\n");
    assert(rc == P1_SUCCESS);
    USLOSS_Console("Wrote \"%s\".\n", buffer);

    bzero(buffer, sizeof(buffer));
    USLOSS_Console("Read from the disk.\n");
    rc = Sys_DiskRead(buffer, 0, 1, 1, 0);
    USLOSS_Console("Verify that the disk read was successful.\n");
    assert(rc == P1_SUCCESS);
    TEST(strcmp(MSG2, buffer), 0);
    USLOSS_Console("Read \"%s\".\n", buffer);

    bzero(buffer, sizeof(buffer));
    USLOSS_Console("Read from the disk.\n");
    rc = Sys_DiskRead(buffer, 0, 0, 1, 0);
    USLOSS_Console("Verify that the disk read was successful.\n");
    assert(rc == P1_SUCCESS);
    TEST(strcmp(MSG1, buffer), 0);
    USLOSS_Console("Read \"%s\".\n", buffer);
    return 11;
}
int P2_Startup(void *arg)
{
    int rc, waitPid, status, p3Pid;

    P2ClockInit();
    P2DiskInit();
    rc = P2_Spawn("P3_Startup", P3_Startup, NULL, 4*USLOSS_MIN_STACK, 3, &p3Pid);
    TEST(rc, P1_SUCCESS);
    rc = P2_Wait(&waitPid, &status);
    TEST(rc, P1_SUCCESS);
    TEST(waitPid, p3Pid);
    TEST(status, 11);
    P2DiskShutdown();
    P2ClockShutdown();
    USLOSS_Console("You passed all the tests! Treat yourself to a cookie!\n");
    PASSED();
    return 0;
}


void test_setup(int argc, char **argv) {
    int rc;

    rc = Disk_Create(NULL, 0, 10);
    assert(rc == 0);
}

void test_cleanup(int argc, char **argv) {
    DeleteAllDisks();
    if (passed) {
        USLOSS_Console("TEST PASSED.\n");
    }
}
