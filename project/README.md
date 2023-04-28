The server listens for incoming connections on a specified port number (8080 in this case) using a TCP socket. Once a client establishes a connection, the server reads the HTTP request from the client, processes it, and sends back an appropriate HTTP response.

The main loop of the server accepts incoming connections using the accept() function, which blocks until a new client connects. Once a new client connects, the server creates a new socket for that client, reads the HTTP request using the recv() function, processes the request using the process_request() function, and sends the HTTP response using the send() function.

The process_request() function extracts the HTTP method, path, and protocol from the request using the sscanf() function. If the HTTP method is not GET, it sends a 404 error response. Otherwise, it extracts the file extension from the path and determines the appropriate content type for the response based on the file extension. If the file does not exist or cannot be opened, it sends a 404 error response. Otherwise, it sends the contents of the file as the response.

The send_file() function reads the contents of the file in chunks using fread() and sends each chunk using send(). It also constructs the HTTP response header with the appropriate content type and content length.

The main problem I encountered while working on this project was learning to use sockets from scratch. Initially, I had trouble figuring out which API to use and how to approach the problem. Additionally, as I am not familiar with coding in C, I had to search constantly for the correct functions and syntax to use. This led to a lot of trial and error and slowed down my progress.

One specific challenge I faced was dealing with "bind: Address already in use" error. I searched for the solution to this error and found one website suggested me to enable the SO_REUSEADDR socket option. I also tried to manually kill the server process.

I was able to finish this project based on multiple online tutorials and community posts like Stack Overflow, GeeksforGeeks and Medium.