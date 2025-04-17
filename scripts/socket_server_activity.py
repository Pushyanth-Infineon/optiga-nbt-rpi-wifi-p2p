
import socket

# Define the server host and port
HOST = '192.168.4.1'  # Host name
PORT = 5005            # Port to listen on

def receive_file():
    # Create a socket object
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        # Bind the socket to the host and port
        s.bind((HOST, PORT))
        print(f"Server started at {HOST} on port {PORT}")

        # Listen for incoming connections
        s.listen()
        print("Waiting for a connection...")

        # Accept a connection from a client
        conn, addr = s.accept()
        with conn:
            print(f"Connected by {addr}")

            # Open a file to write the received data
            with open('received_txt.txt', 'wb') as f:
                while True:
                    # Receive data from the client
                    data = conn.recv(1024)
                    if not data:
                        break
                    # Write the received data to the file
                    f.write(data)
            print("File received successfully")
            print("Open received_txt file to view the data received")

if __name__ == "__main__":
    receive_file()



    
