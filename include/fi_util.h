/*
 * Copyright (c) 2015-2017 Intel Corporation, Inc.  All rights reserved.
 * Copyright (c) 2016 Cisco Systems, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <pthread.h>
#include <stdio.h>

#include <rdma/fabric.h>
#include <rdma/fi_atomic.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_eq.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_tagged.h>
#include <rdma/fi_trigger.h>

#include <fi.h>
#include <fi_list.h>
#include <fi_mem.h>
#include <fi_rbuf.h>
#include <fi_signal.h>
#include <fi_enosys.h>
#include <fi_osd.h>
#include <fi_indexer.h>

#ifndef _FI_UTIL_H_
#define _FI_UTIL_H_


#define UTIL_FLAG_ERROR	(1ULL << 60)

#define OFI_Q_STRERROR(prov, log, q, q_str, entry, strerror)			\
	FI_WARN(prov, log, "fi_" q_str "_readerr: err: %d, prov_err: %s (%d)\n",\
			entry.err,						\
			strerror(q, entry.prov_errno, entry.err_data, NULL, 0), \
			entry.prov_errno)

#define OFI_Q_READERR(prov, log, q, q_str, readerr, strerror, ret, err_entry)	\
	do {									\
		ret = readerr(q, &err_entry, 0);				\
		if (ret != sizeof(err_entry)) {					\
			FI_WARN(prov, log,					\
					"Unable to fi_" q_str "_readerr\n");	\
		} else {							\
			OFI_Q_STRERROR(prov, log, q, q_str,			\
					err_entry, strerror);			\
		}								\
	} while (0)

#define OFI_CQ_READERR(prov, log, cq, ret, err_entry)		\
	OFI_Q_READERR(prov, log, cq, "cq", fi_cq_readerr,	\
			fi_cq_strerror, ret, err_entry)

#define OFI_EQ_READERR(prov, log, eq, ret, err_entry)		\
	OFI_Q_READERR(prov, log, eq, "eq", fi_eq_readerr, 	\
			fi_eq_strerror, ret, err_entry)

#define ofi_sin_addr(addr) (((struct sockaddr_in *)(addr))->sin_addr)
#define ofi_sin6_addr(addr) (((struct sockaddr_in6 *)(addr))->sin6_addr)

#define FI_INFO_FIELD(provider, prov_attr, user_attr, prov_str, user_str, type)	\
	do {										\
		FI_INFO(provider, FI_LOG_CORE, prov_str ": %s\n",			\
				fi_tostr(&prov_attr, type));				\
		FI_INFO(provider, FI_LOG_CORE, user_str ": %s\n",			\
				fi_tostr(&user_attr, type));				\
	} while (0)

#define FI_INFO_STRING(provider, prov_attr, user_attr, prov_str, user_str)	\
	do {									\
		FI_INFO(provider, FI_LOG_CORE, prov_str ": %s\n", prov_attr);	\
		FI_INFO(provider, FI_LOG_CORE, user_str ": %s\n", user_attr);	\
	} while (0)

#define FI_INFO_CHECK(provider, prov, user, field, type)		\
	FI_INFO_FIELD(provider, prov->field, user->field, "Supported",	\
		      "Requested", type)

#define FI_INFO_CHECK_VAL(provider, prov, user, field)					\
	do {										\
		FI_INFO(provider, FI_LOG_CORE, "Supported: %zd\n", prov->field);	\
		FI_INFO(provider, FI_LOG_CORE, "Requested: %zd\n", user->field);	\
	} while (0)

#define FI_INFO_MODE(provider, prov_mode, user_mode)				\
	FI_INFO_FIELD(provider, prov_mode, user_mode, "Expected", "Given",	\
		      FI_TYPE_MODE)

#define FI_INFO_MR_MODE(provider, prov_mode, user_mode)			\
	FI_INFO_FIELD(provider, prov_mode, user_mode, "Expected", "Given",	\
		      FI_TYPE_MR_MODE)

#define FI_INFO_NAME(provider, prov, user)				\
	FI_INFO_STRING(provider, prov->name, user->name, "Supported",	\
		       "Requested")

enum {
	UTIL_TX_SHARED_CTX = 1 << 0,
	UTIL_RX_SHARED_CTX = 1 << 1,
};

/*
 * Provider details
 */
struct util_prov {
	const struct fi_provider	*prov;
	const struct fi_info		*info;
	const int			flags;
};


/*
 * Fabric
 */
struct util_fabric_info {
	const char 			*name;
	const struct fi_provider 	*prov;
};

