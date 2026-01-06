//
// Created by mamia on 2/1/2026.
//

#include <iostream>
#include <ostream>

#include "parser.h"

std::string Value::marshalString() {
    std::string result;
    result.push_back(static_cast<char>(DataType::STRING));
    for (char c : string) {
        result.push_back(c);
    }
    result.push_back('\r');
    result.push_back('\n');

    return result;
}

std::string Value::marshalBulk() {
    std::string result;
    result.push_back(static_cast<char>(DataType::BULK));

    std::string bulk_size = std::to_string(bulk.size());
    for (char c : bulk_size) {
        result.push_back(c);
    }
    result.push_back('\r');
    result.push_back('\n');
    for (char c : bulk) {
        result.push_back(c);
    }
    result.push_back('\r');
    result.push_back('\n');

    return result;
}

std::string Value::marshalNullBulk() {
    std::string result("$-1\r\n");

    return result;
}

std::string Value::marshalInteger() {
    std::string result;

    result.push_back(static_cast<char>(DataType::INTEGER));
    std::string size = std::to_string(integer);
    for (char c : size) {
        result.push_back(c);
    }
    result.push_back('\r');
    result.push_back('\n');

    return result;
}

std::string Value::marshal() {
    switch (type) {
        case DataType::STRING:
            return marshalString();
        case DataType::INTEGER:
            return marshalInteger();
        case DataType::BULK:
            return marshalBulk();
        case DataType::NULLBULK:
            return marshalNullBulk();
        case DataType::ARRAY:
            break;
    }
}