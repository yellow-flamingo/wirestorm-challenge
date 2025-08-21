# CTMP Proxy

My solution to the CoreTech Wirestorm challenge. The solution and tests for part 1 of the challenge can be found in branch 'part_1'.

## CoreTech Message Protocol (CTMP)
CTMP is a custom messaging protocol designed to encapsulate large data transfers over TCP.
The header of basic messages (handled by the server in branch 'part_1') must have the following form:
    0               1               2               3
    0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    | MAGIC 0xCC    | PADDING       | LENGTH                      |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    | PADDING                                                     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    | DATA ...................................................... |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7

The header of advanced messages (handled by the server in the 'main' branch) must have the following form:
    0               1               2               3
    0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    | MAGIC 0xCC    | OPTIONS       | LENGTH                      |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    | CHECKSUM                      | PADDING                     |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    | DATA ...................................................... |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7

## Features
The proxy:
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

## Usage
You can make a connection to the relevant ports using netcat and send some data to see the proxy in action:
- Connect a source client to port 33333 on localhost: ```nc localhost 33333```
- Connect a destination client to port 44444 on localhost ```nc localhost 44444```

Note: The code in chapter 7.2 of Beej's Guide To Network Programming (which can be found here: https://beej.us/guide/bgnet/pdf/bgnet_a4_c_1.pdf), was used as a starting point and then adapted.