struct util_fabric {
	struct fid_fabric	fabric_fid;
	struct dlist_entry	list_entry;
	fastlock_t		lock;
	ofi_atomic32_t		ref;
	const char		*name;
	const struct fi_provider *prov;

	struct dlist_entry	domain_list;
};

int ofi_fabric_init(const struct fi_provider *prov,
		    const struct fi_fabric_attr *prov_attr,
		    const struct fi_fabric_attr *user_attr,
		    struct util_fabric *fabric, void *context);
int ofi_fabric_close(struct util_fabric *fabric);
int ofi_trywait(struct fid_fabric *fabric, struct fid **fids, int count);

/*
 * Domain
 */
struct util_domain {
	struct fid_domain	domain_fid;
	struct dlist_entry	list_entry;
	struct util_fabric	*fabric;
	struct util_eq		*eq;
	fastlock_t		lock;
	ofi_atomic32_t		ref;
	const struct fi_provider *prov;

	char			*name;
	uint64_t		info_domain_caps;
	uint64_t		info_domain_mode;
	int			mr_mode;
	uint32_t		addr_format;
	enum fi_av_type		av_type;
};

int ofi_domain_init(struct fid_fabric *fabric_fid, const struct fi_info *info,
		     struct util_domain *domain, void *context);
int ofi_domain_bind_eq(struct util_domain *domain, struct util_eq *eq);
int ofi_domain_close(struct util_domain *domain);


/*
 * Endpoint
 */

struct util_ep;
typedef void (*ofi_ep_progress_func)(struct util_ep *util_ep);

struct util_ep {
	struct fid_ep		ep_fid;
	struct util_domain	*domain;

	struct util_av		*av;
	struct dlist_entry	av_entry;
	struct util_cq		*rx_cq;
	struct util_cq		*tx_cq;
	struct util_eq		*eq;

	uint64_t		caps;
	uint64_t		flags;
	ofi_ep_progress_func	progress;
	struct util_cmap	*cmap;
	fastlock_t		lock;
};

int ofi_ep_bind_av(struct util_ep *util_ep, struct util_av *av);
int ofi_ep_bind_eq(struct util_ep *ep, struct util_eq *eq);
int ofi_endpoint_init(struct fid_domain *domain, const struct util_prov *util_prov,
		      struct fi_info *info, struct util_ep *ep, void *context,
		      ofi_ep_progress_func progress);

int ofi_endpoint_close(struct util_ep *util_ep);

/*
 * Completion queue
 *
 * Utility provider derived CQs that require manual progress must
 * progress the CQ when fi_cq_read is called with a count = 0.
 * In such cases, fi_cq_read will return 0 if there are available
 * entries on the CQ.  This allows poll sets to drive progress
 * without introducing private interfaces to the CQ.
 */
#define FI_DEFAULT_CQ_SIZE	1024

typedef void (*fi_cq_read_func)(void **dst, void *src);

struct util_cq_err_entry {
	struct fi_cq_err_entry	err_entry;
	struct slist_entry	list_entry;
};

OFI_DECLARE_CIRQUE(struct fi_cq_tagged_entry, util_comp_cirq);

typedef void (*ofi_cq_progress_func)(struct util_cq *cq);

struct util_cq {
	struct fid_cq		cq_fid;
	struct util_domain	*domain;
	struct util_wait	*wait;
	ofi_atomic32_t		ref;
	struct dlist_entry	ep_list;
	fastlock_t		ep_list_lock;
	fastlock_t		cq_lock;

	struct util_comp_cirq	*cirq;
	fi_addr_t		*src;

	struct slist		err_list;
	fi_cq_read_func		read_entry;
	int			internal_wait;
	ofi_cq_progress_func	progress;
};

int ofi_cq_init(const struct fi_provider *prov, struct fid_domain *domain,
		 struct fi_cq_attr *attr, struct util_cq *cq,
		 ofi_cq_progress_func progress, void *context);
void ofi_cq_progress(struct util_cq *cq);
int ofi_cq_cleanup(struct util_cq *cq);
ssize_t ofi_cq_read(struct fid_cq *cq_fid, void *buf, size_t count);
ssize_t ofi_cq_readfrom(struct fid_cq *cq_fid, void *buf, size_t count,
		fi_addr_t *src_addr);
ssize_t ofi_cq_readerr(struct fid_cq *cq_fid, struct fi_cq_err_entry *buf,
		uint64_t flags);
ssize_t ofi_cq_sread(struct fid_cq *cq_fid, void *buf, size_t count,
		const void *cond, int timeout);
