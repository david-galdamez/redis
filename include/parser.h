//
// Created by mamia on 31/12/2025.
//

#ifndef REDIS_PARSER_H
#define REDIS_PARSER_H

#endif //REDIS_PARSER_H

#include <vector>
#include <string>
#pragma once

enum class DataType : char {
    BULK = '$',
    NULLBULK = NULL,
    STRING = '+',
    ARRAY = '*',
};

struct Value {
    DataType type;
    std::string bulk;
    std::string string;
    std::vector<Value> array;
    std::string marshal();
private:
    std::string marshalString();
    std::string marshalBulk();
    static std::string marshalNullBulk();
    std::string marshalArray();
};

class Reader {
public:
    explicit Reader(std::string buffer);
    Value parseRequest();
private:
    std::string buffer;
    int r_pos = 0;
    char readByte();
    std::string readLine();
    int readInteger();
    Value readArray();
    Value readBulk();
};
