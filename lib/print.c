/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify   
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation.
 */
#include <assert.h>				/* assert */
#include <errno.h>				/* errno */
#include <stdio.h>				/* snprintf */
#include <netdb.h>				/* getservbyport */
#include <sys/types.h>				/* inet_ntop */
#include <sys/socket.h>				/* inet_ntop */
#include <arpa/inet.h>				/* inet_ntop */
#include <net/ethernet.h>			/* ETH_ALEN */

#include <libipset/data.h>			/* ipset_data_* */
#include <libipset/parse.h>			/* IPSET_*_SEPARATOR */
#include <libipset/types.h>			/* ipset set types */
#include <libipset/session.h>			/* IPSET_FLAG_ */
#include <libipset/utils.h>			/* UNUSED */
#include <libipset/ui.h>			/* IPSET_ENV_* */
#include <libipset/print.h>			/* prototypes */

/* Print data (to output buffer). All function must follow snprintf. */

#define SNPRINTF_FAILURE(size, len, offset)			\
do {								\
	if (size < 0 || (unsigned int) size >= len)		\
		return size;					\
	offset += size;						\
	len -= size;						\
} while (0)

/**
 * ipset_print_ether - print ethernet address to string
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print Ethernet address to output buffer.
 *
 * Return lenght of printed string or error size.
 */
int
ipset_print_ether(char *buf, unsigned int len,
		  const struct ipset_data *data, enum ipset_opt opt,
		  uint8_t env UNUSED)
{
	const unsigned char *ether;
	int i, size, offset = 0;
	
	assert(buf);
	assert(len > 0);
	assert(data);
	assert(opt == IPSET_OPT_ETHER);
	
	if (len < ETH_ALEN*3)
		return -1;
	
	ether = ipset_data_get(data, opt);
	assert(ether);

	size = snprintf(buf, len, "%02X", ether[0]);
	SNPRINTF_FAILURE(size, len, offset);
	for (i = 1; i < ETH_ALEN; i++) {
		size = snprintf(buf + offset, len, ":%02X", ether[i]);
		SNPRINTF_FAILURE(size, len, offset);
	}
	
	return offset;
}

/**
 * ipset_print_family - print INET family
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print INET family string to output buffer.
 *
 * Return lenght of printed string or error size.
 */
int
ipset_print_family(char *buf, unsigned int len,
		   const struct ipset_data *data, int opt,
		   uint8_t env UNUSED)
{
	uint8_t family;

	assert(buf);
	assert(len > 0);
	assert(data);
	assert(opt == IPSET_OPT_FAMILY);

	if (len < strlen("inet6") + 1)
		return -1;

	family = ipset_data_family(data);

	return snprintf(buf, len, "%s",
			family == AF_INET ? "inet" :
			family == AF_INET6 ? "inet6" : "any");
}

/**
 * ipset_print_type - print ipset type string
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print ipset module string identifier to output buffer.
 *
 * Return lenght of printed string or error size.
 */
int
ipset_print_type(char *buf, unsigned int len,
		 const struct ipset_data *data, enum ipset_opt opt,
		 uint8_t env UNUSED)
{
	const struct ipset_type *type;

	assert(buf);
	assert(len > 0);
	assert(data);
	assert(opt == IPSET_OPT_TYPE);

	type = ipset_data_get(data, opt);
	assert(type);
	if (len < strlen(type->name) + 1)
		return -1;
		
	return snprintf(buf, len, "%s", type->name);
}

#define GETNAMEINFO(family, f, n)					\
static inline int							\
__getnameinfo##f(char *buf, unsigned int len,				\
		 int flags, const union nf_inet_addr *addr)		\
{									\
	struct sockaddr_in##n saddr;					\
	int err;							\
									\
	memset(&saddr, 0, sizeof(saddr));				\
	in##f##cpy(&saddr.sin##n##_addr, &addr->in##n);			\
	saddr.sin##n##_family = family;					\
									\
	err = getnameinfo((const struct sockaddr *)&saddr, 		\
			  sizeof(saddr),				\
			  buf, len, NULL, 0, flags);			\
									\
	if (err == EAI_AGAIN && !(flags & NI_NUMERICHOST))		\
		err = getnameinfo((const struct sockaddr *)&saddr, 	\
				  sizeof(saddr),			\
				  buf, len, NULL, 0, 			\
				  flags | NI_NUMERICHOST);		\
	return (err != 0 ? -1 : (int)strlen(buf));			\
}

