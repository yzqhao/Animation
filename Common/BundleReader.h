
#pragma once

#include "../System/Macros.h"

#include <BaseTsd.h>
typedef SSIZE_T ssize_t;

class BundleReader
{
public:

    BundleReader();
    ~BundleReader();

    void init(byte* buffer, ssize_t length);

    ssize_t read(void* ptr, ssize_t size, ssize_t count);

    byte* readLine(int num, byte* line);

    bool eof();

    ssize_t length();

    ssize_t tell();

    bool seek(long int offset, int origin);

    bool rewind();

    template<typename T> bool read(T* ptr);
    template<typename T> bool readArray(unsigned int* length, std::vector<T>* values);

    std::string readString();

    bool readMatrix(float* m);

private:
    ssize_t _position;
    ssize_t  _length;
    byte* _buffer;
};


template<typename T>
inline bool BundleReader::read(T *ptr)
{
    return (read(ptr, sizeof(T), 1) == 1);
}


template<typename T>
inline bool BundleReader::readArray(unsigned int *length, std::vector<T> *values)
{
    if (!read(length))
    {
        return false;
    }
    
    if (*length > 0 && values)
    {
        values->resize(*length);
        if (read(&(*values)[0], sizeof(T), *length) != *length)
        {
            return false;
        }
    }
    return true;
}

template<>
inline bool BundleReader::read<byte>(byte*ptr)
{
    if (read(ptr, sizeof(byte), 1) == 1)
    {
        return true;
    }
    else
    {
        *ptr = -1;
        return false;
    }
}

template<>
inline bool BundleReader::read<std::string>(std::string* /*ptr*/)
{
    return false;
}

/**
* template function to read array of value.
*/
template<>
inline bool BundleReader::readArray<std::string>(unsigned int *length, std::vector<std::string> *values)
{
    if (!read(length))
    {
        return false;
    }
    values->clear();
    if (*length > 0 && values)
    {
        for (int i = 0; i < (int)*length; ++i)
        {
            values->push_back(readString());
        }
    }
    return true;
}

