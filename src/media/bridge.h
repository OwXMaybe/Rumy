#pragma once

#include <Print.h>
#include <stddef.h>
#include <stdint.h>

namespace bridge {

void begin();
void service(uint32_t now);
bool wifi();
int available();
size_t read(uint8_t* buf, size_t n);
int tcpAvailable();
int tcpRead();
void write(const uint8_t* data, size_t n);
void printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void println(const char* s);
Print& out();
bool setKey(const char* hex);
bool hasKey();
uint32_t byteTimeoutMs();
void pace();
void bulk(bool on);
bool bulkActive();
void report(Print& out);

}
