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
    INTEGER = ':',
    BULK = '$',
    NULLBULK,
    NULLARRAY,
    STRING = '+',
    ARRAY = '*',
};

struct Value {
    DataType type;
    int integer;
    std::string bulk;
    std::string string;
    std::vector<Value> array;
    std::string marshal();
private:
    std::string marshalString();
    std::string marshalBulk();
    std::string marshalNullBulk();
    std::string marshalNullArray();
    std::string marshalInteger();
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
