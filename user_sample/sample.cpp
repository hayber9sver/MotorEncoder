
extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
}
#include <iostream>

#define DEVFILE "/dev/devonedev"

typedef struct rotatryinfo{
    int count;
    int64_t time;
}RotatryInfo;


void read_buffer(int fd)
{
    RotatryInfo  buf[10];
    int ret;
    int i;

    ret = read(fd, buf, 10);
    if (ret == -1) {
        std::cerr << "read error" << std::endl;
    }
    for(const auto& data : buf)
    {
        std::cout << "idx :" << data.count << " timestamp:"<< data.time << std::endl; 
    }
    std::cout << std::endl;
}

int main(void)
{
    int ret;
    int fd;

    fd = open(DEVFILE, O_RDONLY);
    if (fd == -1) {
        std::cerr << "open error"  << std::endl;
        return 0;
    }

    // read buffer
    read_buffer(fd);
    close(fd);

    return 0;
}