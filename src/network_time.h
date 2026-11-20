#pragma once

#include <stdbool.h>
#include <time.h>

void network_time_start(void);
bool network_time_local(struct tm *result);
