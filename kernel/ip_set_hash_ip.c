/* Copyright (C) 2003-2010 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Kernel module implementing an IP set type: the hash:ip type */

#include <linux/netfilter/ip_set_kernel.h>
#include <linux/netfilter/ip_set_jhash.h>
#include <linux/module.h>
#include <linux/ip.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <linux/spinlock.h>
#include <linux/random.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/netlink.h>
#include <net/pfxlen.h>
#include <net/tcp.h>

#include <linux/netfilter.h>
#include <linux/netfilter/ip_set.h>
#include <linux/netfilter/ip_set_timeout.h>
#include <linux/netfilter/ip_set_hash.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("hash:ip type of IP sets");
MODULE_ALIAS("ip_set_hash:ip");

/* Type specific function prefix */
#define TYPE		hash_ip

static bool
hash_ip_same_set(const struct ip_set *a, const struct ip_set *b);

#define hash_ip4_same_set	hash_ip_same_set
#define hash_ip6_same_set	hash_ip_same_set

/* The type variant functions: IPv4 */

/* Member elements without timeout */
struct hash_ip4_elem {
	u32 ip;
};

/* Member elements with timeout support */
struct hash_ip4_telem {
	u32 ip;
	unsigned long timeout;
};

static inline bool
hash_ip4_data_equal(const struct hash_ip4_elem *ip1,
		    const struct hash_ip4_elem *ip2)
{
	return ip1->ip == ip2->ip;
}

static inline bool
hash_ip4_data_isnull(const struct hash_ip4_elem *elem)
{
	return elem->ip == 0;
}

static inline void
hash_ip4_data_copy(struct hash_ip4_elem *dst, const struct hash_ip4_elem *src)
{
	dst->ip = src->ip;
}

static inline void
hash_ip4_data_swap(struct hash_ip4_elem *dst, struct hash_ip4_elem *src)
{
	swap(dst->ip, src->ip);
}

/* Zero valued IP addresses cannot be stored */
static inline void
hash_ip4_data_zero_out(struct hash_ip4_elem *elem)
{
	elem->ip = 0;
}

static inline bool
hash_ip4_data_list(struct sk_buff *skb, const struct hash_ip4_elem *data)
{
	NLA_PUT_IPADDR4(skb, IPSET_ATTR_IP, data->ip);
	return 0;

nla_put_failure:
	return 1;
}

static inline bool
hash_ip4_data_tlist(struct sk_buff *skb, const struct hash_ip4_elem *data)
{
	const struct hash_ip4_telem *tdata =
		(const struct hash_ip4_telem *)data;

	NLA_PUT_IPADDR4(skb, IPSET_ATTR_IP, tdata->ip);
	NLA_PUT_NET32(skb, IPSET_ATTR_TIMEOUT,
		      htonl(ip_set_timeout_get(tdata->timeout)));

	return 0;

nla_put_failure:
	return 1;
}

#define IP_SET_HASH_WITH_NETMASK
#define PF		4
#define HOST_MASK	32
#include <linux/netfilter/ip_set_chash.h>

static int
hash_ip4_kadt(struct ip_set *set, const struct sk_buff *skb,
	      enum ipset_adt adt, u8 pf, u8 dim, u8 flags)
{
	struct chash *h = set->data;
	ipset_adtfn adtfn = set->variant->adt[adt];
	u32 ip;
	
	ip4addrptr(skb, flags & IPSET_DIM_ONE_SRC, &ip);
	ip &= NETMASK(h->netmask);
	if (ip == 0)
		return -EINVAL;

	return adtfn(set, &ip, GFP_ATOMIC, h->timeout);
}

static const struct nla_policy
hash_ip4_adt_policy[IPSET_ATTR_ADT_MAX + 1] __read_mostly = {
	[IPSET_ATTR_IP]		= { .type = NLA_NESTED },
	[IPSET_ATTR_IP_TO]	= { .type = NLA_NESTED },
	[IPSET_ATTR_CIDR]	= { .type = NLA_U8 },
	[IPSET_ATTR_TIMEOUT]	= { .type = NLA_U32 },
	[IPSET_ATTR_LINENO]	= { .type = NLA_U32 },
};

