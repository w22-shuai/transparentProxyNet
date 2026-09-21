#include "GlobalHeaders.h"
#include "server.h"


int main() {
    initlog();

    server Server;
    Server.start();
    return 0;
}
