// sample.cc - a sample application program that uses the external pager

#include "vm_app.h"

int main() {
    char* p;
    char* q;
    char* r;
    p = (char*) vm_extend(); // p is an address in the arena
    q = (char*) vm_extend();
    r = (char*) vm_extend();

    p[0] = 'h';
    p[1] = 'e';
    p[2] = 'l';
    p[3] = 'l';
    p[4] = 'o';
    q[0] = 'h';
    r[0] = 'i';
    //vm_syslog(p, 5); // pager logs "hello"
    return 0;
}

