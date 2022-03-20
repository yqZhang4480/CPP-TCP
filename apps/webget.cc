#include "socket.hh"
#include "util.hh"

#include <cstdlib>
#include <iostream>

using namespace std;

void get_URL(const string &host, const string &path) {
    // Your code here.
    TCPSocket socket;
    Address address(host,"http"); 
    socket.connect(address);
    socket.write("\r\n");
    socket.write("GET ");
    socket.write(path);
    socket.write(" HTTP/1.1\r\n");
    socket.write("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/99.0.4844.51 Safari/537.36 Edg/99.0.1150.30\r\n");
		socket.write("Host: cs144.keithw.org\r\n");
    socket.write("Connection: close\r\n\r\n");
		socket.write("\r\n");
    while(true){
        auto str = socket.read();
        if(socket.eof()){
            socket.close();
            break;
        }
        cout<<str;
    }
     
		
		
    // You will need to connect to the "http" service on
    // the computer whose name is in the "host" string,
    // then request the URL path given in the "path" string.

    // Then you'll need to print out everything the server sends back,
    // (not just one call to read() -- everything) until you reach
    // the "eof" (end of file).

    TCPSocket socket_connect;
    socket_connect.connect(Address(host, "http"));

    socket_connect.write("\r\n");
    socket_connect.write("GET ");
    socket_connect.write(path);
    socket_connect.write(" HTTP/1.1\r\n");
    socket_connect.write("Host: cs144.keithw.org\r\n");
    socket_connect.write("Connection: close\r\n\r\n");
    socket_connect.write("\r\n");

    while (true)
    {
        auto str = socket_connect.read();
        if (socket_connect.eof())
        {
            socket_connect.close();
            break;
        }
        cout << str;
    }
}

int main(int argc, char *argv[]) {
    try {
        if (argc <= 0) {
            abort();  // For sticklers: don't try to access argv[0] if argc <= 0.
        }

        // The program takes two command-line arguments: the hostname and "path" part of the URL.
        // Print the usage message unless there are these two arguments (plus the program name
        // itself, so arg count = 3 in total).
        if (argc != 3) {
            cerr << "Usage: " << argv[0] << " HOST PATH\n";
            cerr << "\tExample: " << argv[0] << " stanford.edu /class/cs144\n";
            return EXIT_FAILURE;
        }

        // Get the command-line arguments.
        const string host = argv[1];
        const string path = argv[2];

        // Call the student-written function.
        get_URL(host, path);
    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
