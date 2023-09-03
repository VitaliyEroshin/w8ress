#pragma once

#include <optional>
#include <string>
#include <vector>

class Logic {
public:
    struct Response {
        enum Type {
            Redirection,
            File,
            HtmlTemplate
        };

        Type type;
        std::string result;
        std::vector<std::string> args;
    };

    virtual Response Get(std::string) {
        return {Response::Type::HtmlTemplate, "/404.html", {}};
    }
};