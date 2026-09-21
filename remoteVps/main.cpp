#include "GlobalHeaders.h"
#include "server.h"

int main() {
    initlog();
    server server;
    server.start();

    return 0;
}