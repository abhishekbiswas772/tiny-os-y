#include "../cpu/types.h"

void memory_copy(char *source, char *dest, int nbytes)
{
    int i;
    for (i = 0; i < nbytes; i++)
    {
        *(dest + i) = *(source + i);
    }
}


void memory_set(u8 *dest, u8 val, u32 len) {
    u8 *temp = (u8 *) dest;
    while(len != 0){
        *temp++ = val;
        len--;
    }
}

void int_to_ascii(int n, char str[])
{
    int i = 0;
    int sign = n;
    if (n < 0)
    {
        n = -n;
    }

    do
    {
        str[i++] = (n % 10) + '0';
    } while ((n /= 10) > 0);

    if (sign < 0)
    {
        str[i++] = '-';
    }
    str[i] = '\0';

    int start = 0;
    int end = i - 1;
    while (start < end)
    {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}
