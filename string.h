#ifndef _STRING_H_
#define _STRING_H_

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

extern char * strerror(int errno);
extern char * ___strtok;

static inline char *strcpy(char *dest, const char *src)
{
	char *d = dest;
	while ((*d++ = *src++));
	return dest;
}

static inline char *strncpy(char *dest, const char *src, int count)
{
	char *d = dest;
	while (count-- > 0) {
		*d++ = *src;
		if (*src) src++;
	}
	return dest;
}

static inline char *strcat(char *dest, const char *src)
{
	char *d = dest;
	while (*d) d++;
	while ((*d++ = *src++));
	return dest;
}

static inline char *strncat(char *dest, const char *src, int count)
{
	char *d = dest;
	while (*d) d++;
	while (count-- > 0 && *src)
		*d++ = *src++;
	*d = '\0';
	return dest;
}

static inline int strcmp(const char *cs, const char *ct)
{
	while (*cs && *cs == *ct) { cs++; ct++; }
	return (unsigned char)*cs - (unsigned char)*ct;
}

static inline int strncmp(const char *cs, const char *ct, int count)
{
	while (count-- > 0) {
		if (*cs != *ct)
			return (unsigned char)*cs - (unsigned char)*ct;
		if (!*cs) return 0;
		cs++; ct++;
	}
	return 0;
}

static inline char *strchr(const char *s, char c)
{
	while (*s) {
		if (*s == c) return (char *)s;
		s++;
	}
	return NULL;
}

static inline char *strrchr(const char *s, char c)
{
	const char *last = NULL;
	while (*s) {
		if (*s == c) last = s;
		s++;
	}
	return (char *)last;
}

static inline int strspn(const char *cs, const char *ct)
{
	int count = 0;
	while (*cs) {
		const char *p = ct;
		while (*p && *p != *cs) p++;
		if (!*p) break;
		count++; cs++;
	}
	return count;
}

static inline int strcspn(const char *cs, const char *ct)
{
	int count = 0;
	while (*cs) {
		const char *p = ct;
		while (*p && *p != *cs) p++;
		if (*p) break;
		count++; cs++;
	}
	return count;
}

static inline char *strpbrk(const char *cs, const char *ct)
{
	while (*cs) {
		const char *p = ct;
		while (*p) {
			if (*p == *cs) return (char *)cs;
			p++;
		}
		cs++;
	}
	return NULL;
}

static inline char *strstr(const char *cs, const char *ct)
{
	if (!*ct) return (char *)cs;
	while (*cs) {
		const char *a = cs, *b = ct;
		while (*a && *b && *a == *b) { a++; b++; }
		if (!*b) return (char *)cs;
		cs++;
	}
	return NULL;
}

static inline int strlen(const char *s)
{
	int len = 0;
	while (*s++) len++;
	return len;
}

static inline char *strtok(char *s, const char *ct)
{
	if (s) ___strtok = s;
	if (!___strtok) return NULL;
	while (*___strtok && strchr(ct, *___strtok)) ___strtok++;
	if (!*___strtok) return (___strtok = NULL);
	s = ___strtok;
	while (*___strtok && !strchr(ct, *___strtok)) ___strtok++;
	if (*___strtok) *___strtok++ = '\0';
	else ___strtok = NULL;
	return s;
}

static inline void *memcpy(void *dest, const void *src, int n)
{
	char *d = dest;
	const char *s = src;
	while (n--) *d++ = *s++;
	return dest;
}

static inline void *memmove(void *dest, const void *src, int n)
{
	char *d = dest;
	const char *s = src;
	if (d < s) {
		while (n--) *d++ = *s++;
	} else {
		d += n; s += n;
		while (n--) *--d = *--s;
	}
	return dest;
}

static inline int memcmp(const void *cs, const void *ct, int count)
{
	const unsigned char *a = cs, *b = ct;
	while (count--) {
		if (*a != *b) return *a - *b;
		a++; b++;
	}
	return 0;
}

static inline void *memchr(const void *cs, char c, int count)
{
	const char *s = cs;
	while (count--) {
		if (*s == c) return (void *)s;
		s++;
	}
	return NULL;
}

static inline void *memset(void *s, char c, int count)
{
	char *d = s;
	while (count--) *d++ = c;
	return s;
}

#endif