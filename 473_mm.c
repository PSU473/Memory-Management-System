// Starter code for the page replacement project
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <signal.h>
#include "473_mm.h"
#include <ucontext.h>

//Global Variables

#define FIFO    1
#define THIRD_CHANCE  2

struct sigaction sa;

void mySignalHandler();

void *myStart;
int myVMSize;
int myNumFrames;
int myPageSize;
int myPolicy;
int evict_frame = 0;
int myMaxPages;

typedef struct {
    int frameNum;
    int page;
    int ref;
    int modified;
    int permissions;
    int chance;
    int phys_addr;
} frame;

frame* frames = NULL;

void mm_init(void* vm, int vm_size, int n_frames, int page_size, int policy){
    myStart = vm;
    myVMSize = vm_size;
    myNumFrames = n_frames;
    myPageSize =page_size;
    myPolicy = policy;

    frames = malloc(n_frames*sizeof(frame));

    for(int i=0; i < n_frames; i++) {
        frames[i].frameNum = i;
        frames[i].ref = 0;
        frames[i].page = -1;
        frames[i].chance = 0;
    }

    int max_pages = (myVMSize/myPageSize);
    myMaxPages = max_pages;

    sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = &mySignalHandler;

    sigaction(SIGSEGV, &sa, NULL);

    for(int i = 0; i < max_pages; i++) {
        mprotect(myStart + (i*myPageSize), myPageSize, PROT_NONE);
    }
}


