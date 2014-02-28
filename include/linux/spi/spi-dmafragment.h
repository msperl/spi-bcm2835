/*
 * Driver for Broadcom BCM2835 SPI Controllers
 *
 * Copyright (C) 2014 Martin Sperl
 *
 * This driver is inspired by:
 * spi-ath79.c, Copyright (C) 2009-2011 Gabor Juhos <juhosg@openwrt.org>
 * spi-atmel.c, Copyright (C) 2006 Atmel Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __SPI_DMAFRAGMENT_H
#define __SPI_DMAFRAGMENT_H
#include <linux/dma-fragment.h>
#include <linux/spi/spi.h>

#define SPI_OPTIMIZE_VARY_TX_BUF               (1<<0)
#define SPI_OPTIMIZE_VARY_RX_BUF               (1<<1)
#define SPI_OPTIMIZE_VARY_SPEED_HZ             (1<<2)
#define SPI_OPTIMIZE_VARY_DELAY_USECS          (1<<3)
#define SPI_OPTIMIZE_VARY_LENGTH               (1<<4)

struct spi_merged_dma_fragment {
	struct dma_fragment fragment;

	struct list_head transform_pre_dma_list;
	struct list_head transform_post_dma_list;

	/* message for which this is prepared */
	struct spi_message *message;
	/* transient data used during linking of fragments
	 * otherwise undefined
	 */
	struct spi_transfer *transfer;
	struct spi_transfer *last_transfer;
};

static inline void spi_merged_dma_fragment_init(
	struct spi_merged_dma_fragment *frag, size_t size)
{
	dma_fragment_init(&frag->fragment,size);
	INIT_LIST_HEAD(&frag->transform_pre_dma_list);
	INIT_LIST_HEAD(&frag->transform_post_dma_list);
}

static inline struct dma_fragment *spi_merged_dma_fragment_alloc(
	struct device *device,size_t size,gfp_t gfpflags)
{
	struct spi_merged_dma_fragment *frag;
	if ( size < sizeof(*frag) )
		size = sizeof(*frag);
	frag = ( typeof(frag) )
		dma_fragment_alloc(device,
				max(size,sizeof(*frag)),
				gfpflags);
	if ( frag )
		spi_merged_dma_fragment_init(
			frag,
			size);

	return &frag->fragment;
}

static inline void spi_merged_dma_fragment_free(
	struct spi_merged_dma_fragment *frag)
{
	struct dma_fragment_transform *transform;
	/* remove all the dma_fragment_transforms belonging to us */
	while( !list_empty(&frag->transform_pre_dma_list)) {
		transform = list_first_entry(
			&frag->transform_pre_dma_list,
			typeof(*transform),
			transform_list);
		dma_fragment_transform_free(transform);
	}
	/* remove all the dma_fragment_transforms belonging to us */
	while( !list_empty(&frag->transform_post_dma_list)) {
		transform = list_first_entry(
			&frag->transform_post_dma_list,
			typeof(*transform),
			transform_list);
		dma_fragment_transform_free(transform);
	}

	dma_fragment_free((struct dma_fragment *)frag);
}

/**
 * spi_merged_dma_fragment_dump - dump the spi_merged_dma_fragment
 * into a spi_merged_dma_fragment
 * @fragment: the fragment cache from which to fetch the fragment
 * @dev: the devie to use during the dump
 * @tindent: the number of tab indents to add
 * @flags: the flags for dumping the fragment
 * @dma_link_dump: the function which to use to dump the dmablock
 */
void spi_merged_dma_fragment_dump(
	struct spi_merged_dma_fragment *fragment,
	struct device *dev,
	int tindent,
	int flags,
	void (*dma_cb_dump)(struct dma_link *,
			struct device *,int)
	);

static inline void spi_merged_dma_fragment_add_dma_fragment_transform(
	struct spi_merged_dma_fragment *frag,
	struct dma_fragment_transform *transform,
	int post)
{
	if (post)
		list_add_tail(&transform->transform_list,
			&frag->transform_post_dma_list);
	else
		list_add_tail(&transform->transform_list,
			&frag->transform_pre_dma_list);
}

static inline struct dma_fragment_transform *
spi_merged_dma_fragment_addnew_transform(
	struct spi_merged_dma_fragment *merged,
	int (*function)(struct dma_fragment_transform *, void *,gfp_t),
	struct dma_fragment *frag,
	void *src,
	void *dst,
	void *extra,
	int size,
	int post,
	gfp_t gfpflags)
{
	struct dma_fragment_transform *trans;
	trans = dma_fragment_transform_alloc(
			function,
			frag,
			src,dst,extra,
			size,gfpflags
			);
	if (trans)
		spi_merged_dma_fragment_add_dma_fragment_transform(
			merged,trans,post
			);

	return trans;
}

/**
 * spi_merged_dma_fragment_merge_dma_fragment_from_cache - merge a
 * dma_fragment from a pool
 * into a spi_merged_dma_fragment
 * @fragment: the fragment cache from which to fetch the fragment
 * @merged: the merged fragment
 */
int spi_merged_dma_fragment_merge_fragment_cache(
	struct dma_fragment_cache *fragmentcache,
	struct spi_merged_dma_fragment *merged,
	int flags,
	gfp_t gfpflags);

#endif /* __SPI_DMAFRAGMENT_H */
