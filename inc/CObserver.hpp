#pragma once
#ifndef OBSERVER_HPP
#define OBSERVER_HPP

//#define _WINSOCK_DEPRECATED_NO_WARNINGS
//#define _WIN32_WINNT 0x0601
class Observer
{
public:
    virtual void update(unsigned char* data, uint32_t width, uint32_t height) = 0;
    virtual ~Observer() = default;

};


#endif // OBSERVER_HPP