void mySignalHandler(int signo, siginfo_t *info, void *context) {
    int vmOffset = info->si_addr - myStart;
    int pageNum = vmOffset / myPageSize;
    int byteOffset = vmOffset - (pageNum * myPageSize);

    ucontext_t *new_context = (ucontext_t *) context;

    int readWrite;
    int faultType;
    int evictedPage = -1;
    int phys_addr = 0;
    int evict = 0;
    int writeBack = 0;
    
    if (new_context->uc_mcontext.gregs[REG_ERR] & 0x2) {
        readWrite = PROT_WRITE;
    }
    else {
        readWrite = PROT_READ;
    }

    if (myPolicy == FIFO) {

        if (evict_frame > myNumFrames - 1)
            evict_frame = 0;

        // Go through frames, if the frame hasn't been referenced, place it there
        for (int i = 0; i < myNumFrames; i++) {
            //If there is a hit
            if (frames[i].page == pageNum && frames[i].permissions == readWrite) {
                //Do Nothing
                break;
            }
            //Fault type 0, No eviction
            if (frames[i].ref == 0 && readWrite == PROT_READ) {
                frames[i].page = pageNum;
                frames[i].ref = 1;
                frames[i].modified = 0;
                frames[i].permissions = readWrite;
                frames[i].phys_addr = byteOffset + (i*myPageSize);
                evictedPage = -1;
                faultType = 0;
                mprotect(myStart + (myPageSize * pageNum), myPageSize, readWrite);
                mm_logger(frames[i].page, faultType, evictedPage, writeBack, frames[i].phys_addr);
                break;
            }
            //Fault type 1, No eviction
            if (frames[i].ref == 0 && readWrite == PROT_WRITE) {
                frames[i].page = pageNum;
                frames[i].ref = 1;
                frames[i].modified = 1;
                frames[i].permissions = readWrite;
                frames[i].phys_addr = byteOffset + (i*myPageSize);
                evictedPage = -1;
                faultType = 1;
                mprotect(myStart + (myPageSize * pageNum), myPageSize, readWrite);
                mm_logger(frames[i].page, faultType, evictedPage, writeBack, frames[i].phys_addr);
                break;
            }
            //Fault type 2
            if (frames[i].page == pageNum && frames[i].permissions != readWrite && readWrite == PROT_WRITE) {
                faultType = 2;
                frames[i].modified = 1;
                frames[i].permissions = readWrite;
                frames[i].phys_addr = byteOffset + (i*myPageSize);
                evictedPage = -1;
                mprotect(myStart + (myPageSize * pageNum), myPageSize, readWrite);
                mm_logger(frames[i].page, faultType, evictedPage, writeBack, frames[i].phys_addr);
                break;
            }

            //Eviction needed
            if (i == myNumFrames -1) {
                evict = 1;
            }           
        }

        //Eviction
        if (evict == 1) {

            //Writeback
            if (frames[evict_frame].modified == 1) {
                writeBack = 1;
            }

            //Evict Page
            evictedPage = frames[evict_frame].page;
            mprotect(myStart + (myPageSize * frames[evict_frame].page), myPageSize, PROT_NONE);

            //Fault type 0, Eviction
            if (readWrite == PROT_WRITE) {
                faultType = 1;
            }
            //Fault type 1, Eviction
            if (readWrite == PROT_READ) {
                faultType = 0;
            }

            frames[evict_frame].chance = 0;
            frames[evict_frame].ref = 1;
            frames[evict_frame].page = pageNum;
            frames[evict_frame].modified = 0;
            frames[evict_frame].permissions = readWrite;
            frames[evict_frame].phys_addr = byteOffset + (evict_frame*myPageSize);
            mprotect(myStart + (myPageSize * pageNum), myPageSize, readWrite);
            mm_logger(frames[evict_frame].page, faultType, evictedPage, writeBack, frames[evict_frame].phys_addr);
            evict_frame++;
        }
    }

    if (myPolicy == THIRD_CHANCE) {
        // Go through frames, if the frame hasn't been referenced, place it there
        for (int i = 0; i < myNumFrames; i++) {

            //Fault type 0, No eviction
            if (frames[i].page == -1 && readWrite == PROT_READ) {
                frames[i].page = pageNum;
                frames[i].ref = 1;
                frames[i].modified = 0;
                frames[i].permissions = readWrite;
                frames[i].phys_addr = byteOffset + (i*myPageSize);
                evictedPage = -1;
                faultType = 0;
                mprotect(myStart + (myPageSize * pageNum), myPageSize, readWrite);
                mm_logger(frames[i].page, faultType, evictedPage, writeBack, frames[i].phys_addr);
                break;
            }
            //Fault type 1, No eviction
            if (frames[i].page == -1 && readWrite == PROT_WRITE) {
                frames[i].page = pageNum;
                frames[i].ref = 1;
                frames[i].modified = 1;
                frames[i].permissions = readWrite;
                frames[i].phys_addr = byteOffset + (i*myPageSize);
                evictedPage = -1;
                faultType = 1;
                mprotect(myStart + (myPageSize * pageNum), myPageSize, readWrite);
                mm_logger(frames[i].page, faultType, evictedPage, writeBack, frames[i].phys_addr);
                break;
            }
            //Fault type 2
            if (frames[i].page == pageNum && frames[i].permissions != readWrite && readWrite == PROT_WRITE) {
                faultType = 2;
                frames[i].modified = 1;
                frames[i].permissions = readWrite;
                frames[i].chance = 0;
                frames[i].ref = 1;
                frames[i].phys_addr = byteOffset + (i*myPageSize);
                evictedPage = -1;
                mprotect(myStart + (myPageSize * pageNum), myPageSize, readWrite);
                mm_logger(frames[i].page, faultType, evictedPage, writeBack, frames[i].phys_addr);
                break;
            }

            //Fault type 3
            if(readWrite == PROT_READ && frames[i].page == pageNum && (frames[i].permissions == PROT_READ || frames[i].permissions == PROT_WRITE)) {
                faultType = 3;
                evictedPage = -1;
                frames[i].ref = 1;
                frames[i].chance = 0;
                frames[i].phys_addr = byteOffset + (i*myPageSize);
                mprotect(myStart + (myPageSize * pageNum), myPageSize, readWrite);
                mm_logger(frames[i].page, faultType, evictedPage, writeBack, frames[i].phys_addr);
                break;
            }

            //Fault type 4
            if(readWrite == PROT_WRITE && frames[i].page == pageNum && frames[i].permissions == PROT_WRITE) {
                faultType = 4;
                evictedPage = -1;
                frames[i].ref = 1;
                frames[i].chance = 0;
                frames[i].phys_addr = byteOffset + (i*myPageSize);
                mprotect(myStart + (myPageSize * pageNum), myPageSize, readWrite);
                mm_logger(frames[i].page, faultType, evictedPage, writeBack, frames[i].phys_addr);
                break;
            }

            //Eviction needed
            if (i == myNumFrames - 1) {
                evict = 1;
            }           
        }

        //Eviction
        if (evict == 1) {

            while (1) {
                if (evict_frame > myNumFrames - 1) {
                    evict_frame = 0;
                }
                if (((frames[evict_frame].permissions == PROT_READ && frames[evict_frame].chance >= 1) || (frames[evict_frame].permissions == PROT_WRITE && frames[evict_frame].chance >= 2)) && frames[evict_frame].ref == 0) {
                    break;
                }
                else {
                    frames[evict_frame].chance++;
                    frames[evict_frame].ref = 0;
                    mprotect(myStart + (myPageSize * frames[evict_frame].page), myPageSize, PROT_NONE);
                    evict_frame++;
                }
            }

            //Writeback
            if (frames[evict_frame].modified == 1) {
                writeBack = 1;
            }

            //Evict Page
            evictedPage = frames[evict_frame].page;
            mprotect(myStart + (myPageSize * frames[evict_frame].page), myPageSize, PROT_NONE);

            //Fault type 0, Eviction
            if (readWrite == PROT_WRITE) {
                frames[evict_frame].modified = 1;
                faultType = 1;
            }

            //Fault type 1, Eviction
            if (readWrite == PROT_READ) {
                frames[evict_frame].modified = 0;
                faultType = 0;
            }
            
            //Update Page
            frames[evict_frame].page = pageNum;
            frames[evict_frame].ref = 1;
            frames[evict_frame].permissions = readWrite;
            frames[evict_frame].phys_addr = byteOffset + (evict_frame*myPageSize);
            mprotect(myStart + (myPageSize * pageNum), myPageSize, readWrite);
            mm_logger(frames[evict_frame].page, faultType, evictedPage, writeBack, frames[evict_frame].phys_addr);
            evict_frame++;
        }
    }
}

