#pragma once

struct RespawnLine {
    int x1, y1, x2, y2;
    bool isActive;
    RespawnLine() : x1(0), y1(0), x2(0), y2(0), isActive(true) {}
    RespawnLine(int a,int b,int c,int d) : x1(a), y1(b), x2(c), y2(d), isActive(true) {}
};
