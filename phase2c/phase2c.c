#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <usloss.h>
#include <phase1.h>

#include "phase2Int.h"


static int      DiskDriver(void *);
static void     DiskReadStub(USLOSS_Sysargs *sysargs);
static void     DiskWriteStub(USLOSS_Sysargs *sysargs);
static void     DiskSizeStub(USLOSS_Sysargs *sysargs);

typedef struct DiskRequest{
    int first;
    int sectors;
    int track;
    void *buffer;
    USLOSS_DeviceRequest request;
    struct DiskRequest *next;
}DiskRequest;

typedef struct Disk{
    int pid;
    DiskRequest *requestQhead;
}Disk;

static Disk disks[USLOSS_DISK_UNITS];
static int requestSem;

void enQ(int unit, DiskRequest *request){
    if(disks[unit].requestQhead==NULL){
        disks[unit].requestQhead=request;
    }else{
        DiskRequest *tmp;
        tmp = disks[unit].requestQhead;
        while(tmp->next!=NULL){
            tmp=tmp->next;
        }
        tmp->next=request;
    }
}

void deQ(int unit){
    if(disks[unit].requestQhead==NULL){
        return;
    }else{
        DiskRequest *tmp;
        tmp=disks[unit].requestQhead;
        disks[unit].requestQhead=disks[unit].requestQhead->next;
        free(tmp);
    }
}
/*
 * P2DiskInit
 *
 * Initialize the disk data structures and fork the disk drivers.
 */
void 
P2DiskInit(void) 
{
    int rc;
    // initialize data structures here
    for(int i=0;i<USLOSS_DISK_UNITS;i++){
        disks[i].requestQhead=NULL;
    }

    rc = P1_SemCreate("Request_Sem",0,&requestSem);
    // install system call stubs here

    rc = P2_SetSyscallHandler(SYS_DISKREAD, DiskReadStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_DISKWRITE, DiskWriteStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_DISKSIZE, DiskSizeStub);
    assert(rc == P1_SUCCESS);

    // fork the disk drivers here
    rc = P1_Fork("Disk1_Driver", DiskDriver, (void*) 0, USLOSS_MIN_STACK, 2 , 0, &disks[0].pid);
    assert(rc == P1_SUCCESS);
    rc = P1_Fork("Disk2_Driver", DiskDriver, (void*) 1, USLOSS_MIN_STACK, 2 , 0, &disks[1].pid);
    assert(rc == P1_SUCCESS);

}

/*
 * P2DiskShutdown
 *
 * Clean up the disk data structures and stop the disk drivers.
 */

void 
P2DiskShutdown(void) 
{
    int rc;
    int status;
    rc=P1_SemFree(requestSem);
    for(int i =0;i<USLOSS_DISK_UNITS;i++){
        DiskRequest *tmp;
        tmp = disks[i].requestQhead;
        while(disks[i].requestQhead!=NULL){
            tmp=disks[i].requestQhead->next;
            free(disks[i].requestQhead);
            disks[i].requestQhead=tmp;
        }
    }
    rc=USLOSS_DeviceInput(USLOSS_DISK_DEV, 0,&status);
    rc=P1_WakeupDevice(USLOSS_DISK_DEV, 0,status,TRUE);
    rc=USLOSS_DeviceInput(USLOSS_DISK_DEV, 1,&status);
    rc=P1_WakeupDevice(USLOSS_DISK_DEV, 1,status,TRUE);
}

/*
 * DiskDriver
 *
 * Kernel process that manages a disk device and services disk I/O requests from other processes.
 * Note that it may require several disk operations to service a single I/O request.
 */
static int 
DiskDriver(void *arg) 
{
    int unit = (int) arg;
    // repeat
    //   wait for next request
    //   while request isn't complete
    //          send appropriate operation to disk (USLOSS_DeviceOutput)
    //          wait for operation to finish (P1_WaitDevice)
    //          handle errors
    //   update the request status and wake the waiting process
    // until P2DiskShutdown has been called
    while(1){
        int rc;
        int status;
        rc = P1_WaitDevice(USLOSS_DISK_DEV, unit, &status);
        if (rc == P1_WAIT_ABORTED) {
            break;
        }
        if(disks[unit].requestQhead!=NULL){
            DiskRequest *tmp=disks[unit].requestQhead;
            for (int i = 0; i < tmp->sectors; i++){
                int index = (tmp->first+i)%USLOSS_DISK_SECTOR_SIZE;
                tmp->request.reg1 =(void *) index;
                tmp->request.reg2 = tmp->buffer+USLOSS_DISK_SECTOR_SIZE*i;
                rc=USLOSS_DeviceOutput(USLOSS_DISK_DEV,unit,&tmp->request);
                rc=P1_WaitDevice(USLOSS_DISK_DEV, unit, &status);
            }
            deQ(unit);
            rc = P1_V(requestSem);
        }
        
    }
    return P1_SUCCESS;
}

/*
 * P2_DiskRead
 *
 * Reads the specified number of sectors from the disk starting at the specified track and sector.
 */