static int
hash_ip4_uadt(struct ip_set *set, struct nlattr *head, int len,
	      enum ipset_adt adt, u32 *lineno, u32 flags)
{
	struct chash *h = set->data;
	struct nlattr *tb[IPSET_ATTR_ADT_MAX+1];
	ipset_adtfn adtfn = set->variant->adt[adt];
	u32 ip, nip, ip_to, hosts, timeout = h->timeout;
	int ret = 0;

	if (nla_parse(tb, IPSET_ATTR_ADT_MAX, head, len,
		      hash_ip4_adt_policy))
		return -IPSET_ERR_PROTOCOL;

	if (tb[IPSET_ATTR_LINENO])
		*lineno = nla_get_u32(tb[IPSET_ATTR_LINENO]);

	ret = ip_set_get_ipaddr4(tb, IPSET_ATTR_IP, &ip);
	if (ret)
		return ret;

	ip &= NETMASK(h->netmask);
	if (ip == 0)
		return -IPSET_ERR_HASH_ELEM;

	if (tb[IPSET_ATTR_TIMEOUT]) {
		if (!with_timeout(h->timeout))
			return -IPSET_ERR_TIMEOUT;
		timeout = ip_set_timeout_uget(tb[IPSET_ATTR_TIMEOUT]);
	}

	if (adt == IPSET_TEST)
		return adtfn(set, &ip, GFP_ATOMIC, timeout);

	ip = ntohl(ip);
	if (tb[IPSET_ATTR_IP_TO]) {
		ret = ip_set_get_ipaddr4(tb, IPSET_ATTR_IP_TO, &ip_to);
		if (ret)
			return ret;
		ip_to = ntohl(ip_to);
		if (ip > ip_to)
			swap(ip, ip_to);
	} else if (tb[IPSET_ATTR_CIDR]) {
		u8 cidr = nla_get_u8(tb[IPSET_ATTR_CIDR]);
		
		if (cidr > 32)
			return -IPSET_ERR_INVALID_CIDR;
		ip &= HOSTMASK(cidr);
		ip_to = ip | ~HOSTMASK(cidr);
	} else
		ip_to = ip;

	hosts = h->netmask == 32 ? 1 : 2 << (32 - h->netmask - 1);

	for (; !before(ip_to, ip); ip += hosts) {
		nip = htonl(ip);
		ret = adtfn(set, &nip, GFP_ATOMIC, timeout);

		if (ret && !ip_set_eexist(ret, flags))
			return ret;
		else
			ret = 0;
	}
	return ret;
}

static bool
hash_ip_same_set(const struct ip_set *a, const struct ip_set *b)
{
	struct chash *x = a->data;
	struct chash *y = b->data;

	/* Resizing changes htable_bits, so we ignore it */
	return x->maxelem == y->maxelem
	       && x->timeout == y->timeout
	       && x->netmask == y->netmask
	       && x->array_size == y->array_size
	       && x->chain_limit == y->chain_limit;
}

/* The type variant functions: IPv6 */

struct hash_ip6_elem {
	union nf_inet_addr ip;
};

struct hash_ip6_telem {
	union nf_inet_addr ip;
	unsigned long timeout;
};

static inline bool
hash_ip6_data_equal(const struct hash_ip6_elem *ip1,
		    const struct hash_ip6_elem *ip2)
{
	return ipv6_addr_cmp(&ip1->ip.in6, &ip2->ip.in6) == 0;
}

static inline bool
hash_ip6_data_isnull(const struct hash_ip6_elem *elem)
{
	return ipv6_addr_any(&elem->ip.in6);
}

static inline void
hash_ip6_data_copy(struct hash_ip6_elem *dst, const struct hash_ip6_elem *src)
{
	ipv6_addr_copy(&dst->ip.in6, &src->ip.in6);
}

static inline void
hash_ip6_data_swap(struct hash_ip6_elem *dst, struct hash_ip6_elem *src)
{
	struct in6_addr tmp;
	
	ipv6_addr_copy(&tmp, &dst->ip.in6);
	ipv6_addr_copy(&dst->ip.in6, &src->ip.in6);
	ipv6_addr_copy(&src->ip.in6, &tmp);
}

