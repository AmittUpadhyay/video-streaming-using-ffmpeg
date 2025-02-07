#pragma once
#ifndef OBSERVER_HPP
#define OBSERVER_HPP

class Observer
{
public:
    virtual void update(unsigned char* data, uint32_t width, uint32_t height) = 0;
    virtual ~Observer() = default;

};


#endif // OBSERVER_HPP
