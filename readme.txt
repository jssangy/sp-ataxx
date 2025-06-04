1. 컴파일 방법
cd rpi-led-rgb-matrix
make
cd ..
g++ -o board board.c -DSTANDALONE_BUILD -DHAS_LED_MATRIX -I./rpi-rgb-led-matrix/include -L./rpi-rgb-led-matrix/lib -lrgbmatrix -lrt -lm -lpthread
make

2. 실행 방법 (board)
sudo ./board

3. 실행 방법 (client)
sudo ./client -ip 10.8.128.233 -port 8080 -username TeamShannon