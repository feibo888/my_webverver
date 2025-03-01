#include <iostream>

#include <unistd.h>
#include "server/webserver.h"

int main()
{

    WebServer server(30001, 3, 60000, false,
                     3306, "root", "123456", "server_data",
                     12, 6, true, 1, 1024);
    server.start();

    return 0;
}

