CSE 473 Project 3: Virtual Memory - Paging
------------------------------------------

**(due before 11:59PM on deadline day via Github - NO EXTENSIONS WILL BE GIVEN!)**

* * *

### Please direct all your project-related questions/clarifications to the TAs, either via zoom/piazza/email.

### Academic Integrity Statement:

_The University defines academic integrity as the pursuit of scholarly activity in an open, honest and responsible manner. All students should act with personal integrity, respect other students' dignity, rights and property, and help create and maintain an environment in which all can succeed through the fruits of their efforts (refer to Senate Policy 49-20. Dishonesty of any kind will not be tolerated in this course. Dishonesty includes, but is not limited to, cheating, plagiarizing, fabricating information or citations, facilitating acts of academic dishonesty by others, having unauthorized possession of examinations, submitting work of another person or work previously used without informing the instructor, or tampering with the academic work of other students. Students who are found to be dishonest will receive academic sanctions and will be reported to the University's Office of Student Conduct for possible further disciplinary sanctions (refer to Senate Policy G-9). The Academic Integrity policy for the Department of Computer Science and Engineering for programming projects can be explicitly found at [EECS Academic Integrity Policy](https://www.eecs.psu.edu/students/resources/EECS-CSE-Academic-Integrity.aspx). Please go through it carefully and do ask the instructor for any clarifications if something is not clear._

No exchange of code is permitted between teams. Both giver(s) and taker(s) will be punished harshly with a zero in the Project Grade. Hence, make sure that your code is not visible to anyone outside your team!

### Description

In project 2, you learned about different virtual memory allocation/de-allocation schemes. In project 3, you will learn about the interaction between physical memory and the virtual memory system - the hardware-software roles in implementing virtual memory, and  the software algorithms for managing the virtual pages within a limited physical memory. 

This project requires you to implement  
**(i)** an access control mechanism for the former where the hardware will directly do the translations/accesses for the cases where pages are already in memory, and fault over to the software when the page is not resident; and  
**(ii)** **First-In-First-Out Replacement** and **Third chance Replacement** (a variant of the 2nd chance replacement algorithm discussed in class) to make room for newly incoming pages into physical memory.

### Requirements

#### **You are required to implement the interface function in 473\_mm.c (defined in 473\_mm.h):**

```C
void mm_init(void* vm, int vm_size , int n_frames, int page_size, int policy);
```

This function initializes your memory management system. You can use this call to initialize the data structures and perform other set up operations (signal handlers etc). The following are passed as input to this function:-  

*   `vm`: denotes the pointer to the start of the virtual address space that needs to be managed.
*   `vm_size`: denotes the size of the virtual address space in bytes (starting from \*vm).
*   `n_frames`: denotes the number of physical pages available in your system.
*   `page_size`: denotes the size of both virtual and physical pages.
*   `policy`: can take values 1 or 2 -- 1 indicates FIFO page replacement policy and 2 indicates clock replacement policy.

_Note:_ The virtual memory (vm\*) is already allocated and page aligned when this call is made. Though 'page fault' and 'write-back' in a normal system will involve disk read/write respectively, you DO NOT NEED TO ACTUALLY MAINTAIN SWAP SPACE OR PHYSICAL MEMORY SPACE in your implementation. You only need to keep appropriate data structures to track the required statistics and report them through the mm\_logger() function below.

After mm\_init(), there will be no specific call in our test code to explicitly state that a page is being read/written (load/store), i.e. there are NO function calls for each access to a page. This should be automatically inferred/captured by your code by implementing access protection on the virtual address space (pointed to by vm\*) through (mprotect()) system calls, and the accesses (reads and writes) made by the user program will automatically get trapped by the signal handler that you will have to write to catch the SIGSEGV signals emulating what happens in the paging system.  A useful pointer to how mprotect() operates can be found [here](http://www.opengroup.org/onlinepubs/007908775/xsh/mprotect.html) . The 'mprotect()' function specifies the desired protection-level or access permission for virtual memory pages. A program violating the specified access permission will receive a 'SIGSEGV' signal.

You will need to write a signal handler to catch this 'SIGSEGV' signal and perform appropriate actions required by the specific replacement policy. A useful pointer on how to set signal handlers using 'sigaction()' function can be found [here](http://www.opengroup.org/onlinepubs/007908799/xsh/sigaction.html) . Your SIGSEGV signal handler should emulate  the Virtual Memory system. It should emulate the behavior of a page fault handler. It should also call the following function mm\_logger() (which is provided in project3.c) with the appropriate parameters to log the progress of the execution.

The signal handler that you write to catch the SIGSEGVs before implementing the page replacement, must first determine (i) the address of the access that raised the fault, and (ii) whether the fault raised originated from read operation or a write operation. The function signature specified to sigaction() to handle the signal has three parameters passed to it. The second argument (siginfo\_t\*) and the third argument (ucontext\_t\*) provided to this function point to the information about the signal and context from within the process where the signal was raised respectively. The siginfo\_t structure contains information necessary to get the address where the fault was raised, and the ucontext\_t information contains the dump of the register state when the fault (signal) was raised. Fortunately, x86 architectures export the error information onto the error registers (REG\_ERR) upon a page-fault with specific error codes as defined [here](https://lxr.missinglinkelectronics.com/linux+v3.12/arch/x86/mm/fault.c#L23). Accessing the specific bits within the ucontext\_t structure should help you determine whether the fault was raised due to a read/write operation. Note that this technique is may not work on machines other than x86 architectures running a linux kernel. Hence take specific care to ensure your solution works on the W-204 machines.

**You are required to call the following function in 473\_mm.c (already implemented in project3.c):**

```C
void mm_logger(int virt_page, int fault_type, int evicted_virt_page, int write_back, int phy_addr);
```

*   `virt_page`: This is the page number of the virtual page being referenced.
*   `fault_type`: Indicates what caused the SIGSEGV. Your seg-fault handler should ascertain the reason for taking the fault and return the corresponding code depending on the scenario.  
    *   0 - Read access to a non-present page    
    *   1 - Write access to a non-present page
    *   2 - Write access to a page that is currently Read-only
    *   3 - Track a "read" reference to the page that currently has Read and/or Write permissions.
    *   4 - Track a "write" reference to the page that currently has Read-Write permissions.
    *   **Note**: fault\_type=2 only requires a permission change, while 3 and 4 would not incur a fault in the actual hardware but are just needed in your emulation to set the Reference bit (note that if the Reference bit was already set to one, a signal should NOT have been raised!)
*   `evicted_virt_page`: If this fault evicts a page from physical memory, this represents the virtual page number that is evicted. In case of no page is evicted, you should pass -1.
*   `write_back`: This indicates whether the evicted page needs to be written back to disk. If there is write back it should be 1, otherwise it should be 0.
*   `phy_addr`: This represents the physical address being accessed (frame\_number concatenated with the offset). For e.g. An access on a page in frame number = 0 with an offset of 32 bytes will have the phy\_addr = 0x0020.

### Page Replacement Algorithms

Below is a brief description of the two page replacement algorithm that you will have to implement in your signal handler,  
  
**(i) First-In-First-Out Replacement:**

As the name suggests, this page replacement policy evicts the oldest virtual page (among the currently resident pages in physical memory) brought in to the physical memory. You will have to protect the pages that are NOT in physical memory to catch accesses to these page and record page faults. Initially, all virtual pages will have read/write permissions turned off, so that you catch any access to any of those pages. Once you decide to bring in a page, you will mprotect() it with appropriate permissions based on the type of access. You have to determine this first in the segfault handler (hint: explore the contents of what is passed to the segfault handler). Subsequently, you can give it Read-only permission if it is read access, and Read-and-Write permission if it is a write access. When you evict a virtual page, you will mprotect() it with none, so that any future access to it will raise a SIGSEGV, and you will record it as a page fault.

However, you should minimize the number of SIGSEGVs, i.e. if there is no page fault and/or actions to be done by the handler, you should NOT raise a signal

**(ii) Third chance Replacement:**

In this algorithm, you will maintain a circular linked list, with the head pointer pointing to the next potential candidate page for eviction as discussed in class. You will be maintaining "Reference bit" and "Modified bit" for each page in physical memory. When looking for the next candidate page for eviction, if by any chance, the page pointed by the head pointer has its Reference bit on (set as one), the head pointer resets this bit (sets it to zero), and moves to the next element in the circular list and retries. When the head finds a candidate with 'Reference Bit' set as zero, it becomes a candidate for replacement as per the second chance page replacement algorithm. However, in our "third chance algorithm", you will need to give such a page a third chance if it has been modified (modified bit is set to one) since pages requiring write-back will incur higher overheads for replacement.

A physical page under the clock head could be in one of the following 3 states: (a) R=0, M=0; (b) R=1, M=0; or (c) R=1, M=1; In case (a), the page can be immediately evicted. In case (b), you should give it a second chance, i.e. reset the R bit, and if the next time the clock head comes to that page its bits indicate state (a), then replace it. In case (c), you should give it a third chance, i.e. reset the R bit in the 1st pass, and even in the 2nd pass that the head comes to that page, you should skip it. Caution! You may be tempted to reset the M bit. However, if you do that, note that you will not know whether this page needs to be written back to disk (write\_back parameter) later on at the time of replacement. Only the third time, should it be replaced (and written back). However, note it is possible that between the 2nd pass and the 3rd pass, the R bit could again change to 1 in which case it will again get skipped in the 3rd and 4th pass and would get evicted only in the 5th pass (as long as there is no further reference before then).

**NOTE** that the reference bit is not really accessible in the user program, and the only way for you to emulate this is by taking SIGSEGV signals appropriately. So, for a page in physical memory for which the reference bit is off (zero), you may still require to mprotect() to take a fault on either a load or store to that page. In such a case, a load will turn on the reference bit, and a store will turn on both the reference and modified bits. For a page in physical memory for which the reference bit is on, you would need to mprotect it in Read-Only mode until the first write to it (to update the modified bit). Turning off reference bits would implying doing appropriate mprotect()s. As in FIFO, please note that you should try to minimize the number of SIGSEGVs raised/handled for emulation of these actions.

### Notes: 

*   You are STRONGLY SUGGESTED to first write a stand-alone program which will take a SIGSEGV fault, and you write a signal handler to catch this fault. Additionally, investigate how to determine if the raised fault is from a read/write to a page. Ensure that you  understand how to write signal handlers and leverage mprotect() before you proceed on this project.
*   The test programs will NOT be multithreaded.
*   For the specifics and any latest updates make sure you keep checking canvas for any latest information/updates regarding the project.
*   The test programs (i.e. the user programs which will exercise the memory system you will implement) are available in the Github starter repo provided to you.

### Input File Format:

Each line is: **<_operation_\> <_virt\_page_\> <_start\_offset_\> <_result_\>**

*   `operation`:      **read** (for read operation) **write** (for write operation).
*   `virt_page`:      This is the **page number** of the **virtual page** being referenced.
*   `start_offset`: The **starting offset** within the page being referenced.
*   `result`:           **Value** being written in case of write. **zero** in case of read.

### What to turn in:

1.  **473\_mm.c** (and if needed, its header file), containing the interface functions along with necessary code to implement both page replacement schemes.
    
2.  A report to clarify the assumptions, design choices, the reasons that you made such decisions, and breakdown of the contribution of each member to the project. Also remember to include any special instructions for testing your code.
    

The project source code and report are due before 11:59 through Github. You need to set up an appointment with the TA to demonstrate your implementation and answer a range of questions related to the entire project (even though an individual may have worked on only one part). _Academic dishonesty of any kind will not be tolerated._

### Additional Information, PLEASE READ CAREFULLY!!!

1.  The inputs and sample outputs are given just for illustrative purposes to test your code. You can create more extensive input files for further testing.
2.  The TAs WILL surprise you with several other test inputs during the demo and your routines should still work.
3.  Please stay tuned constantly to the canvas page for the exact and latest interface functions you need to implement, their arguments, test programs, examples, documentation/manuals and announcements.
4.  You can work in teams (at most 2 per team - they could be across sections), or individually, for the project. You are free to choose your partner but if either of you choose to drop out of your team for any reason at any time, each of you will be individually responsible for implementing and demonstrating the entire project and writing the project report by the designated deadline. If you have difficulty finding a partner, give your name to the TAs who will maintain an online list of students without partners to pair you up. Please let the TAs know who you are working with right away. Feel free to change teams from the prior projects.
5.  Even though you will work in pairs, each of you should be fully familiar with the entire code and be prepared to answer a range of questions related to the entire project. It will be a good idea to work together at least during the initial design of the code. You should also ensure that there is a fair division of labor between the members.
6.  All the code-bases will be tested on the W-204 Linux Lab machines located on the second floor of Westgate. Ensure you test and run them in that environment.

[top](#top)

* * *

Submission Guidelines
---------------------

1.  Ensure your Git local environment is configured and setup, and you have signed up for GitHub. (Refer to P0 for detailed steps on this). You should have a valid GitHub account (personal accounts are fine).
2.  Accept the invitation for P3 from this link. This should setup your private repository for this project with the starter code on GitHub. Please note, this is by default a private repository only accessible by you and the course instructors. You can add your team-mates as collaborators (follow P0 for this). _Please ensure this is the ONLY repository that you submit your code to. Any of your code should NOT BE published publically on your own repositories. Contact the TAs incase you have queries regarding this._
3.  Clone your remote (private) repository using the `**$ git clone**` command to get started on the project. (Refer to P0 for the detailed explanation).
4.  Ensure you commit your code often and on time using the `**$ git commit**` and the `**$ git push**` commands (similar to how you had done P0).
5.  The last commit that you do prior to the timeline will be taken as your final submission
6.  The projects (including a **brief** report) is due by 11:59PM on the designated day and the reports+programs. Ensure you push all these into the git repos by then.
7.  NO EXTENSIONS WILL BE ENTERTAINED. You will not be able to submit after the deadline, as the Git repos will not allow you to push code.  
    ENSURE YOU PUSH YOUR CODE TO THE GIT REPOS BEFORE THE DEADLINE.
8.  You need to set up an appointment with your TAs to demonstrate your implementation.