static inline void
hash_ip6_data_zero_out(struct hash_ip6_elem *elem)
{
	ipv6_addr_set(&elem->ip.in6, 0, 0, 0, 0);
}

static inline void
ip6_netmask(union nf_inet_addr *ip, u8 prefix)
{
	ip->ip6[0] &= NETMASK6(prefix)[0];
	ip->ip6[1] &= NETMASK6(prefix)[1];
	ip->ip6[2] &= NETMASK6(prefix)[2];
	ip->ip6[3] &= NETMASK6(prefix)[3];
}

static inline bool
hash_ip6_data_list(struct sk_buff *skb, const struct hash_ip6_elem *data)
{
	NLA_PUT_IPADDR6(skb, IPSET_ATTR_IP, &data->ip);
	return 0;

nla_put_failure:
	return 1;
}

static inline bool
hash_ip6_data_tlist(struct sk_buff *skb, const struct hash_ip6_elem *data)
{
	const struct hash_ip6_telem *e = 
		(const struct hash_ip6_telem *)data;
	
	NLA_PUT_IPADDR6(skb, IPSET_ATTR_IP, &e->ip);
	NLA_PUT_NET32(skb, IPSET_ATTR_TIMEOUT,
		      htonl(ip_set_timeout_get(e->timeout)));
	return 0;

nla_put_failure:
	return 1;
}

#undef PF
#undef HOST_MASK

#define PF		6
#define HOST_MASK	128
#include <linux/netfilter/ip_set_chash.h>

static int
hash_ip6_kadt(struct ip_set *set, const struct sk_buff *skb,
	      enum ipset_adt adt, u8 pf, u8 dim, u8 flags)
{
	struct chash *h = set->data;
	ipset_adtfn adtfn = set->variant->adt[adt];
	union nf_inet_addr ip;

	ip6addrptr(skb, flags & IPSET_DIM_ONE_SRC, &ip.in6);
	ip6_netmask(&ip, h->netmask);
	if (ipv6_addr_any(&ip.in6))
		return -EINVAL;

	return adtfn(set, &ip, GFP_ATOMIC, h->timeout);
}

static const struct nla_policy
hash_ip6_adt_policy[IPSET_ATTR_ADT_MAX + 1] __read_mostly = {
	[IPSET_ATTR_IP]		= { .type = NLA_NESTED },
	[IPSET_ATTR_TIMEOUT]	= { .type = NLA_U32 },
	[IPSET_ATTR_LINENO]	= { .type = NLA_U32 },
};

static int
hash_ip6_uadt(struct ip_set *set, struct nlattr *head, int len,
	      enum ipset_adt adt, u32 *lineno, u32 flags)
{
	struct chash *h = set->data;
	struct nlattr *tb[IPSET_ATTR_ADT_MAX+1];
	ipset_adtfn adtfn = set->variant->adt[adt];
	union nf_inet_addr ip;
	u32 timeout = h->timeout;
	int ret;

	if (nla_parse(tb, IPSET_ATTR_ADT_MAX, head, len,
		      hash_ip6_adt_policy))
		return -IPSET_ERR_PROTOCOL;

	if (tb[IPSET_ATTR_LINENO])
		*lineno = nla_get_u32(tb[IPSET_ATTR_LINENO]);

	ret = ip_set_get_ipaddr6(tb, IPSET_ATTR_IP, &ip);
	if (ret)
		return ret;

	ip6_netmask(&ip, h->netmask);
	if (ipv6_addr_any(&ip.in6))
		return -IPSET_ERR_HASH_ELEM;

	if (tb[IPSET_ATTR_TIMEOUT]) {
		if (!with_timeout(h->timeout))
			return -IPSET_ERR_TIMEOUT;
		timeout = ip_set_timeout_uget(tb[IPSET_ATTR_TIMEOUT]);
	}

	ret = adtfn(set, &ip, GFP_ATOMIC, timeout);

	return ip_set_eexist(ret, flags) ? 0 : ret;
}

/* Create hash:ip type of sets */