#define SNPRINTF_IP(mask, f)						\
static int								\
snprintf_ipv##f(char *buf, unsigned int len, int flags,			\
	       const union nf_inet_addr *ip, uint8_t cidr)		\
{									\
	int size, offset = 0;						\
									\
	size = __getnameinfo##f(buf, len, flags, ip);			\
	SNPRINTF_FAILURE(size, len, offset);				\
									\
	if (cidr == mask)						\
		return offset;						\
	if ((unsigned int)(size + 5) < len) 				\
		return -1;						\
	size = snprintf(buf + offset, len,				\
			"%s%u", IPSET_CIDR_SEPARATOR, cidr);		\
	SNPRINTF_FAILURE(size, len, offset);				\
	return offset;							\
}

GETNAMEINFO(AF_INET, 4, )
SNPRINTF_IP(32, 4)

GETNAMEINFO(AF_INET6, 6, 6)
SNPRINTF_IP(128, 6)

/**
 * ipset_print_ip - print IPv4|IPv6 address to string
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print IPv4|IPv6 address, address/cidr or address range to output buffer.
 *
 * Return lenght of printed string or error size.
 */
int
ipset_print_ip(char *buf, unsigned int len,
	       const struct ipset_data *data, enum ipset_opt opt,
	       uint8_t env)
{
	const union nf_inet_addr *ip;
	uint8_t family, cidr;
	int flags, size, offset = 0;
	enum ipset_opt cidropt;

	assert(buf);
	assert(len > 0);
	assert(data);
	assert(opt == IPSET_OPT_IP || opt == IPSET_OPT_IP2);

	D("len: %u", len);
	family = ipset_data_family(data);
	cidropt = opt == IPSET_OPT_IP ? IPSET_OPT_CIDR : IPSET_OPT_CIDR2;
	if (ipset_data_test(data, cidropt))
		cidr = *(uint8_t *) ipset_data_get(data, cidropt);
	else
		cidr = family == AF_INET6 ? 128 : 32;
	flags = env & (1 << IPSET_ENV_RESOLVE) ? 0 : NI_NUMERICHOST;
	
	ip = ipset_data_get(data, opt);
	assert(ip);
	if (family == AF_INET)
		size = snprintf_ipv4(buf, len, flags, ip, cidr);
	else if (family == AF_INET6)
		size = snprintf_ipv6(buf, len, flags, ip, cidr);
	else
		return -1;
	SNPRINTF_FAILURE(size, len, offset);

	D("len: %u, offset %u", len, offset);
	if (!ipset_data_test(data, IPSET_OPT_IP_TO))
		return offset;

	size = snprintf(buf + offset, len, "%s", IPSET_RANGE_SEPARATOR);
	SNPRINTF_FAILURE(size, len, offset);

	ip = ipset_data_get(data, IPSET_OPT_IP_TO);
	if (family == AF_INET)
		size = snprintf_ipv4(buf + offset, len, flags, ip, cidr);
	else if (family == AF_INET6)
		size = snprintf_ipv6(buf + offset, len, flags, ip, cidr);
	else
		return -1;
	
	SNPRINTF_FAILURE(size, len, offset);	
	return offset;
}

/**
 * ipset_print_ipaddr - print IPv4|IPv6 address to string
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print IPv4|IPv6 address or address/cidr to output buffer.
 *
 * Return lenght of printed string or error size.
 */
int
ipset_print_ipaddr(char *buf, unsigned int len,
		   const struct ipset_data *data, enum ipset_opt opt,
		   uint8_t env)
{
	const union nf_inet_addr *ip;
	uint8_t family, cidr;
	enum ipset_opt cidropt;
	int flags;

	assert(buf);
	assert(len > 0);
	assert(data);
	assert(opt == IPSET_OPT_IP
	       || opt == IPSET_OPT_IP_TO
	       || opt == IPSET_OPT_IP2);

	family = ipset_data_family(data);
	cidropt = opt == IPSET_OPT_IP ? IPSET_OPT_CIDR : IPSET_OPT_CIDR2;
	if (ipset_data_test(data, cidropt))
		cidr = *(uint8_t *) ipset_data_get(data, cidropt);
	else
		cidr = family == AF_INET6 ? 128 : 32;
	flags = env & (1 << IPSET_ENV_RESOLVE) ? 0 : NI_NUMERICHOST;

	ip = ipset_data_get(data, opt);
	assert(ip);
	if (family == AF_INET)
		return snprintf_ipv4(buf, len, flags, ip, cidr);
	else if (family == AF_INET6)
		return snprintf_ipv6(buf, len, flags, ip, cidr);

	return -1;
}

