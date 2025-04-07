#pragma once

struct Vec2
{
    float x;
    float y;
};

struct Rect
{
    union
    {
        struct
        {
            Vec2 pos;
            Vec2 size;
        };
        
        struct
        {
            float x;
            float y;
            float w;
            float h;
        };
    };
};
