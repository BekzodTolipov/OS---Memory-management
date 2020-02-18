# Memory Management

To run the program:

	1. Type: "make"
	2. Run executable: "./oss"
	3. For help text type: ./oss -h
	4. Extra options:
		./oss -l : specifies LRU for page replacement (default is FIFO).
		./oss -t : specifies the timer that alarm will go off.

## OSS
Program Description: This program is designed to demonstrate how memory is managed in
this simulated operating system. The OSS will start with creating all necessary shared mem-
mories and initialize. Default program will run FIFO page replacement algorithm when main
memory is full and increment number of page_faults. Program has option to run with LRU page
replacement algorithm by typing option -l at the run time.
  
## USER
Program Description: This program will be executed by child processes created in oss.
It will start attaching to pcb and clock in shared memory.
First process will check if page block is in main memory by checking its valid bit. If
its invlaid than it will proceed to request oss to put it in main memory by incrementing
page_fault.
If its valid than process checks protection bit to see if it can access it to write into
it. If process modified the frame, it will let oss know, so it can modify the dirty bit.
After 1000+-100 references, process will determen if its going to terminate.

## Outcome
At the end of execution you will recieve statistics regarding number of page faults, and how many
times FIFO or LRU ran.
 