ssize_t ofi_cq_sreadfrom(struct fid_cq *cq_fid, void *buf, size_t count,
		fi_addr_t *src_addr, const void *cond, int timeout);
int ofi_cq_signal(struct fid_cq *cq_fid);

/*
 * Counter
 */
struct util_cntr {
	struct fid_cntr		cntr_fid;
	struct util_domain	*domain;
	ofi_atomic32_t		ref;
	uint64_t		checkpoint_cnt;
	uint64_t		checkpoint_err;
};


/*
 * AV / addressing
 */
struct util_av_hash_entry {
	int			index;
	int			next;
};

struct util_av_hash {
	struct util_av_hash_entry *table;
	int			free_list;
	int			slots;
	int			total_count;
};

struct util_av {
	struct fid_av		av_fid;
	struct util_domain	*domain;
	struct util_eq		*eq;
	ofi_atomic32_t		ref;
	fastlock_t		lock;
	const struct fi_provider *prov;

	void			*context;
	uint64_t		flags;
	size_t			count;
	size_t			addrlen;
	ssize_t			free_list;
	struct util_av_hash	hash;
	void			*data;
	struct dlist_entry	ep_list;
};

struct util_av_attr {
	size_t			addrlen;
	size_t			overhead;
	uint64_t		flags;
};

int ofi_av_init(struct util_domain *domain,
	       const struct fi_av_attr *attr, const struct util_av_attr *util_attr,
	       struct util_av *av, void *context);
int ofi_av_close(struct util_av *av);

int ofi_av_insert_addr(struct util_av *av, const void *addr, int slot, int *index);
int ofi_av_lookup_index(struct util_av *av, const void *addr, int slot);
int ofi_av_bind(struct fid *av_fid, struct fid *eq_fid, uint64_t flags);
void ofi_av_write_event(struct util_av *av, uint64_t data,
			int err, void *context);

int ip_av_create(struct fid_domain *domain_fid, struct fi_av_attr *attr,
		 struct fid_av **av, void *context);

void *ofi_av_get_addr(struct util_av *av, int index);
#define ip_av_get_addr ofi_av_get_addr
int ip_av_get_index(struct util_av *av, const void *addr);

int ofi_get_addr(uint32_t addr_format, uint64_t flags,
		 const char *node, const char *service,
		 void **addr, size_t *addrlen);
int ofi_get_src_addr(uint32_t addr_format,
		     const void *dest_addr, size_t dest_addrlen,
		     void **src_addr, size_t *src_addrlen);
void ofi_getnodename(char *buf, int buflen);
int ofi_av_get_index(struct util_av *av, const void *addr);

/*
 * Connection Map
 */

#define UTIL_CMAP_IDX_BITS 48

enum util_cmap_state {
	CMAP_UNSPEC,
	CMAP_CONNECTING,
	CMAP_CONNECTED
};

struct util_cmap_handle {
	struct util_cmap *cmap;
	enum util_cmap_state state;
	/* Unique identifier for a connection. Can be exchanged with a peer
	 * during connection setup and can later be used in a messsage header
	 * to identify the source of the message (Used for FI_SOURCE, RNDV
	 * protocol, etc.) */
	uint64_t key;
	uint64_t remote_key;
	fi_addr_t fi_addr;
	struct util_cmap_peer *peer;
};

struct util_cmap_peer {
	struct util_cmap_handle *handle;
	struct dlist_entry entry;
	size_t addrlen;
	uint8_t addr[];
};

typedef void (*ofi_cmap_free_handle_func)(void *arg);

struct util_cmap {
	struct util_av *av;

	/* cmap handles that correspond to addresses in AV */
	struct util_cmap_handle **handles_av;

	/* Store all cmap handles (inclusive of handles_av) in an indexer.
	 * This allows reverse lookup of the handle using the index. */
	struct indexer handles_idx;

	struct ofi_key_idx key_idx;

	struct dlist_entry peer_list;
	ofi_cmap_free_handle_func free_handle;
	fastlock_t lock;
};

struct util_cmap_handle *ofi_cmap_key2handle(struct util_cmap *cmap, uint64_t key);
void ofi_cmap_update_state(struct util_cmap_handle *handle,
		enum util_cmap_state state);
/* Either fi_addr or addr and addrlen args must be given. */
int ofi_cmap_add_handle(struct util_cmap *cmap, struct util_cmap_handle *handle,
		enum util_cmap_state state, fi_addr_t fi_addr, void *addr,
		size_t addrlen);
