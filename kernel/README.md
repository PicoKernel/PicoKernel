# Kernel Documentation

#Team Members 

- @rootmnt (Team Lead)
- @kartikmishra210

## Definition of a Kernel

A Kernel is the core of an Operating System. It's a program that always runs, controls execution and coordinates everything.

In general, kernels may handle :

- Scheduling
- Memory Management
- Interrups
- System Calls
- Isolation and Protection

------------------------------------------------------------

## Our Implementation of a Kernel

Since our Operating System is meant for Embedded Systems, more specifically Raspberry Pi Pico 2W, Our idea of a kernel would be a little different than a full-fledged kernel implementation. 

Since our Hardware lacks :

- MMU
- Kernel-User Separation
- Virtual Memory

So our kernel would be much simpler in mechanism but stricter in discipline since there are more chances of everything going down with a small problem.


Our implementation features an authoritative design of the PicoKernel kernel. Our kernel's responsibilities are as follows :

- System Initialization 
- Owning the main execution loop
- Routing
- Maintaining Global System State
- Providing Logging APIs
- Handling Errors via Panic (Will be added later on)
- Enforcing rules as to who can call what

We will be implementing :

- Static Memory (No Dynamic Memory Allocation unless required).
- APIs like logging, panic and routing which is to be used by other teams.
- Kernel Panic codes and conditions with a safe halt
- Scheduler
- Registration of modules upon system boot
- Interface and other important program initializations.

Since we are adopting a modular approach here, everything implemented in the kernel must be easily scalable and fixable without changing a whole lot of code. The kernel's internal parts like scheduler, APIs will also be different modules so they are easy to extend as needed in the future.

We will be taking the Cooperative-Scheduling route meaning the tasks would willingly return the control back so it could be handed to the one next in line.

-------------------------------------------------------------

# Project List

We will be creating small projects before actually moving on to the kernel creation which will not be committed on this repository. This repository will only have commits starting from Project 9.

All the learning and how the ideas from the project will be implemented in our actual Operation System, this README will gradually be updated with documentation of different features and probably explanation of how things are coming together. 

- Project 1 : Kernel Loop

	We will be creating a kernel loop that will run indefinitely which will be our base for running the kernel indefinitely.

- Project 2 : Command Routing

	We will create a routing program that will route commands to correct modules, we will ensure that this remains scalable for many commands.

- Project 3 : Global System State

	We will create a Global System State owned by the kernel having different information variables like uptime, number_of_commands etc.

- Project 4 : Cooperative Scheduler

	We will create a Cooperative Scheduler that doesn't have any preemption 

- Project 5 : Time as a Kernel Service

	TBVH I don't get this as well, gotta research and then update this README 

- Project 6 : Kernel Panic

	A Program that handles exceptions and lets the kernel halt the system safely instead of abrupt stops

- Project 7 : Enforcing Layer Boundaries

	Here we define what layers can call who and kernel keeps everything in check, if any discrepancies are found, the kernel immediately halts and outputs the error code so as to prevent future breakages.

- Project 8 : Pico Bring-Up Kernel

	Here we create a simple complete kernel that :
	- Boots
	- Prints logs via UART
	- loops forever
	- panics visibly

- Project 9 : Build Up from Project 8

	This is just improving from Project 8 and adding things like module registration, mediation etc.

--------------------------------------------------------

This concludes our Team's current plans and goals, as we build and learn, we will keep this README updated!