/**
 * ipset_print_number - print number to string
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print number to output buffer.
 *
 * Return lenght of printed string or error size.
 */
int
ipset_print_number(char *buf, unsigned int len,
		   const struct ipset_data *data, enum ipset_opt opt,
		   uint8_t env UNUSED)
{
	size_t maxsize;
	const void *number;

	assert(buf);
	assert(len > 0);
	assert(data);
	
	number = ipset_data_get(data, opt);
	maxsize = ipset_data_sizeof(opt, AF_INET);
	D("opt: %u, maxsize %zu", opt, maxsize);
	if (maxsize == sizeof(uint8_t))
		return snprintf(buf, len, "%u", *(uint8_t *) number);
	else if (maxsize == sizeof(uint16_t))
		return snprintf(buf, len, "%u", *(uint16_t *) number);
	else if (maxsize == sizeof(uint32_t))
		return snprintf(buf, len, "%lu",
				(long unsigned) *(uint32_t *) number);
	else
		assert(0);
	return 0;
}

/**
 * ipset_print_name - print setname element string
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print setname element string to output buffer.
 *
 * Return lenght of printed string or error size.
 */
int
ipset_print_name(char *buf, unsigned int len,
		 const struct ipset_data *data, enum ipset_opt opt,
		 uint8_t env UNUSED)
{
	const char *name;
	int size, offset = 0;

	assert(buf);
	assert(len > 0);
	assert(data);
	assert(opt == IPSET_OPT_NAME);

	if (len < 2*IPSET_MAXNAMELEN + 2 + strlen("before"))
		return -1;

	name = ipset_data_get(data, opt);
	assert(name);
	size = snprintf(buf, len, "%s", name);
	SNPRINTF_FAILURE(size, len, offset);	

	if (ipset_data_test(data, IPSET_OPT_NAMEREF)) {
		bool before = ipset_data_test(data, IPSET_OPT_BEFORE);
		size = snprintf(buf + offset, len,
			        "%s%s%s%s", IPSET_ELEM_SEPARATOR,
			        before ? "before" : "after",
			        IPSET_ELEM_SEPARATOR,
			        (const char *) ipset_data_get(data,
			        	IPSET_OPT_NAMEREF));
		SNPRINTF_FAILURE(size, len, offset);	
	}

	return offset;
}

/**
 * ipset_print_port - print port or port range
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print port or port range to output buffer.
 *
 * Return lenght of printed string or error size.
 */
int
ipset_print_port(char *buf, unsigned int len,
		 const struct ipset_data *data, enum ipset_opt opt,
		 uint8_t env UNUSED)
{
	const uint16_t *port;
	int size, offset = 0;

	assert(buf);
	assert(len > 0);
	assert(data);
	assert(opt == IPSET_OPT_PORT);

	if (len < 2*strlen("65535") + 2)
		return -1;

	port = ipset_data_get(data, IPSET_OPT_PORT);
	assert(port);
	size = snprintf(buf, len, "%u", *port);
	SNPRINTF_FAILURE(size, len, offset);	
	
	if (ipset_data_test(data, IPSET_OPT_PORT_TO)) {
		port = ipset_data_get(data, IPSET_OPT_PORT_TO);
		size = snprintf(buf + offset, len,
			        "%s%u",
			        IPSET_RANGE_SEPARATOR, *port);
		SNPRINTF_FAILURE(size, len, offset);
	}

	return offset;
}

#define print_second(data)	\
ipset_data_flags_test(data,	\
	IPSET_FLAG(IPSET_OPT_PORT)|IPSET_FLAG(IPSET_OPT_ETHER)) 

#define print_third(data)	\
ipset_data_flags_test(data, IPSET_FLAG(IPSET_OPT_IP2)) 

