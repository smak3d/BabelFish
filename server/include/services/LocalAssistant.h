#pragma once

#include <string>
#include <vector>

class LocalAssistant
{

    public:
        LocalAssistant();
        ~LocalAssistant();

        std::string prompt(const std::string& text);

};