struct util_cmap_handle *ofi_cmap_get_handle(struct util_cmap *cmap, fi_addr_t fi_addr);
void ofi_cmap_del_handle(struct util_cmap_handle *handle);
void ofi_cmap_free(struct util_cmap *cmap);
struct util_cmap *ofi_cmap_alloc(struct util_av *av,
		ofi_cmap_free_handle_func free_handle);

/*
 * Poll set
 */
struct util_poll {
	struct fid_poll		poll_fid;
	struct util_domain	*domain;
	struct dlist_entry	fid_list;
	fastlock_t		lock;
	ofi_atomic32_t		ref;
	const struct fi_provider *prov;
};

int fi_poll_create_(const struct fi_provider *prov, struct fid_domain *domain,
		    struct fi_poll_attr *attr, struct fid_poll **pollset);
int fi_poll_create(struct fid_domain *domain, struct fi_poll_attr *attr,
		   struct fid_poll **pollset);


/*
 * Wait set
 */
struct util_wait;
typedef void (*fi_wait_signal_func)(struct util_wait *wait);
typedef int (*fi_wait_try_func)(struct util_wait *wait);

struct util_wait {
	struct fid_wait		wait_fid;
	struct util_fabric	*fabric;
	struct util_poll	*pollset;
	ofi_atomic32_t		ref;
	const struct fi_provider *prov;

	enum fi_wait_obj	wait_obj;
	fi_wait_signal_func	signal;
	fi_wait_try_func	try;
};

int fi_wait_init(struct util_fabric *fabric, struct fi_wait_attr *attr,
		 struct util_wait *wait);
int fi_wait_cleanup(struct util_wait *wait);

struct util_wait_fd {
	struct util_wait	util_wait;
	struct fd_signal	signal;
	fi_epoll_t		epoll_fd;
};

int ofi_wait_fd_open(struct fid_fabric *fabric, struct fi_wait_attr *attr,
		struct fid_wait **waitset);


/*
 * EQ
 */
struct util_eq {
	struct fid_eq		eq_fid;
	struct util_fabric	*fabric;
	struct util_wait	*wait;
	fastlock_t		lock;
	ofi_atomic32_t		ref;
	const struct fi_provider *prov;

	struct slist		list;
	int			internal_wait;
};

struct util_event {
	struct slist_entry	entry;
	int			size;
	int			event;
	int			err;
	uint8_t			data[0];
};

int ofi_eq_create(struct fid_fabric *fabric, struct fi_eq_attr *attr,
		 struct fid_eq **eq_fid, void *context);

/*
 * MR
 */
#define OFI_MR_BASIC_MAP (FI_MR_ALLOCATED | FI_MR_PROV_KEY | FI_MR_VIRT_ADDR)

#define OFI_CHECK_MR_BASIC(mode) ((mode == FI_MR_BASIC) || \
				  ((mode & OFI_MR_BASIC_MAP) == OFI_MR_BASIC_MAP))

#define OFI_CHECK_MR_SCALABLE(mode) (!(mode & OFI_MR_BASIC_MAP))

struct ofi_mr_map {
	const struct fi_provider *prov;
	void			*rbtree;
	uint64_t		key;
	enum fi_mr_mode		mode;
};

/* If the app sets FI_MR_LOCAL, we ignore FI_LOCAL_MR.  So, if the
 * app doesn't set FI_MR_LOCAL, we need to check for FI_LOCAL_MR.
 * The provider is assumed only to set FI_MR_LOCAL correctly.
 */
static inline uint64_t ofi_mr_get_prov_mode(uint32_t version,
					    const struct fi_info *user_info,
					    const struct fi_info *prov_info)
{
	if (FI_VERSION_LT(version, FI_VERSION(1, 5)) ||
	    (user_info->domain_attr &&
	     !(user_info->domain_attr->mr_mode & FI_MR_LOCAL))) {
		return (prov_info->domain_attr->mr_mode & FI_MR_LOCAL) ?
			prov_info->mode | FI_LOCAL_MR : prov_info->mode;
	} else {
		return prov_info->mode;
	}
}

int ofi_mr_map_init(const struct fi_provider *in_prov, int mode,
		    struct ofi_mr_map *map);
void ofi_mr_map_close(struct ofi_mr_map *map);

int ofi_mr_insert(struct ofi_mr_map *map,
		  const struct fi_mr_attr *attr,
		  uint64_t *key, void *context);
int ofi_mr_remove(struct ofi_mr_map *map, uint64_t key);
void *ofi_mr_get(struct ofi_mr_map *map,  uint64_t key);

