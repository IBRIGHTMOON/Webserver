#include <unistd.h>
#include "server/webserver.h"

int main()
{
    WebServer server(9999, 3, 60000, false, 3306, "root", "root", "webserver", 12, 6);
    server.Start();
}