static const struct nla_policy
hash_ip_create_policy[IPSET_ATTR_CREATE_MAX+1] __read_mostly = {
	[IPSET_ATTR_HASHSIZE]	= { .type = NLA_U32 },
	[IPSET_ATTR_MAXELEM]	= { .type = NLA_U32 },
	[IPSET_ATTR_PROBES]	= { .type = NLA_U8 },
	[IPSET_ATTR_RESIZE]	= { .type = NLA_U8  },
	[IPSET_ATTR_TIMEOUT]	= { .type = NLA_U32 },
	[IPSET_ATTR_NETMASK]	= { .type = NLA_U8  },
};

static int
hash_ip_create(struct ip_set *set, struct nlattr *head, int len, u32 flags)
{
	struct nlattr *tb[IPSET_ATTR_CREATE_MAX+1];
	u32 hashsize = IPSET_DEFAULT_HASHSIZE, maxelem = IPSET_DEFAULT_MAXELEM;
	u8 netmask;
	struct chash *h;

	if (!(set->family == AF_INET || set->family == AF_INET6))
		return -IPSET_ERR_INVALID_FAMILY;
	netmask = set->family == AF_INET ? 32 : 128;
	pr_debug("Create set %s with family %s",
		 set->name, set->family == AF_INET ? "inet" : "inet6");

	if (nla_parse(tb, IPSET_ATTR_CREATE_MAX, head, len,
		      hash_ip_create_policy))
		return -IPSET_ERR_PROTOCOL;

	if (tb[IPSET_ATTR_HASHSIZE]) {
		hashsize = ip_set_get_h32(tb[IPSET_ATTR_HASHSIZE]);
		if (hashsize < IPSET_MIMINAL_HASHSIZE)
			hashsize = IPSET_MIMINAL_HASHSIZE;
	}

	if (tb[IPSET_ATTR_MAXELEM])
		maxelem = ip_set_get_h32(tb[IPSET_ATTR_MAXELEM]);

	if (tb[IPSET_ATTR_NETMASK]) {
		netmask = nla_get_u8(tb[IPSET_ATTR_NETMASK]);
		
		if ((set->family == AF_INET && netmask > 32)
		    || (set->family == AF_INET6 && netmask > 128)
		    || netmask == 0)
			return -IPSET_ERR_INVALID_NETMASK;
	}

	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return -ENOMEM;

	h->maxelem = maxelem;
	h->netmask = netmask;
	h->htable_bits = htable_bits(hashsize);
	h->array_size = CHASH_DEFAULT_ARRAY_SIZE;
	h->chain_limit = CHASH_DEFAULT_CHAIN_LIMIT;
	get_random_bytes(&h->initval, sizeof(h->initval));
	h->timeout = IPSET_NO_TIMEOUT;

	h->htable = ip_set_alloc(
			jhash_size(h->htable_bits) * sizeof(struct slist),
			GFP_KERNEL);
	if (!h->htable) {
		kfree(h);
		return -ENOMEM;
	}

	set->data = h;

	if (tb[IPSET_ATTR_TIMEOUT]) {
		h->timeout = ip_set_timeout_uget(tb[IPSET_ATTR_TIMEOUT]);
		
		set->variant = set->family == AF_INET
			? &hash_ip4_tvariant : &hash_ip6_tvariant;

		if (set->family == AF_INET)
			hash_ip4_gc_init(set);
		else
			hash_ip6_gc_init(set);
	} else {
		set->variant = set->family == AF_INET
			? &hash_ip4_variant : &hash_ip6_variant;
	}
	
	pr_debug("create %s hashsize %u (%u) maxelem %u: %p(%p)",
		 set->name, jhash_size(h->htable_bits),
		 h->htable_bits, h->maxelem, set->data, h->htable);
	   
	return 0;
}

static struct ip_set_type hash_ip_type = {
	.name		= "hash:ip",
	.protocol	= IPSET_PROTOCOL,
	.features	= IPSET_TYPE_IP,
	.dimension	= IPSET_DIM_ONE,
	.family		= AF_UNSPEC,
	.revision	= 0,
	.create		= hash_ip_create,
	.me		= THIS_MODULE,
};

static int __init
hash_ip_init(void)
{
	return ip_set_type_register(&hash_ip_type);
}

static void __exit
hash_ip_fini(void)
{
	ip_set_type_unregister(&hash_ip_type);
}

module_init(hash_ip_init);
module_exit(hash_ip_fini);