int ofi_mr_verify(struct ofi_mr_map *map, uintptr_t *io_addr,
		  size_t len, uint64_t key, uint64_t access,
		  void **context);


/*
 * Attributes and capabilities
 */
#define FI_PRIMARY_CAPS	(FI_MSG | FI_RMA | FI_TAGGED | FI_ATOMICS | FI_MULTICAST | \
			 FI_NAMED_RX_CTX | FI_DIRECTED_RECV | \
			 FI_READ | FI_WRITE | FI_RECV | FI_SEND | \
			 FI_REMOTE_READ | FI_REMOTE_WRITE)

#define FI_SECONDARY_CAPS (FI_MULTI_RECV | FI_SOURCE | FI_RMA_EVENT | \
			   FI_SHARED_AV | FI_TRIGGER | FI_FENCE | \
			   FI_LOCAL_COMM | FI_REMOTE_COMM)

int ofi_check_mr_mode(uint32_t api_version, uint32_t prov_mode,
			     uint32_t user_mode);
int ofi_check_fabric_attr(const struct fi_provider *prov,
			  const struct fi_fabric_attr *prov_attr,
			  const struct fi_fabric_attr *user_attr);
int ofi_check_wait_attr(const struct fi_provider *prov,
		        const struct fi_wait_attr *attr);
int ofi_check_domain_attr(const struct fi_provider *prov, uint32_t api_version,
			  const struct fi_domain_attr *prov_attr,
			  const struct fi_domain_attr *user_attr);
int ofi_check_ep_attr(const struct util_prov *util_prov, uint32_t api_version,
		      const struct fi_ep_attr *user_attr);
int ofi_check_cq_attr(const struct fi_provider *prov,
		      const struct fi_cq_attr *attr);
int ofi_check_rx_attr(const struct fi_provider *prov,
		      const struct fi_rx_attr *prov_attr,
		      const struct fi_rx_attr *user_attr, uint64_t info_mode);
int ofi_check_tx_attr(const struct fi_provider *prov,
		      const struct fi_tx_attr *prov_attr,
		      const struct fi_tx_attr *user_attr, uint64_t info_mode);
int ofi_check_info(const struct util_prov *util_prov, uint32_t api_version,
		   const struct fi_info *user_info);
void ofi_alter_info(struct fi_info *info, const struct fi_info *hints,
		    uint32_t api_version);

struct fi_info *ofi_allocinfo_internal(void);
int util_getinfo(const struct util_prov *util_prov, uint32_t version,
		 const char *node, const char *service, uint64_t flags,
		 struct fi_info *hints, struct fi_info **info);


struct fid_list_entry {
	struct dlist_entry	entry;
	struct fid		*fid;
};

int fid_list_insert(struct dlist_entry *fid_list, fastlock_t *lock,
		    struct fid *fid);
void fid_list_remove(struct dlist_entry *fid_list, fastlock_t *lock,
		     struct fid *fid);

void ofi_fabric_insert(struct util_fabric *fabric);
struct util_fabric *ofi_fabric_find(struct util_fabric_info *fabric_info);
void ofi_fabric_remove(struct util_fabric *fabric);

/*
 * Utility Providers
 */

typedef int (*ofi_alter_info_t)(uint32_t version, struct fi_info *src_info,
				struct fi_info *dest_info);

int ofi_get_core_info(uint32_t version, const char *node, const char *service,
		      uint64_t flags, const struct util_prov *util_prov,
		      struct fi_info *util_hints, ofi_alter_info_t info_to_core,
		      struct fi_info **core_info);
int ofix_getinfo(uint32_t version, const char *node, const char *service,
		 uint64_t flags, const struct util_prov *util_prov,
		 struct fi_info *hints, ofi_alter_info_t info_to_core,
		 ofi_alter_info_t info_to_util, struct fi_info **info);
int ofi_get_core_info_fabric(struct fi_fabric_attr *util_attr,
			     struct fi_info **core_info);


#define OFI_NAME_DELIM	';'
#define OFI_UTIL_PREFIX "ofi-"

char *ofi_strdup_append(const char *head, const char *tail);
// char *ofi_strdup_head(const char *str);
// char *ofi_strdup_tail(const char *str);
const char *ofi_util_name(const char *prov_name, size_t *len);
const char *ofi_core_name(const char *prov_name, size_t *len);


int ofi_shm_map(struct util_shm *shm, const char *name, size_t size,
		int readonly, void **mapped);
int ofi_shm_unmap(struct util_shm *shm);

#endif
