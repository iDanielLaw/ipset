/* Copyright 2007-2010 Jozsef Kadlecsik (kadlec@blackhole.kfki.hu)
 *
 * This program is free software; you can redistribute it and/or modify   
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation.
 */
#ifndef LIBIPSET_PRINT_H
#define LIBIPSET_PRINT_H

#include <libipset/data.h>			/* enum ipset_opt */

extern int ipset_print_ether(char *buf, unsigned int len,
			     const struct ipset_data *data, enum ipset_opt opt,
			     uint8_t env);
extern int ipset_print_family(char *buf, unsigned int len,
			      const struct ipset_data *data, int opt,
			      uint8_t env);
extern int ipset_print_type(char *buf, unsigned int len,
			    const struct ipset_data *data, enum ipset_opt opt,
			    uint8_t env);
extern int ipset_print_ip(char *buf, unsigned int len,
			  const struct ipset_data *data, enum ipset_opt opt,
			  uint8_t env);
extern int ipset_print_ipaddr(char *buf, unsigned int len,
			      const struct ipset_data *data, enum ipset_opt opt,
			      uint8_t env);
extern int ipset_print_number(char *buf, unsigned int len,
			      const struct ipset_data *data, enum ipset_opt opt,
			      uint8_t env);
extern int ipset_print_name(char *buf, unsigned int len,
			    const struct ipset_data *data, enum ipset_opt opt,
			    uint8_t env);
extern int ipset_print_port(char *buf, unsigned int len,
			    const struct ipset_data *data, enum ipset_opt opt,
			    uint8_t env);
extern int ipset_print_flag(char *buf, unsigned int len,
			    const struct ipset_data *data, enum ipset_opt opt,
			    uint8_t env);
extern int ipset_print_elem(char *buf, unsigned int len,
			    const struct ipset_data *data, enum ipset_opt opt,
			    uint8_t env);

#define ipset_print_portnum	ipset_print_number

extern int ipset_print_data(char *buf, unsigned int len,
			    const struct ipset_data *data, enum ipset_opt opt,
			    uint8_t env);

#endif /* LIBIPSET_PRINT_H */
