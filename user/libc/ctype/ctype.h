/*
 * ctype.h - POSIX字符分类和转换
 */

#ifndef _CTYPE_H
#define _CTYPE_H

/* 字符分类 */
int isalpha(int c);
int isdigit(int c);
int isalnum(int c);
int isspace(int c);
int isupper(int c);
int islower(int c);
int isprint(int c);
int iscntrl(int c);
int isxdigit(int c);

/* 字符转换 */
int toupper(int c);
int tolower(int c);

#endif /* _CTYPE_H */


