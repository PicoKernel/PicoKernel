# DRIVERS

## What are drivers?
- Drivers are small pieces of software that let your operating system talk to hardware.
- It decides how to talk to hardware.
- How to configure system.
- Drivers acts as a bridge between Kernal and Hardware.

## What are we building drivers for ?
We are building drivers for our Raspberry Pi pico 2W and ESP32.

## <font color="red">How drivers work ?</font>
1. App make a request
2. Program talks to the driver
3. Driver talks to hardware
4. Interrupt happens
5. Data goes now back up

## <font color="red">Projects which we are going to work on :-</font>
- Bare driver skeleton
- Drivers API Discipline
- UART Driver
- Timer Driver
- Defensive Driver coding
- Driver error Signaling
- Driver Isolation Discipline
- Pico SDK Boundary Management
- Transition to PicoKernal Driver


### List of drivers we are going to build :-
1.Interrupt Controller (NVIC) (KERNEL)
2.DMA controller (minimal interface) (KERNEL)
3.Timer / SysTick driver (KERNEL)
4.Clock / PLL driver (KERNEL)
5.Memory Protection (MPU) (KERNEL – critical for microkernel isolation)




## Team Members :
1.Krishna Raj
2.Ishan Das
