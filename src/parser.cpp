//
// Created by mamia on 31/12/2025.
//

#include "parser.h"

#include <cstring>
#include <iostream>
#include <utility>

Reader::Reader(std::string buffer) : buffer(std::move(buffer)) {
}

char Reader::readByte() {
    char c = buffer[r_pos];
    r_pos += 1;

    return c;
}

std::string Reader::readLine() {
    std::string result;
    while (true) {
        char c = readByte();
        result.push_back(c);

        if (result.size() >= 2 && result[result.size() - 2] == '\r' && result[result.size() - 1] == '\n') {
            break;
        }
    }

    return result.substr(0, result.size() - 2);
}

int Reader::readInteger() {
    std::string n = readLine();

    int size = std::stoi(n);

    return size;
}

Value Reader::readBulk() {
    Value value{};
    value.type = DataType::BULK;

    const int bulk_size = readInteger();

    value.bulk.resize(bulk_size);
    for (int i = 0; i < bulk_size; i++) {
        value.bulk[i] = readByte();
    }

    readLine();

    return value;
}

Value Reader::readArray() {
    Value value{};
    value.type = DataType::ARRAY;

    int array_size = readInteger();

    for (int i = 0; i < array_size; i++) {
        const Value new_val = parseRequest();

        value.array.push_back(new_val);
    }

    return value;
}

Value Reader::parseRequest() {
    switch (char type = readByte()) {
        case static_cast<char>(DataType::BULK):
            return readBulk();
        case static_cast<char>(DataType::ARRAY):
            return readArray();
        default:
            std::cerr << "Unknown type\n";
            return Value{};
    }
}
