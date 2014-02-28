/*
 * generic spi_dma_fragment support
 *
 * Copyright (C) 2014 Martin Sperl
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi-dmafragment.h>

/**
 * spi_merged_dma_fragment_release_fragment_to_cache - transform that
 * release the dma_fragment back to its cache and also return the
 * dma_links that has been hijacked
 */
int spi_merged_dma_fragment_release_fragment_to_cache(
	struct dma_fragment_transform *transform,
	void *data,
	gfp_t gfpflags)
{
	/*struct dma_fragment *fragment = transform->fragment;*/
	struct list_head    *head     = transform->src;
	struct list_head    *tail     = transform->dst;
	struct dma_fragment *frag     = transform->extra;

	/* restore fragment.dma_link_list
	   correct would be walking the list, but it is not efficient...
	   - maybe this should go to lists.h */
	/* first unlink from origin */
	head->prev->next = tail->next;
	tail->next->prev = head->prev;
	/* link ourself to new list */
	head->prev = frag->dma_link_list.next;
	frag->dma_link_list.next = head;
	tail->next = frag->dma_link_list.prev;
	frag->dma_link_list.prev = tail;

	/* now that we have relinked the dma_links
	   to the correct location return the fragment back to cache */
	dma_fragment_cache_return(frag);

	return 0;
}

/**
 * spi_merged_dma_fragment_merge_fragment_cache - merge a
 * dma_fragment from a pool
 * into a spi_merged_dma_fragment
 * @fragment: the fragment cache from which to fetch the fragment
 * @merged: the merged fragment
 */
int spi_merged_dma_fragment_merge_fragment_cache(
	struct dma_fragment_cache *fragmentcache,
	struct spi_merged_dma_fragment *merged,
	int flags,
	gfp_t gfpflags
	)
{
	struct dma_fragment *frag;
	int err = 0;
	struct list_head tmp_copy;

	/* fetch the fragment from dma_fragment_cache */
	frag = dma_fragment_cache_fetch(fragmentcache,gfpflags);
	if (!frag)
		return -ENOMEM;

	/* run the transforms with the merged fragment */
	err = dma_fragment_execute_transforms(
		frag,
		merged,
		gfpflags
		);
	if (err)
		goto error;

	/* "highjack" the fragment dma_list */

	/* first we need a copy of the list as is */
	tmp_copy.next = frag->dma_link_list.next;
	tmp_copy.prev = frag->dma_link_list.prev;

	/* now splice it away */
	list_splice_tail_init(
		&frag->dma_link_list,
		&merged->fragment.dma_link_list
		);

	/* add a transform that will release the fragment back to cache */
	if (! dma_fragment_addnew_transform(
		&spi_merged_dma_fragment_release_fragment_to_cache,
		&merged->fragment,
		tmp_copy.next,
		tmp_copy.prev,
		frag,
		0,gfpflags) ) {
		err = -ENOMEM;
		/* in the error case we need to do this here */
		list_splice_tail_init(
			&frag->dma_link_list,
			&(merged->fragment.dma_link_list)
			);
		goto error;
	}

	/* now link the fragments */
	/* TODO */

	return 0;
error:
	printk(KERN_ERR "spi_merged_dma_fragment_merge_fragment_cache: %i\n",err);

	/* call the transforms for the merged transfer */
	dma_fragment_execute_transforms(
		&merged->fragment,
		NULL,
		gfpflags
		);
	return err;
}
EXPORT_SYMBOL_GPL(spi_merged_dma_fragment_merge_fragment_cache);


static const char *_tab_indent_string = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

static inline const char *_tab_indent(int indent) {
	return &_tab_indent_string[16-min(16,indent)];
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
	)
{
	int i;
	struct dma_fragment_transform *transform;

	dma_fragment_dump(&fragment->fragment,dev,
			tindent,flags,dma_cb_dump);
	tindent++;

	/* dump the individual dma_fragment_transforms */
	dev_printk(KERN_INFO,dev,"%spre-DMA-Transforms:\n",
		_tab_indent(tindent));
	i=0;
	list_for_each_entry(transform,
			&fragment->transform_pre_dma_list,
			transform_list) {
		dev_printk(KERN_INFO,dev,
			"%spre-DMA-Transform %i:\n",
			_tab_indent(tindent+1),
			i++);
		dma_fragment_transform_dump(transform, dev, tindent+2);
	}
	/* dump the individual dma_fragment_transforms */
	dev_printk(KERN_INFO,dev,"%spost-DMA-Transforms:\n",
		_tab_indent(tindent));
	i=0;
	list_for_each_entry(transform,
			&fragment->transform_post_dma_list,
			transform_list) {
		dev_printk(KERN_INFO,dev,
			"%spost-DMA-Transform %i:\n",
			_tab_indent(tindent+1),
			i++);
		dma_fragment_transform_dump(transform, dev, tindent+2);
	}

}
EXPORT_SYMBOL_GPL(spi_merged_dma_fragment_dump);


MODULE_DESCRIPTION("spi specific dma-fragment infrastructure");
MODULE_AUTHOR("Martin Sperl <kernel@martin.sperl.org>");
MODULE_LICENSE("GPL");
