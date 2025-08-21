# CTMP Proxy

My solution to the CoreTech Wirestorm challenge. The solution and tests for part 1 of the challenge can be found in branch 'part_1'.

## Features
The program:
- Allows a single source client to connect on port 33333
- Allows multiple destination clients to connect on port 44444
- Validates messages sent by the source client to check they are of the format specificed by the CTMP
- Broadcasts valid messages to all the destination clients

## Building and Verifying
1. Open a terminal window
2. Clone the repository: ```git clone https://github.com/yellow-flamingo/wirestorm-challenge.git```
3. ```cd``` into the repository
4. Compile the server code: ```gcc -o server server.c```
5. Run the server: ```./server```
6. In a separate terminal window, run the tests: ```python3 tests.py```
7. If all the tests pass, a success message will be shown

To build the part 1 solution and run those tests:
1. Checkout to branch 'part_1': ```git checkout part_1```
2. Compile and run the server and tests file as explained above

Note: The code in chapter 7.2 of Beej's Guide To Network Programming (which can be found here: https://beej.us/guide/bgnet/pdf/bgnet_a4_c_1.pdf), was used as a starting point and then adapted.
