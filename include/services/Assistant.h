#pragma once

#include <string>
#include <vector>

class Assistant
{

    public:
        Assistant();
        ~Assistant();

        std::string prompt(const std::string& text);

};