int 
P2_DiskRead(int unit, int track, int first, int sectors, void *buffer) 
{
    int rc;
    int status;
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0){
        USLOSS_IllegalInstruction();
    }
    if(unit!=0&&unit!=1){
        return P1_INVALID_UNIT;
    }
    if(track<0||track>=USLOSS_DISK_TRACK_SIZE){
        return P2_INVALID_TRACK;
    }
    if(first<0||first>=USLOSS_DISK_SECTOR_SIZE){
        return P2_INVALID_FIRST;
    }
    if(sectors<0||(first+sectors)/USLOSS_DISK_SECTOR_SIZE+track>=USLOSS_DISK_TRACK_SIZE){
        return P2_INVALID_SECTORS;
    }
    if(buffer==NULL){
        return P2_NULL_ADDRESS;
    }
    // give request to the proper device driver
    DiskRequest *diskRequest=malloc(sizeof(DiskRequest));
    diskRequest->request.opr = USLOSS_DISK_READ;
    diskRequest->first=first;
    diskRequest->track=track;
    diskRequest->sectors=sectors;
    diskRequest->buffer=buffer;
    diskRequest->next=NULL;
    enQ(unit,diskRequest);
    rc=USLOSS_DeviceInput(USLOSS_DISK_DEV, 0,&status);
    rc=P1_WakeupDevice(USLOSS_DISK_DEV, 0,status,FALSE);
    // wait until device driver completes the request
    rc = P1_P(requestSem);
    return P1_SUCCESS;
}

int 
P2_DiskWrite(int unit, int track, int first, int sectors, void *buffer) 
{
    int rc;
    int status;
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0){
        USLOSS_IllegalInstruction();
    }
    if(unit!=0&&unit!=1){
        return P1_INVALID_UNIT;
    }
    if(track<0||track>=USLOSS_DISK_TRACK_SIZE){
        return P2_INVALID_TRACK;
    }
    if(first<0||first>=USLOSS_DISK_SECTOR_SIZE){
        return P2_INVALID_FIRST;
    }
    if(sectors<0||(first+sectors)/USLOSS_DISK_SECTOR_SIZE+track>=USLOSS_DISK_TRACK_SIZE){
        return P2_INVALID_SECTORS;
    }
    if(buffer==NULL){
        return P2_NULL_ADDRESS;
    }
    // give request to the proper device driver
    DiskRequest *diskRequest=malloc(sizeof(DiskRequest));
    diskRequest->request.opr = USLOSS_DISK_WRITE;
    diskRequest->first=first;
    diskRequest->track=track;
    diskRequest->sectors=sectors;
    diskRequest->buffer=buffer;
    diskRequest->next=NULL;
    enQ(unit,diskRequest);
    rc=USLOSS_DeviceInput(USLOSS_DISK_DEV, 0,&status);
    rc=P1_WakeupDevice(USLOSS_DISK_DEV, 0,status,FALSE);
    // wait until device driver completes the request
    rc = P1_P(requestSem);
    return P1_SUCCESS;
}

int 
P2_DiskSize(int unit, int *sector, int *track,int *disk) 
{
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0){
        USLOSS_IllegalInstruction();
    }
    if(unit!=0&&unit!=1){
        return P1_INVALID_UNIT;
    }
    if(sector==NULL||track==NULL||disk==NULL){
        return P2_NULL_ADDRESS;
    }

    return P1_SUCCESS;
}

/*
 * DiskReadStub
 *
 * Stub for the Sys_DiskRead system call.
 */
static void 
DiskReadStub(USLOSS_Sysargs *sysargs) 
{
    // unpack sysargs
    void *buffer =(void*) sysargs->arg1;
    int sectors = (int) sysargs->arg2;
    int track = (int) sysargs->arg3;
    int first = (int) sysargs->arg4;
    int unit = (int) sysargs->arg5;
    // call P2_DiskRead
    int rc = P2_DiskRead(unit,track,first,sectors,buffer);
    // put result in sysargs
    sysargs->arg4=(void*) rc;
}

static void
DiskWriteStub(USLOSS_Sysargs *sysargs)
{
    // unpack sysargs
    void *buffer =(void*) sysargs->arg1;
    int sectors = (int) sysargs->arg2;
    int track = (int) sysargs->arg3;
    int first = (int) sysargs->arg4;
    int unit = (int) sysargs->arg5;
    // call P2_DiskWrite
    int rc = P2_DiskWrite(unit,track,first,sectors,buffer);
    // put result in sysargs
    sysargs->arg4=(void*) rc;
}

static void
DiskSizeStub(USLOSS_Sysargs *sysargs)
{
    int unit = (int) sysargs->arg1;
    int sector;
    int track;
    int disk;
    int rc;
    rc = P2_DiskSize(unit,&sector,&track,&disk);
    sysargs->arg1 =(void*) sector;
    sysargs->arg2 =(void*) track;
    sysargs->arg3 =(void*) disk;
    sysargs->arg4 =(void*) rc;
}