/**
 * ipset_print_elem - print ADT elem according to settype
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print (multipart) element according to settype
 *
 * Return lenght of printed string or error size.
 */
int
ipset_print_elem(char *buf, unsigned int len,
		 const struct ipset_data *data, enum ipset_opt opt UNUSED,
		 uint8_t env)
{
	const struct ipset_type *type;
	int size, offset = 0;

	assert(buf);
	assert(len > 0);
	assert(data);

	type = ipset_data_get(data, IPSET_OPT_TYPE);
	if (!type)
		return -1;
	
	size = type->elem[IPSET_DIM_ONE].print(buf, len, data,
			type->elem[IPSET_DIM_ONE].opt, env);
	SNPRINTF_FAILURE(size, len, offset);
	if (ipset_data_test(data, type->elem[IPSET_DIM_TWO].opt))
		D("print second elem");
	if (type->dimension == IPSET_DIM_ONE
	    || (type->last_elem_optional
	        && !ipset_data_test(data, type->elem[IPSET_DIM_TWO].opt)))
		return offset;
	
	size = snprintf(buf + offset, len, IPSET_ELEM_SEPARATOR);
	SNPRINTF_FAILURE(size, len, offset);
	size = type->elem[IPSET_DIM_TWO].print(buf + offset, len, data,
			type->elem[IPSET_DIM_TWO].opt, env);
	SNPRINTF_FAILURE(size, len, offset);
	if (type->dimension == IPSET_DIM_TWO
	    || (type->last_elem_optional
	        && !ipset_data_test(data, type->elem[IPSET_DIM_THREE].opt)))
		return offset;

	size = snprintf(buf + offset, len, IPSET_ELEM_SEPARATOR);
	SNPRINTF_FAILURE(size, len, offset);
	size = type->elem[IPSET_DIM_THREE].print(buf + offset, len, data,
			type->elem[IPSET_DIM_THREE].opt, env);
	SNPRINTF_FAILURE(size, len, offset);

	return offset;
}

/**
 * ipset_print_flag - print a flag
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Print a flag, i.e. option without value
 *
 * Return lenght of printed string or error size.
 */
int
ipset_print_flag(char *buf UNUSED, unsigned int len UNUSED,
		 const struct ipset_data *data UNUSED,
		 enum ipset_opt opt UNUSED, uint8_t env UNUSED)
{	
	return 0;
}

/**
 * ipset_print_data - print data, generic fuction
 * @buf: printing buffer
 * @len: length of available buffer space
 * @data: data blob
 * @opt: the option kind
 * @env: environment flags
 *
 * Generic wrapper of the printing functions.
 *
 * Return lenght of printed string or error size.
 */
int
ipset_print_data(char *buf, unsigned int len,
		 const struct ipset_data *data, enum ipset_opt opt,
		 uint8_t env)
{
	int size = 0, offset = 0;

	assert(buf);
	assert(len > 0);
	assert(data);

	switch (opt) {
	case IPSET_OPT_FAMILY:
		size = ipset_print_family(buf, len, data, opt, env);
		break;
	case IPSET_OPT_TYPE:
		size = ipset_print_type(buf, len, data, opt, env);
		break;
	case IPSET_SETNAME:
		size = snprintf(buf, len, "%s", ipset_data_setname(data));
		break;
	case IPSET_OPT_ELEM:
		size = ipset_print_elem(buf, len, data, opt, env);
		break;
	case IPSET_OPT_IP:
		size = ipset_print_ip(buf, len, data, opt, env);
		break;
	case IPSET_OPT_PORT:
		size = ipset_print_port(buf, len, data, opt, env);
		break;
	case IPSET_OPT_GC:
	case IPSET_OPT_HASHSIZE:
	case IPSET_OPT_MAXELEM:
	case IPSET_OPT_NETMASK:
	case IPSET_OPT_PROBES:
	case IPSET_OPT_RESIZE:
	case IPSET_OPT_TIMEOUT:
	case IPSET_OPT_REFERENCES:
	case IPSET_OPT_ELEMENTS:
	case IPSET_OPT_SIZE:
		size = ipset_print_number(buf, len, data, opt, env);
		break;
	default:
		return -1;
	}
	SNPRINTF_FAILURE(size, len, offset);
	
	return offset;
}
