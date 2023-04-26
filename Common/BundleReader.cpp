
#include "BundleReader.h"

BundleReader::BundleReader()
{
    _buffer = nullptr;
    _position = 0;
    _length = 0;
};

BundleReader::~BundleReader()
{
    
};

void BundleReader::init(byte* buffer, ssize_t length)
{
    _position = 0;
    _buffer  = buffer;
    _length = length;
}

ssize_t BundleReader::read(void* ptr, ssize_t size, ssize_t count)
{
    if (!_buffer || eof())
    {
        return 0;
    }

    ssize_t validCount;
    ssize_t validLength = _length - _position;
    ssize_t needLength = size*count;
    byte* ptr1 = (byte*)ptr;
    if(validLength < needLength)
    {
        validCount = validLength/size;
        ssize_t readLength = size*validCount;
        memcpy(ptr1,(byte*)_buffer+_position,readLength);
        ptr1 += readLength;
        _position += readLength;
        readLength = validLength - readLength;
        if(readLength>0)
        {
            memcpy(ptr1,(byte*)_buffer+_position,readLength);
            _position += readLength;
			validCount += 1;
		}
    }
    else
    {
        memcpy(ptr1,(byte*)_buffer+_position,needLength);
        _position += needLength;
        validCount = count;
    }

    return validCount;
}

byte* BundleReader::readLine(int num, byte* line)
{
    if (!_buffer)
        return nullptr;

    byte* buffer = (byte*)_buffer+_position;
    byte* p = line;
    byte c;
    ssize_t readNum = 0;
    while((c=*buffer) != 10 && readNum < (ssize_t)num && _position < _length)
    {
        *p = c;
        p++;
        buffer++;
        _position++;
        readNum++;
    }
    *p = '\0';

    return line;
}

bool BundleReader::eof()
{
    if (!_buffer)
        return true;
    
    return ((ssize_t)tell()) >= length();
}

ssize_t BundleReader::length()
{
    return _length;
}

ssize_t BundleReader::tell()
{
    if (!_buffer)
        return -1;
    return _position;
}

bool BundleReader::seek(long int offset, int origin)
{
    if (!_buffer)
        return false;

    if(origin == SEEK_CUR)
    {
        _position += offset;
    }
    else if(origin == SEEK_SET)
    {
        _position = offset;
    }
    else if(origin == SEEK_END)
    {
        _position = _length+offset;
    }
    else
        return false;

    return true;
}

bool BundleReader::rewind()
{
    if (_buffer != nullptr)
    {
        _position = 0;
        return true;
    }
    return false;
}

std::string BundleReader::readString()
{
    unsigned int length;
    if(read(&length, 4, 1) != 1)
    {
        return std::string();
    }

    std::string str;
    
    ssize_t validLength = _length - _position;
    if (length > 0 && static_cast<ssize_t>(length) <= validLength)
    {
        str.resize(length);
        if (read(&str[0], 1, length) != length)
        {
            return std::string();
        }
    }
    
    return str;
}

bool BundleReader::readMatrix(float* m)
{
    return (read(m, sizeof(float), 16) == 16);
}
