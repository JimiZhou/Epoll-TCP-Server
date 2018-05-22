//
// Created by yujiezhou on 15/05/18.
//
#include "connmgr.h"
#include <stdio.h>

int main(int argc, char **argv) {
    printf("Start listening on port 5678\n");
    connmgr_listen(5678);
}