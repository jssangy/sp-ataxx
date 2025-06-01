컴파일 방법
make

또는 

gcc -o client client.c cJSON.c
gcc -o server server.c cJSON.c

실행 방법
창 1
./server 8080

창 2
./client -ip 127.0.0.1 -port 8080 -username Alice

창 3
./client -ip 127.0.0.1 -port 8080 -username Bob