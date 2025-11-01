/*
 * ctype.c - POSIX ctype.h: 字符分类函数
 */

int isalpha(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isdigit(int c)
{
    return (c >= '0' && c <= '9');
}

int isalnum(int c)
{
    return isalpha(c) || isdigit(c);
}

int isspace(int c)
{
    return (c == ' ' || c == '\t' || c == '\n' || 
            c == '\r' || c == '\v' || c == '\f');
}

int isupper(int c)
{
    return (c >= 'A' && c <= 'Z');
}

int islower(int c)
{
    return (c >= 'a' && c <= 'z');
}

int isprint(int c)
{
    return (c >= 32 && c <= 126);
}

int iscntrl(int c)
{
    return (c >= 0 && c < 32) || c == 127;
}

int isxdigit(int c)
{
    return isdigit(c) || 
           (c >= 'a' && c <= 'f') || 
           (c >= 'A' && c <= 'F');
}

int toupper(int c)
{
    if (islower(c)) {
        return c - 32;
    }
    return c;
}

int tolower(int c)
{
    if (isupper(c)) {
        return c + 32;
    }
    return c;
}


