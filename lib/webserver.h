#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <stdbool.h> 

bool webserver_init(void);

void get_offsets(float *temp, float *press, float *hum);

extern float offset_temp, offset_pressao, offset_umidade;
#endif // WEBSERVER_H