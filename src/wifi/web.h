#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "../wifi/wifi.h"

#define print(...) printf(__VA_ARGS__)
#define println(...)     \
  do {                   \
    printf(__VA_ARGS__); \
    printf("\n");        \
  } while (0)

void initWebServer();
void endServer();

extern bool isWebServer;

#endif