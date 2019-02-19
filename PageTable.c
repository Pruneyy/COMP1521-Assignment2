// PageTable.c ... implementation of Page Table operations
// COMP1521 17s2 Assignment 2
// Written by John Shepherd, September 2017

#include <stdlib.h>
#include <stdio.h>
#include "Memory.h"
#include "Stats.h"
#include "PageTable.h"

// Symbolic constants

#define NOT_USED 0
#define IN_MEMORY 1
#define ON_DISK 2

#define UPDATED 1
#define NOTUPDATED 0

// PTE = Page Table Entry

typedef struct {
   char status;      // NOT_USED, IN_MEMORY, ON_DISK
   char modified;    // boolean: changed since loaded
   int  frame;       // memory frame holding this page 
   int  accessTime;  // clock tick for last access
   int  loadTime;    // clock tick for last time loaded
   int  nPeeks;      // total number times this page read
   int  nPokes;      // total number times this page modified
   int  next;        // linked list next
   int  prev;        // linked list prev
} PTE;

// The virtual address space of the process is managed
//  by an array of Page Table Entries (PTEs)
// The Page Table is not directly accessible outside
//  this file (hence the static declaration)

static PTE *PageTable;              // array of page table entries
static int  nPages;                 // # entries in page table
static int  replacePolicy;          // how to do page replacement
static int  fifoList;               // index of first PTE in FIFO list
static int  fifoLast;               // index of last PTE in FIFO list
static int  first;                  // first page in list
static int  last;                   // last page in list
static int  isupdated = NOTUPDATED; // isupdated check status

// Forward refs for private functions

static int findVictim(int pno, int time);
void updatePageTable(int pno, int fno, int time);
void updateVictimTable(int vno, int time);
void updateList(int pno);

// initPageTable: create/initialise Page Table data structures

void initPageTable(int policy, int np)
{
   PageTable = malloc(np * sizeof(PTE));
   if (PageTable == NULL) {
      fprintf(stderr, "Can't initialise Memory\n");
      exit(EXIT_FAILURE);
   }
   replacePolicy = policy;
   nPages = np;
   fifoList = NONE;
   fifoLast = NONE;
   first = NONE;
   last = NONE;

   for (int i = 0; i < nPages; i++) {
      PTE *p = &PageTable[i];
      p->status = NOT_USED;
      p->modified = 0;
      p->frame = NONE;
      p->accessTime = NONE;
      p->loadTime = NONE;
      p->nPeeks = p->nPokes = 0;
      p->next = NONE;
      p->prev = NONE;
   }
}

// requestPage: request access to page pno in mode
// returns memory frame holding this page
// page may have to be loaded
// PTE(status,modified,frame,accessTime,nextPage,nPeeks,nWrites)

int requestPage(int pno, char mode, int time)
{
   if (pno < 0 || pno >= nPages) {
      fprintf(stderr,"Invalid page reference\n");
      exit(EXIT_FAILURE);
   }
   PTE *p = &PageTable[pno];                                                           
   isupdated = NOTUPDATED;                                                                   
   int fno;                                                                            
   switch (p->status) {
   case NOT_USED:
   case ON_DISK:
      countPageFault();                                                                
      fno = findFreeFrame();
      if (fno == NONE) {
         int vno = findVictim(pno, time);                                             
#ifdef DBUG
         printf("Evict page %d\n",vno);
#endif
         
         PTE *v = &PageTable[vno];           // save frame (if modified)
         fno = v->frame;
         if (v->modified == 1) {
            saveFrame(fno);
         }         
         updateVictimTable(vno, time);
         updateList(pno);
      }
      printf("Page %d given frame %d\n",pno,fno);


      loadFrame(fno,pno,time);               // load pno into fno
      updatePageTable(pno, fno, time); 

      if(isupdated == NOTUPDATED){           // update list (if empty frame)
         if(first == NONE){                  // base case - empty list
            first = last = pno;           
         }                                
         else {                              // every other case
            p->prev = last;
            PTE *originalLast = &PageTable[last];
            originalLast->next = pno;
            last = pno;   
         }
      }

      break;
   case IN_MEMORY:
      countPageHit();                        // page in frame = page hit                
      p->accessTime = time;                  // update access time           

      break;
   default:
      fprintf(stderr,"Invalid page status\n");
      exit(EXIT_FAILURE);
   }
   if (mode == 'r') {
      p->nPeeks++;
   }
   else if (mode == 'w') {
      p->nPokes++;
      p->modified = 1;
   }
   p->accessTime = time;
   return p->frame;
}

// findVictim: find a page to be replaced
// uses the configured replacement policy

static int findVictim(int pno, int time)
{
   int victim = 0;
   switch (replacePolicy) {
   case REPL_LRU:                         // LRU strat
      victim = first;                     // set victim 
      
      break;
   case REPL_FIFO:                        // FIFO strat
      victim = first;                     // set victim
    
      break;
   case REPL_CLOCK:
      return 0;
   }
   return victim;
}

// showPageTableStatus: dump page table
// PTE(status,modified,frame,accessTime,nextPage,nPeeks,nWrites)

void showPageTableStatus(void)
{
   char *s;
   printf("%4s %6s %4s %6s %7s %7s %7s %7s\n",
          "Page","Status","Mod?","Frame","Acc(t)","Load(t)","#Peeks","#Pokes");
   for (int i = 0; i < nPages; i++) {
      PTE *p = &PageTable[i];
      printf("[%02d]", i);
      switch (p->status) {
      case NOT_USED:  s = "-"; break;
      case IN_MEMORY: s = "mem"; break;
      case ON_DISK:   s = "disk"; break;
      }
      printf(" %6s", s);
      printf(" %4s", p->modified ? "yes" : "no");
      if (p->frame == NONE)
         printf(" %6s", "-");
      else
         printf(" %6d", p->frame);
      if (p->accessTime == NONE)
         printf(" %7s", "-");
      else
         printf(" %7d", p->accessTime);
      if (p->loadTime == NONE)
         printf(" %7s", "-");
      else
         printf(" %7d", p->loadTime);
      printf(" %7d", p->nPeeks);
      printf(" %7d", p->nPokes);
      printf("\n");
   }
}

void updatePageTable (int pno, int fno, int time) {
   PTE *p = &PageTable[pno];

   p->status = IN_MEMORY;              // - new status
   p->modified = 0;                    // - not yet modified
   p->frame = fno;                     // - associated with frame fno
   p->loadTime = time;                 // - just loaded
}

void updateVictimTable (int vno, int time) {
   PTE *v = &PageTable[vno];
   
   v->status = ON_DISK;                // - new status
   v->modified = 0;                    // - no longer modified         
   v->frame = NONE;                    // - no frame mapping         
   v->accessTime = NONE;               // - not accessed
   v->loadTime = NONE;                 // - not loaded
}

void updateList (int pno) {
   PTE *front = &PageTable[first];     // update queue
   PTE *back = &PageTable[last];       // update last->next
   PTE *p = &PageTable[pno];           // update last->prev
   first = front->next;
   back->next = pno;
   p->prev = last;
   last = pno;                         // update last
   
   isupdated = UPDATED;                // flag updated list
}
