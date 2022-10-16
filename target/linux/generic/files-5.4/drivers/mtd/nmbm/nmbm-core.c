// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (C) 2021 MediaTek Inc. All Rights Reserved.
 *
 * Author: Weijie Gao <weijie.gao@mediatek.com>
 */

#include "nmbm-private.h"

#include "nmbm-debug.h"

#define NMBM_VER_MAJOR			1
#define NMBM_VER_MINOR			0
#define NMBM_VER			NMBM_VERSION_MAKE(NMBM_VER_MAJOR, \
							  NMBM_VER_MINOR)

#define NMBM_ALIGN(v, a)		(((v) + (a) - 1) & ~((a) - 1))

/*****************************************************************************/
/* Logging related functions */
/*****************************************************************************/

/*
 * nmbm_log_lower - Print log using OS specific routine
 * @nld: NMBM lower device structure
 * @level: log level
 * @fmt: format string
 */
static void nmbm_log_lower(struct nmbm_lower_device *nld,
			   enum nmbm_log_category level, const char *fmt, ...)
{
	va_list ap;

	if (!nld->logprint)
		return;

	va_start(ap, fmt);
	nld->logprint(nld->arg, level, fmt, ap);
	va_end(ap);
}

/*
 * nmbm_log - Print log using OS specific routine
 * @ni: NMBM instance structure
 * @level: log level
 * @fmt: format string
 */
static void nmbm_log(struct nmbm_instance *ni, enum nmbm_log_category level,
		     const char *fmt, ...)
{
	va_list ap;

	if (!ni)
		return;

	if (!ni->lower.logprint || level < ni->log_display_level)
		return;

	va_start(ap, fmt);
	ni->lower.logprint(ni->lower.arg, level, fmt, ap);
	va_end(ap);
}

/*
 * nmbm_set_log_level - Set log display level
 * @ni: NMBM instance structure
 * @level: log display level
 */
enum nmbm_log_category nmbm_set_log_level(struct nmbm_instance *ni,
					  enum nmbm_log_category level)
{
	enum nmbm_log_category old;

	if (!ni)
		return __NMBM_LOG_MAX;

	old = ni->log_display_level;
	ni->log_display_level = level;
	return old;
}

/*
 * nlog_table_creation - Print log of table creation event
 * @ni: NMBM instance structure
 * @main_table: whether the table is main info table
 * @start_ba: start block address of the table
 * @end_ba: block address after the end of the table
 */
static void nlog_table_creation(struct nmbm_instance *ni, bool main_table,
			       uint32_t start_ba, uint32_t end_ba)
{
	if (start_ba == end_ba - 1)
		nlog_info(ni, "%s info table has been written to block %u\n",
			 main_table ? "Main" : "Backup", start_ba);
	else
		nlog_info(ni, "%s info table has been written to block %u-%u\n",
			 main_table ? "Main" : "Backup", start_ba, end_ba - 1);

	nmbm_mark_block_color_info_table(ni, start_ba, end_ba - 1);
}

/*
 * nlog_table_update - Print log of table update event
 * @ni: NMBM instance structure
 * @main_table: whether the table is main info table
 * @start_ba: start block address of the table
 * @end_ba: block address after the end of the table
 */
static void nlog_table_update(struct nmbm_instance *ni, bool main_table,
			     uint32_t start_ba, uint32_t end_ba)
{
	if (start_ba == end_ba - 1)
		nlog_debug(ni, "%s info table has been updated in block %u\n",
			  main_table ? "Main" : "Backup", start_ba);
	else
		nlog_debug(ni, "%s info table has been updated in block %u-%u\n",
			  main_table ? "Main" : "Backup", start_ba, end_ba - 1);

	nmbm_mark_block_color_info_table(ni, start_ba, end_ba - 1);
}

/*
 * nlog_table_found - Print log of table found event
 * @ni: NMBM instance structure
 * @first_table: whether the table is first found info table
 * @write_count: write count of the info table
 * @start_ba: start block address of the table
 * @end_ba: block address after the end of the table
 */
static void nlog_table_found(struct nmbm_instance *ni, bool first_table,
			    uint32_t write_count, uint32_t start_ba,
			    uint32_t end_ba)
{
	if (start_ba == end_ba - 1)
		nlog_info(ni, "%s info table with writecount %u found in block %u\n",
			 first_table ? "First" : "Second", write_count,
			 start_ba);
	else
		nlog_info(ni, "%s info table with writecount %u found in block %u-%u\n",
			 first_table ? "First" : "Second", write_count,
			 start_ba, end_ba - 1);

	nmbm_mark_block_color_info_table(ni, start_ba, end_ba - 1);
}

/*****************************************************************************/
/* Address conversion functions */
/*****************************************************************************/

/*
 * addr2ba - Convert a linear address to block address
 * @ni: NMBM instance structure
 * @addr: Linear address
 */
static uint32_t addr2ba(struct nmbm_instance *ni, uint64_t addr)
{
	return addr >> ni->erasesize_shift;
}

/*
 * ba2addr - Convert a block address to linear address
 * @ni: NMBM instance structure
 * @ba: Block address
 */
static uint64_t ba2addr(struct nmbm_instance *ni, uint32_t ba)
{
	return (uint64_t)ba << ni->erasesize_shift;
}
/*
 * size2blk - Get minimum required blocks for storing specific size of data
 * @ni: NMBM instance structure
 * @size: size for storing
 */
static uint32_t size2blk(struct nmbm_instance *ni, uint64_t size)
{
	return (size + ni->lower.erasesize - 1) >> ni->erasesize_shift;
}

/*****************************************************************************/
/* High level NAND chip APIs */
/*****************************************************************************/

/*
 * nmbm_reset_chip - Reset NAND device
 * @nld: Lower NAND chip structure
 */
static void nmbm_reset_chip(struct nmbm_instance *ni)
{
	if (ni->lower.reset_chip)
		ni->lower.reset_chip(ni->lower.arg);
}

/*
 * nmbm_read_phys_page - Read page with retry
 * @ni: NMBM instance structure
 * @addr: linear address where the data will be read from
 * @data: the main data to be read
 * @oob: the oob data to be read
 * @mode: mode for processing oob data
 *
 * Read a page for at most NMBM_TRY_COUNT times.
 *
 * Return 0 for success, positive value for corrected bitflip count,
 * -EBADMSG for ecc error, other negative values for other errors
 */
static int nmbm_read_phys_page(struct nmbm_instance *ni, uint64_t addr,
			       void *data, void *oob, enum nmbm_oob_mode mode)
{
	int tries, ret;

	for (tries = 0; tries < NMBM_TRY_COUNT; tries++) {
		ret = ni->lower.read_page(ni->lower.arg, addr, data, oob, mode);
		if (ret >= 0)
			return ret;

		nmbm_reset_chip(ni);
	}

	if (ret != -EBADMSG)
		nlog_err(ni, "Page read failed at address 0x%08llx\n", addr);

	return ret;
}

/*
 * nmbm_write_phys_page - Write page with retry
 * @ni: NMBM instance structure
 * @addr: linear address where the data will be written to
 * @data: the main data to be written
 * @oob: the oob data to be written
 * @mode: mode for processing oob data
 *
 * Write a page for at most NMBM_TRY_COUNT times.
 */
static bool nmbm_write_phys_page(struct nmbm_instance *ni, uint64_t addr,
				 const void *data, const void *oob,
				 enum nmbm_oob_mode mode)
{
	int tries, ret;

	if (ni->lower.flags & NMBM_F_READ_ONLY) {
		nlog_err(ni, "%s called with NMBM_F_READ_ONLY set\n", addr);
		return false;
	}

	for (tries = 0; tries < NMBM_TRY_COUNT; tries++) {
		ret = ni->lower.write_page(ni->lower.arg, addr, data, oob, mode);
		if (!ret)
			return true;

		nmbm_reset_chip(ni);
	}

	nlog_err(ni, "Page write failed at address 0x%08llx\n", addr);

	return false;
}

/*
 * nmbm_erase_phys_block - Erase a block with retry
 * @ni: NMBM instance structure
 * @addr: Linear address
 *
 * Erase a block for at most NMBM_TRY_COUNT times.
 */
static bool nmbm_erase_phys_block(struct nmbm_instance *ni, uint64_t addr)
{
	int tries, ret;

	if (ni->lower.flags & NMBM_F_READ_ONLY) {
		nlog_err(ni, "%s called with NMBM_F_READ_ONLY set\n", addr);
		return false;
	}

	for (tries = 0; tries < NMBM_TRY_COUNT; tries++) {
		ret = ni->lower.erase_block(ni->lower.arg, addr);
		if (!ret)
			return true;

		nmbm_reset_chip(ni);
	}

	nlog_err(ni, "Block erasure failed at address 0x%08llx\n", addr);

	return false;
}

/*
 * nmbm_check_bad_phys_block - Check whether a block is marked bad in OOB
 * @ni: NMBM instance structure
 * @ba: block address
 */
static bool nmbm_check_bad_phys_block(struct nmbm_instance *ni, uint32_t ba)
{
	uint64_t addr = ba2addr(ni, ba);
	int ret;

	if (ni->lower.is_bad_block)
		return ni->lower.is_bad_block(ni->lower.arg, addr);

	/* Treat ECC error as read success */
	ret = nmbm_read_phys_page(ni, addr, NULL,
				  ni->page_cache + ni->lower.writesize,
				  NMBM_MODE_RAW);
	if (ret < 0 && ret != -EBADMSG)
		return true;

	return ni->page_cache[ni->lower.writesize] != 0xff;
}

/*
 * nmbm_mark_phys_bad_block - Mark a block bad
 * @ni: NMBM instance structure
 * @addr: Linear address
 */
static int nmbm_mark_phys_bad_block(struct nmbm_instance *ni, uint32_t ba)
{
	uint64_t addr = ba2addr(ni, ba);
	enum nmbm_log_category level;
	uint32_t off;

	if (ni->lower.flags & NMBM_F_READ_ONLY) {
		nlog_err(ni, "%s called with NMBM_F_READ_ONLY set\n", addr);
		return false;
	}

	nlog_info(ni, "Block %u [0x%08llx] will be marked bad\n", ba, addr);

	if (ni->lower.mark_bad_block)
		return ni->lower.mark_bad_block(ni->lower.arg, addr);

	/* Whole page set to 0x00 */
	memset(ni->page_cache, 0, ni->rawpage_size);

	/* Write to all pages within this block, disable all errors */
	level = nmbm_set_log_level(ni, __NMBM_LOG_MAX);

	for (off = 0; off < ni->lower.erasesize; off += ni->lower.writesize) {
		nmbm_write_phys_page(ni, addr + off, ni->page_cache,
				     ni->page_cache + ni->lower.writesize,
				     NMBM_MODE_RAW);
	}

	nmbm_set_log_level(ni, level);

	return 0;
}

/*****************************************************************************/
/* NMBM related functions */
/*****************************************************************************/

/*
 * nmbm_check_header - Check whether a NMBM structure is valid
 * @data: pointer to a NMBM structure with a NMBM header at beginning
 * @size: Size of the buffer pointed by @header
 *
 * The size of the NMBM structure may be larger than NMBM header,
 * e.g. block mapping table and block state table.
 */
static bool nmbm_check_header(const void *data, uint32_t size)
{
	const struct nmbm_header *header = data;
	struct nmbm_header nhdr;
	uint32_t new_checksum;

	/*
	 * Make sure expected structure size is equal or smaller than
	 * buffer size.
	 */
	if (header->size > size)
		return false;

	memcpy(&nhdr, data, sizeof(nhdr));

	nhdr.checksum = 0;
	new_checksum = nmbm_crc32(0, &nhdr, sizeof(nhdr));
	if (header->size > sizeof(nhdr))
		new_checksum = nmbm_crc32(new_checksum,
			(const uint8_t *)data + sizeof(nhdr),
			header->size - sizeof(nhdr));

	if (header->checksum != new_checksum)
		return false;

	return true;
}

/*
 * nmbm_update_checksum - Update checksum of a NMBM structure
 * @header: pointer to a NMBM structure with a NMBM header at beginning
 *
 * The size of the NMBM structure must be specified by @header->size
 */
static void nmbm_update_checksum(struct nmbm_header *header)
{
	header->checksum = 0;
	header->checksum = nmbm_crc32(0, header, header->size);
}

/*
 * nmbm_get_spare_block_count - Calculate number of blocks should be reserved
 * @block_count: number of blocks of data
 *
 * Calculate number of blocks should be reserved for data
 */
static uint32_t nmbm_get_spare_block_count(uint32_t block_count)
{
	uint32_t val;

	val = (block_count + NMBM_SPARE_BLOCK_DIV / 2) / NMBM_SPARE_BLOCK_DIV;
	val *= NMBM_SPARE_BLOCK_MULTI;

	if (val < NMBM_SPARE_BLOCK_MIN)
		val = NMBM_SPARE_BLOCK_MIN;

	return val;
}

/*
 * nmbm_get_block_state_raw - Get state of a block from raw block state table
 * @block_state: pointer to raw block state table (bitmap)
 * @ba: block address
 */
static uint32_t nmbm_get_block_state_raw(nmbm_bitmap_t *block_state,
					 uint32_t ba)
{
	uint32_t unit, shift;

	unit = ba / NMBM_BITMAP_BLOCKS_PER_UNIT;
	shift = (ba % NMBM_BITMAP_BLOCKS_PER_UNIT) * NMBM_BITMAP_BITS_PER_BLOCK;

	return (block_state[unit] >> shift) & BLOCK_ST_MASK;
}

/*
 * nmbm_get_block_state - Get state of a block from block state table
 * @ni: NMBM instance structure
 * @ba: block address
 */
static uint32_t nmbm_get_block_state(struct nmbm_instance *ni, uint32_t ba)
{
	return nmbm_get_block_state_raw(ni->block_state, ba);
}

/*
 * nmbm_set_block_state - Set state of a block to block state table
 * @ni: NMBM instance structure
 * @ba: block address
 * @state: block state
 *
 * Set state of a block. If the block state changed, ni->block_state_changed
 * will be increased.
 */
static bool nmbm_set_block_state(struct nmbm_instance *ni, uint32_t ba,
				 uint32_t state)
{
	uint32_t unit, shift, orig;
	nmbm_bitmap_t uv;

	unit = ba / NMBM_BITMAP_BLOCKS_PER_UNIT;
	shift = (ba % NMBM_BITMAP_BLOCKS_PER_UNIT) * NMBM_BITMAP_BITS_PER_BLOCK;

	orig = (ni->block_state[unit] >> shift) & BLOCK_ST_MASK;
	state &= BLOCK_ST_MASK;

	uv = ni->block_state[unit] & (~(BLOCK_ST_MASK << shift));
	uv |= state << shift;
	ni->block_state[unit] = uv;

	if (state == BLOCK_ST_BAD)
		nmbm_mark_block_color_bad(ni, ba);

	if (orig != state) {
		ni->block_state_changed++;
		return true;
	}

	return false;
}

/*
 * nmbm_block_walk_asc - Skip specified number of good blocks, ascending addr.
 * @ni: NMBM instance structure
 * @ba: start physical block address
 * @nba: return physical block address after walk
 * @count: number of good blocks to be skipped
 * @limit: highest block address allowed for walking
 *
 * Start from @ba, skipping any bad blocks, counting @count good blocks, and
 * return the next good block address.
 *
 * If no enough good blocks counted while @limit reached, false will be returned.
 *
 * If @count == 0, nearest good block address will be returned.
 * @limit is not counted in walking.
 */
static bool nmbm_block_walk_asc(struct nmbm_instance *ni, uint32_t ba,
				uint32_t *nba, uint32_t count,
				uint32_t limit)
{
	int32_t nblock = count;

	if (limit >= ni->block_count)
		limit = ni->block_count - 1;

	while (ba < limit) {
		if (nmbm_get_block_state(ni, ba) == BLOCK_ST_GOOD)
			nblock--;

		if (nblock < 0) {
			*nba = ba;
			return true;
		}

		ba++;
	}

	return false;
}

/*
 * nmbm_block_walk_desc - Skip specified number of good blocks, descending addr
 * @ni: NMBM instance structure
 * @ba: start physical block address
 * @nba: return physical block address after walk
 * @count: number of good blocks to be skipped
 * @limit: lowest block address allowed for walking
 *
 * Start from @ba, skipping any bad blocks, counting @count good blocks, and
 * return the next good block address.
 *
 * If no enough good blocks counted while @limit reached, false will be returned.
 *
 * If @count == 0, nearest good block address will be returned.
 * @limit is not counted in walking.
 */
static bool nmbm_block_walk_desc(struct nmbm_instance *ni, uint32_t ba,
				 uint32_t *nba, uint32_t count, uint32_t limit)
{
	int32_t nblock = count;

	if (limit >= ni->block_count)
		limit = ni->block_count - 1;

	while (ba > limit) {
		if (nmbm_get_block_state(ni, ba) == BLOCK_ST_GOOD)
			nblock--;

		if (nblock < 0) {
			*nba = ba;
			return true;
		}

		ba--;
	}

	return false;
}

/*
 * nmbm_block_walk - Skip specified number of good blocks from curr. block addr
 * @ni: NMBM instance structure
 * @ascending: whether to walk ascending
 * @ba: start physical block address
 * @nba: return physical block address after walk
 * @count: number of good blocks to be skipped
 * @limit: highest/lowest block address allowed for walking
 *
 * Start from @ba, skipping any bad blocks, counting @count good blocks, and
 * return the next good block address.
 *
 * If no enough good blocks counted while @limit reached, false will be returned.
 *
 * If @count == 0, nearest good block address will be returned.
 * @limit can be set to negative if no limit required.
 * @limit is not counted in walking.
 */
static bool nmbm_block_walk(struct nmbm_instance *ni, bool ascending,
			    uint32_t ba, uint32_t *nba, int32_t count,
			    int32_t limit)
{
	if (ascending)
		return nmbm_block_walk_asc(ni, ba, nba, count, limit);

	return nmbm_block_walk_desc(ni, ba, nba, count, limit);
}

/*
 * nmbm_scan_badblocks - Scan and record all bad blocks
 * @ni: NMBM instance structure
 *
 * Scan the entire lower NAND chip and record all bad blocks in to block state
 * table.
 */
static void nmbm_scan_badblocks(struct nmbm_instance *ni)
{
	uint32_t ba;

	for (ba = 0; ba < ni->block_count; ba++) {
		if (nmbm_check_bad_phys_block(ni, ba)) {
			nmbm_set_block_state(ni, ba, BLOCK_ST_BAD);
			nlog_info(ni, "Bad block %u [0x%08llx]\n", ba,
				 ba2addr(ni, ba));
		}
	}
}

/*
 * nmbm_build_mapping_table - Build initial block mapping table
 * @ni: NMBM instance structure
 *
 * The initial mapping table will be compatible with the stratage of
 * factory production.
 */
static void nmbm_build_mapping_table(struct nmbm_instance *ni)
{
	uint32_t pb, lb;

	for (pb = 0, lb = 0; pb < ni->mgmt_start_ba; pb++) {
		if (nmbm_get_block_state(ni, pb) == BLOCK_ST_BAD)
			continue;

		/* Always map to the next good block */
		ni->block_mapping[lb++] = pb;
	}

	ni->data_block_count = lb;

	/* Unusable/Management blocks */
	for (pb = lb; pb < ni->block_count; pb++)
		ni->block_mapping[pb] = -1;
}

/*
 * nmbm_erase_block_and_check - Erase a block and check its usability
 * @ni: NMBM instance structure
 * @ba: block address to be erased
 *
 * Erase a block anc check its usability
 *
 * Return true if the block is usable, false if erasure failure or the block
 * has too many bitflips.
 */
static bool nmbm_erase_block_and_check(struct nmbm_instance *ni, uint32_t ba)
{
	uint64_t addr, off;
	bool success;
	int ret;

	success = nmbm_erase_phys_block(ni, ba2addr(ni, ba));
	if (!success)
		return false;

	if (!(ni->lower.flags & NMBM_F_EMPTY_PAGE_ECC_OK))
		return true;

	/* Check every page to make sure there aren't too many bitflips */

	addr = ba2addr(ni, ba);

	for (off = 0; off < ni->lower.erasesize; off += ni->lower.writesize) {
		WATCHDOG_RESET();

		ret = nmbm_read_phys_page(ni, addr + off, ni->page_cache, NULL,
					  NMBM_MODE_PLACE_OOB);
		if (ret == -EBADMSG) {
			/*
			 * NMBM_F_EMPTY_PAGE_ECC_OK means the empty page is
			 * still protected by ECC. So reading pages with ECC
			 * enabled and -EBADMSG means there are too many
			 * bitflips that can't be recovered, and the block
			 * containing the page should be marked bad.
			 */
			nlog_err(ni,
				 "Too many bitflips in empty page at 0x%llx\n",
				 addr + off);
			return false;
		}
	}

	return true;
}

/*
 * nmbm_erase_range - Erase a range of blocks
 * @ni: NMBM instance structure
 * @ba: block address where the erasure will start
 * @limit: top block address allowed for erasure
 *
 * Erase blocks within the specific range. Newly-found bad blocks will be
 * marked.
 *
 * @limit is not counted into the allowed erasure address.
 */
static void nmbm_erase_range(struct nmbm_instance *ni, uint32_t ba,
			     uint32_t limit)
{
	bool success;

	while (ba < limit) {
		WATCHDOG_RESET();

		if (nmbm_get_block_state(ni, ba) != BLOCK_ST_GOOD)
			goto next_block;

		/* Insurance to detect unexpected bad block marked by user */
		if (nmbm_check_bad_phys_block(ni, ba)) {
			nmbm_set_block_state(ni, ba, BLOCK_ST_BAD);
			goto next_block;
		}

		success = nmbm_erase_block_and_check(ni, ba);
		if (success)
			goto next_block;

		nmbm_mark_phys_bad_block(ni, ba);
		nmbm_set_block_state(ni, ba, BLOCK_ST_BAD);

	next_block:
		ba++;
	}
}

/*
 * nmbm_write_repeated_data - Write critical data to a block with retry
 * @ni: NMBM instance structure
 * @ba: block address where the data will be written to
 * @data: the data to be written
 * @size: size of the data
 *
 * Write data to every page of the block. Success only if all pages within
 * this block have been successfully written.
 *
 * Make sure data size is not bigger than one page.
 *
 * This function will write and verify every page for at most
 * NMBM_TRY_COUNT times.
 */
static bool nmbm_write_repeated_data(struct nmbm_instance *ni, uint32_t ba,
				     const void *data, uint32_t size)
{
	uint64_t addr, off;
	bool success;
	int ret;

	if (size > ni->lower.writesize)
		return false;

	addr = ba2addr(ni, ba);

	for (off = 0; off < ni->lower.erasesize; off += ni->lower.writesize) {
		WATCHDOG_RESET();

		/* Prepare page data. fill 0xff to unused region */
		memcpy(ni->page_cache, data, size);
		memset(ni->page_cache + size, 0xff, ni->rawpage_size - size);

		success = nmbm_write_phys_page(ni, addr + off, ni->page_cache,
					       NULL, NMBM_MODE_PLACE_OOB);
		if (!success)
			return false;

		/* Verify the data just written. ECC error indicates failure */
		ret = nmbm_read_phys_page(ni, addr + off, ni->page_cache, NULL,
					  NMBM_MODE_PLACE_OOB);
		if (ret < 0)
			return false;

		if (memcmp(ni->page_cache, data, size))
			return false;
	}

	return true;
}

/*
 * nmbm_write_signature - Write signature to NAND chip
 * @ni: NMBM instance structure
 * @limit: top block address allowed for writing
 * @signature: the signature to be written
 * @signature_ba: the actual block address where signature is written to
 *
 * Write signature within a specific range, from chip bottom to limit.
 * At most one block will be written.
 *
 * @limit is not counted into the allowed write address.
 */
static bool nmbm_write_signature(struct nmbm_instance *ni, uint32_t limit,
				 const struct nmbm_signature *signature,
				 uint32_t *signature_ba)
{
	uint32_t ba = ni->block_count - 1;
	bool success;

	while (ba > limit) {
		WATCHDOG_RESET();

		if (nmbm_get_block_state(ni, ba) != BLOCK_ST_GOOD)
			goto next_block;

		/* Insurance to detect unexpected bad block marked by user */
		if (nmbm_check_bad_phys_block(ni, ba)) {
			nmbm_set_block_state(ni, ba, BLOCK_ST_BAD);
			goto next_block;
		}

		success = nmbm_erase_block_and_check(ni, ba);
		if (!success)
			goto skip_bad_block;

		success = nmbm_write_repeated_data(ni, ba, signature,
						   sizeof(*signature));
		if (success) {
			*signature_ba = ba;
			return true;
		}

	skip_bad_block:
		nmbm_mark_phys_bad_block(ni, ba);
		nmbm_set_block_state(ni, ba, BLOCK_ST_BAD);

	next_block:
		ba--;
	};

	return false;
}

/*
 * nmbn_read_data - Read data
 * @ni: NMBM instance structure
 * @addr: linear address where the data will be read from
 * @data: the data to be read
 * @size: the size of data
 *
 * Read data range.
 * Every page will be tried for at most NMBM_TRY_COUNT times.
 *
 * Return 0 for success, positive value for corrected bitflip count,
 * -EBADMSG for ecc error, other negative values for other errors
 */
static int nmbn_read_data(struct nmbm_instance *ni, uint64_t addr, void *data,
			  uint32_t size)
{
	uint64_t off = addr;
	uint8_t *ptr = data;
	uint32_t sizeremain = size, chunksize, leading;
	int ret;

	while (sizeremain) {
		WATCHDOG_RESET();

		leading = off & ni->writesize_mask;
		chunksize = ni->lower.writesize - leading;
		if (chunksize > sizeremain)
			chunksize = sizeremain;

		if (chunksize == ni->lower.writesize) {
			ret = nmbm_read_phys_page(ni, off - leading, ptr, NULL,
						  NMBM_MODE_PLACE_OOB);
			if (ret < 0)
				return ret;
		} else {
			ret = nmbm_read_phys_page(ni, off - leading,
						  ni->page_cache, NULL,
						  NMBM_MODE_PLACE_OOB);
			if (ret < 0)
				return ret;

			memcpy(ptr, ni->page_cache + leading, chunksize);
		}

		off += chunksize;
		ptr += chunksize;
		sizeremain -= chunksize;
	}

	return 0;
}

/*
 * nmbn_write_verify_data - Write data with validation
 * @ni: NMBM instance structure
 * @addr: linear address where the data will be written to
 * @data: the data to be written
 * @size: the size of data
 *
 * Write data and verify.
 * Every page will be tried for at most NMBM_TRY_COUNT times.
 */
static bool nmbn_write_verify_data(struct nmbm_instance *ni, uint64_t addr,
				   const void *data, uint32_t size)
{
	uint64_t off = addr;
	const uint8_t *ptr = data;
	uint32_t sizeremain = size, chunksize, leading;
	bool success;
	int ret;

	while (sizeremain) {
		WATCHDOG_RESET();

		leading = off & ni->writesize_mask;
		chunksize = ni->lower.writesize - leading;
		if (chunksize > sizeremain)
			chunksize = sizeremain;

		/* Prepare page data. fill 0xff to unused region */
		memset(ni->page_cache, 0xff, ni->rawpage_size);
		memcpy(ni->page_cache + leading, ptr, chunksize);

		success = nmbm_write_phys_page(ni, off - leading,
					       ni->page_cache, NULL,
					       NMBM_MODE_PLACE_OOB);
		if (!success)
			return false;

		/* Verify the data just written. ECC error indicates failure */
		ret = nmbm_read_phys_page(ni, off - leading, ni->page_cache,
					  NULL, NMBM_MODE_PLACE_OOB);
		if (ret < 0)
			return false;

		if (memcmp(ni->page_cache + leading, ptr, chunksize))
			return false;

		off += chunksize;
		ptr += chunksize;
		sizeremain -= chunksize;
	}

	return true;
}

/*
 * nmbm_write_mgmt_range - Write management data into NAND within a range
 * @ni: NMBM instance structure
 * @addr: preferred start block address for writing
 * @limit: highest block address allowed for writing
 * @data: the data to be written
 * @size: the size of data
 * @actual_start_ba: actual start block address of data
 * @actual_end_ba: block address after the end of data
 *
 * @limit is not counted into the allowed write address.
 */
static bool nmbm_write_mgmt_range(struct nmbm_instance *ni, uint32_t ba,
				  uint32_t limit, const void *data,
				  uint32_t size, uint32_t *actual_start_ba,
				  uint32_t *actual_end_ba)
{
	const uint8_t *ptr = data;
	uint32_t sizeremain = size, chunksize;
	bool success;

	while (sizeremain && ba < limit) {
		WATCHDOG_RESET();

		chunksize = sizeremain;
		if (chunksize > ni->lower.erasesize)
			chunksize = ni->lower.erasesize;

		if (nmbm_get_block_state(ni, ba) != BLOCK_ST_GOOD)
			goto next_block;

		/* Insurance to detect unexpected bad block marked by user */
		if (nmbm_check_bad_phys_block(ni, ba)) {
			nmbm_set_block_state(ni, ba, BLOCK_ST_BAD);
			goto next_block;
		}

		success = nmbm_erase_block_and_check(ni, ba);
		if (!success)
			goto skip_bad_block;

		success = nmbn_write_verify_data(ni, ba2addr(ni, ba), ptr,
						 chunksize);
		if (!success)
			goto skip_bad_block;

		if (sizeremain == size)
			*actual_start_ba = ba;

		ptr += chunksize;
		sizeremain -= chunksize;

		goto next_block;

	skip_bad_block:
		nmbm_mark_phys_bad_block(ni, ba);
		nmbm_set_block_state(ni, ba, BLOCK_ST_BAD);

	next_block:
		ba++;
	}

	if (sizeremain)
		return false;

	*actual_end_ba = ba;

	return true;
}

/*
 * nmbm_generate_info_table_cache - Generate info table cache data
 * @ni: NMBM instance structure
 *
 * Generate info table cache data to be written into flash.
 */
static bool nmbm_generate_info_table_cache(struct nmbm_instance *ni)
{
	bool changed = false;

	memset(ni->info_table_cache, 0xff, ni->info_table_size);

	memcpy(ni->info_table_cache + ni->info_table.state_table_off,
	       ni->block_state, ni->state_table_size);

	memcpy(ni->info_table_cache + ni->info_table.mapping_table_off,
		ni->block_mapping, ni->mapping_table_size);

	ni->info_table.header.magic = NMBM_MAGIC_INFO_TABLE;
	ni->info_table.header.version = NMBM_VER;
	ni->info_table.header.size = ni->info_table_size;

	if (ni->block_state_changed || ni->block_mapping_changed) {
		ni->info_table.write_count++;
		changed = true;
	}

	memcpy(ni->info_table_cache, &ni->info_table, sizeof(ni->info_table));

	nmbm_update_checksum((struct nmbm_header *)ni->info_table_cache);

	return changed;
}

/*
 * nmbm_write_info_table - Write info table into NAND within a range
 * @ni: NMBM instance structure
 * @ba: preferred start block address for writing
 * @limit: highest block address allowed for writing
 * @actual_start_ba: actual start block address of info table
 * @actual_end_ba: block address after the end of info table
 *
 * @limit is counted into the allowed write address.
 */
static bool nmbm_write_info_table(struct nmbm_instance *ni, uint32_t ba,
				  uint32_t limit, uint32_t *actual_start_ba,
				  uint32_t *actual_end_ba)
{
	return nmbm_write_mgmt_range(ni, ba, limit, ni->info_table_cache,
				     ni->info_table_size, actual_start_ba,
				     actual_end_ba);
}

/*
 * nmbm_mark_tables_clean - Mark info table `clean'
 * @ni: NMBM instance structure
 */
static void nmbm_mark_tables_clean(struct nmbm_instance *ni)
{
	ni->block_state_changed = 0;
	ni->block_mapping_changed = 0;
}

/*
 * nmbm_try_reserve_blocks - Reserve blocks with compromisation
 * @ni: NMBM instance structure
 * @ba: start physical block address
 * @nba: return physical block address after reservation
 * @count: number of good blocks to be skipped
 * @min_count: minimum number of good blocks to be skipped
 * @limit: highest/lowest block address allowed for walking
 *
 * Reserve specific blocks. If failed, try to reserve as many as possible.
 */
static bool nmbm_try_reserve_blocks(struct nmbm_instance *ni, uint32_t ba,
				    uint32_t *nba, uint32_t count,
				    int32_t min_count, int32_t limit)
{
	int32_t nblocks = count;
	bool success;

	while (nblocks >= min_count) {
		success = nmbm_block_walk(ni, true, ba, nba, nblocks, limit);
		if (success)
			return true;

		nblocks--;
	}

	return false;
}

/*
 * nmbm_rebuild_info_table - Build main & backup info table from scratch
 * @ni: NMBM instance structure
 * @allow_no_gap: allow no spare blocks between two tables
 */
static bool nmbm_rebuild_info_table(struct nmbm_instance *ni)
{
	uint32_t table_start_ba, table_end_ba, next_start_ba;
	uint32_t main_table_end_ba;
	bool success;

	/* Set initial value */
	ni->main_table_ba = 0;
	ni->backup_table_ba = 0;
	ni->mapping_blocks_ba = ni->mapping_blocks_top_ba;

	/* Write main table */
	success = nmbm_write_info_table(ni, ni->mgmt_start_ba,
					ni->mapping_blocks_top_ba,
					&table_start_ba, &table_end_ba);
	if (!success) {
		/* Failed to write main table, data will be lost */
		nlog_emerg(ni, "Unable to write at least one info table!\n");
		nlog_emerg(ni, "Please save your data before power off!\n");
		ni->protected = 1;
		return false;
	}

	/* Main info table is successfully written, record its offset */
	ni->main_table_ba = table_start_ba;
	main_table_end_ba = table_end_ba;

	/* Adjust mapping_blocks_ba */
	ni->mapping_blocks_ba = table_end_ba;

	nmbm_mark_tables_clean(ni);

	nlog_table_creation(ni, true, table_start_ba, table_end_ba);

	/* Reserve spare blocks for main info table. */
	success = nmbm_try_reserve_blocks(ni, table_end_ba,
					  &next_start_ba,
					  ni->info_table_spare_blocks, 0,
					  ni->mapping_blocks_top_ba -
					  size2blk(ni, ni->info_table_size));
	if (!success) {
		/* There is no spare block. */
		nlog_debug(ni, "No room for backup info table\n");
		return true;
	}

	/* Write backup info table. */
	success = nmbm_write_info_table(ni, next_start_ba,
					ni->mapping_blocks_top_ba,
					&table_start_ba, &table_end_ba);
	if (!success) {
		/* There is no enough blocks for backup table. */
		nlog_debug(ni, "No room for backup info table\n");
		return true;
	}

	/* Backup table is successfully written, record its offset */
	ni->backup_table_ba = table_start_ba;

	/* Adjust mapping_blocks_off */
	ni->mapping_blocks_ba = table_end_ba;

	/* Erase spare blocks of main table to clean possible interference data */
	nmbm_erase_range(ni, main_table_end_ba, ni->backup_table_ba);

	nlog_table_creation(ni, false, table_start_ba, table_end_ba);

	return true;
}

/*
 * nmbm_rescue_single_info_table - Rescue when there is only one info table
 * @ni: NMBM instance structure
 *
 * This function is called when there is only one info table exists.
 * This function may fail if we can't write new info table
 */
static bool nmbm_rescue_single_info_table(struct nmbm_instance *ni)
{
	uint32_t table_start_ba, table_end_ba, write_ba;
	bool success;

	/* Try to write new info table in front of existing table */
	success = nmbm_write_info_table(ni, ni->mgmt_start_ba,
					ni->main_table_ba,
					&table_start_ba,
					&table_end_ba);
	if (success) {
		/*
		 * New table becomes the main table, existing table becomes
		 * the backup table.
		 */
		ni->backup_table_ba = ni->main_table_ba;
		ni->main_table_ba = table_start_ba;

		nmbm_mark_tables_clean(ni);

		/* Erase spare blocks of main table to clean possible interference data */
		nmbm_erase_range(ni, table_end_ba, ni->backup_table_ba);

		nlog_table_creation(ni, true, table_start_ba, table_end_ba);

		return true;
	}

	/* Try to reserve spare blocks for existing table */
	success = nmbm_try_reserve_blocks(ni, ni->mapping_blocks_ba, &write_ba,
					  ni->info_table_spare_blocks, 0,
					  ni->mapping_blocks_top_ba -
					  size2blk(ni, ni->info_table_size));
	if (!success) {
		nlog_warn(ni, "Failed to rescue single info table\n");
		return false;
	}

	/* Try to write new info table next to the existing table */
	while (write_ba >= ni->mapping_blocks_ba) {
		WATCHDOG_RESET();

		success = nmbm_write_info_table(ni, write_ba,
						ni->mapping_blocks_top_ba,
						&table_start_ba,
						&table_end_ba);
		if (success)
			break;

		write_ba--;
	}

	if (success) {
		/* Erase spare blocks of main table to clean possible interference data */
		nmbm_erase_range(ni, ni->mapping_blocks_ba, table_start_ba);

		/* New table becomes the backup table */
		ni->backup_table_ba = table_start_ba;
		ni->mapping_blocks_ba = table_end_ba;

		nmbm_mark_tables_clean(ni);

		nlog_table_creation(ni, false, table_start_ba, table_end_ba);

		return true;
	}

	nlog_warn(ni, "Failed to rescue single info table\n");
	return false;
}

/*
 * nmbm_update_single_info_table - Update specific one info table
 * @ni: NMBM instance structure
 */
static bool nmbm_update_single_info_table(struct nmbm_instance *ni,
					  bool update_main_table)
{
	uint32_t write_start_ba, write_limit, table_start_ba, table_end_ba;
	bool success;

	/* Determine the write range */
	if (update_main_table) {
		write_start_ba = ni->main_table_ba;
		write_limit = ni->backup_table_ba;
	} else {
		write_start_ba = ni->backup_table_ba;
		write_limit = ni->mapping_blocks_top_ba;
	}

	nmbm_mark_block_color_mgmt(ni, write_start_ba, write_limit - 1);

	success = nmbm_write_info_table(ni, write_start_ba, write_limit,
					&table_start_ba, &table_end_ba);
	if (success) {
		if (update_main_table) {
			ni->main_table_ba = table_start_ba;
		} else {
			ni->backup_table_ba = table_start_ba;
			ni->mapping_blocks_ba = table_end_ba;
		}

		nmbm_mark_tables_clean(ni);

		nlog_table_update(ni, update_main_table, table_start_ba,
				 table_end_ba);

		return true;
	}

	if (update_main_table) {
		/*
		 * If failed to update main table, make backup table the new
		 * main table, and call nmbm_rescue_single_info_table()
		 */
		nlog_warn(ni, "Unable to update %s info table\n",
			 update_main_table ? "Main" : "Backup");

		ni->main_table_ba = ni->backup_table_ba;
		ni->backup_table_ba = 0;
		return nmbm_rescue_single_info_table(ni);
	}

	/* Only one table left */
	ni->mapping_blocks_ba = ni->backup_table_ba;
	ni->backup_table_ba = 0;

	return false;
}

/*
 * nmbm_rescue_main_info_table - Rescue when failed to write main info table
 * @ni: NMBM instance structure
 *
 * This function is called when main info table failed to be written, and
 *    backup info table exists.
 */
static bool nmbm_rescue_main_info_table(struct nmbm_instance *ni)
{
	uint32_t tmp_table_start_ba, tmp_table_end_ba, main_table_start_ba;
	uint32_t main_table_end_ba, write_ba;
	uint32_t info_table_erasesize = size2blk(ni, ni->info_table_size);
	bool success;

	/* Try to reserve spare blocks for existing backup info table */
	success = nmbm_try_reserve_blocks(ni, ni->mapping_blocks_ba, &write_ba,
					  ni->info_table_spare_blocks, 0,
					  ni->mapping_blocks_top_ba -
					  info_table_erasesize);
	if (!success) {
		/* There is no spare block. Backup info table becomes the main table. */
		nlog_err(ni, "No room for temporary info table\n");
		ni->main_table_ba = ni->backup_table_ba;
		ni->backup_table_ba = 0;
		return true;
	}

	/* Try to write temporary info table into spare unmapped blocks */
	while (write_ba >= ni->mapping_blocks_ba) {
		WATCHDOG_RESET();

		success = nmbm_write_info_table(ni, write_ba,
						ni->mapping_blocks_top_ba,
						&tmp_table_start_ba,
						&tmp_table_end_ba);
		if (success)
			break;

		write_ba--;
	}

	if (!success) {
		/* Backup info table becomes the main table */
		nlog_err(ni, "Failed to update main info table\n");
		ni->main_table_ba = ni->backup_table_ba;
		ni->backup_table_ba = 0;
		return true;
	}

	/* Adjust mapping_blocks_off */
	ni->mapping_blocks_ba = tmp_table_end_ba;

	nmbm_mark_block_color_mgmt(ni, ni->backup_table_ba,
				   tmp_table_end_ba - 1);

	/*
	 * Now write main info table at the beginning of management area.
	 * This operation will generally destroy the original backup info
	 * table.
	 */
	success = nmbm_write_info_table(ni, ni->mgmt_start_ba,
					tmp_table_start_ba,
					&main_table_start_ba,
					&main_table_end_ba);
	if (!success) {
		/* Temporary info table becomes the main table */
		ni->main_table_ba = tmp_table_start_ba;
		ni->backup_table_ba = 0;

		nmbm_mark_tables_clean(ni);

		nlog_err(ni, "Failed to update main info table\n");
		nmbm_mark_block_color_info_table(ni, tmp_table_start_ba,
						 tmp_table_end_ba - 1);

		return true;
	}

	/* Main info table has been successfully written, record its offset */
	ni->main_table_ba = main_table_start_ba;

	nmbm_mark_tables_clean(ni);

	nlog_table_creation(ni, true, main_table_start_ba, main_table_end_ba);

	/*
	 * Temporary info table becomes the new backup info table if it's
	 * not overwritten.
	 */
	if (main_table_end_ba <= tmp_table_start_ba) {
		ni->backup_table_ba = tmp_table_start_ba;

		nlog_table_creation(ni, false, tmp_table_start_ba,
				   tmp_table_end_ba);

		return true;
	}

	/* Adjust mapping_blocks_off */
	ni->mapping_blocks_ba = main_table_end_ba;

	/* Try to reserve spare blocks for new main info table */
	success = nmbm_try_reserve_blocks(ni, main_table_end_ba, &write_ba,
					  ni->info_table_spare_blocks, 0,
					  ni->mapping_blocks_top_ba -
					  info_table_erasesize);
	if (!success) {
		/* There is no spare block. Only main table exists. */
		nlog_err(ni, "No room for backup info table\n");
		ni->backup_table_ba = 0;
		return true;
	}

	/* Write new backup info table. */
	while (write_ba >= main_table_end_ba) {
		WATCHDOG_RESET();

		success = nmbm_write_info_table(ni, write_ba,
						ni->mapping_blocks_top_ba,
						&tmp_table_start_ba,
						&tmp_table_end_ba);
		if (success)
			break;

		write_ba--;
	}

	if (!success) {
		nlog_err(ni, "No room for backup info table\n");
		ni->backup_table_ba = 0;
		return true;
	}

	/* Backup info table has been successfully written, record its offset */
	ni->backup_table_ba = tmp_table_start_ba;

	/* Adjust mapping_blocks_off */
	ni->mapping_blocks_ba = tmp_table_end_ba;

	/* Erase spare blocks of main table to clean possible interference data */
	nmbm_erase_range(ni, main_table_end_ba, ni->backup_table_ba);

	nlog_table_creation(ni, false, tmp_table_start_ba, tmp_table_end_ba);

	return true;
}

/*
 * nmbm_update_info_table_once - Update info table once
 * @ni: NMBM instance structure
 * @force: force update
 *
 * Update both main and backup info table. Return true if at least one info
 * table has been successfully written.
 * This function only try to update info table once regard less of the result.
 */
static bool nmbm_update_info_table_once(struct nmbm_instance *ni, bool force)
{
	uint32_t table_start_ba, table_end_ba;
	uint32_t main_table_limit;
	bool success;

	/* Do nothing if there is no change */
	if (!nmbm_generate_info_table_cache(ni) && !force)
		return true;

	/* Check whether both two tables exist */
	if (!ni->backup_table_ba) {
		main_table_limit = ni->mapping_blocks_top_ba;
		goto write_main_table;
	}

	nmbm_mark_block_color_mgmt(ni, ni->backup_table_ba,
				   ni->mapping_blocks_ba - 1);

	/*
	 * Write backup info table in its current range.
	 * Note that limit is set to mapping_blocks_top_off to provide as many
	 * spare blocks as possible for the backup table. If at last
	 * unmapped blocks are used by backup table, mapping_blocks_off will
	 * be adjusted.
	 */
	success = nmbm_write_info_table(ni, ni->backup_table_ba,
					ni->mapping_blocks_top_ba,
					&table_start_ba, &table_end_ba);
	if (!success) {
		/*
		 * There is nothing to do if failed to write backup table.
		 * Write the main table now.
		 */
		nlog_err(ni, "No room for backup table\n");
		ni->mapping_blocks_ba = ni->backup_table_ba;
		ni->backup_table_ba = 0;
		main_table_limit = ni->mapping_blocks_top_ba;
		goto write_main_table;
	}

	/* Backup table is successfully written, record its offset */
	ni->backup_table_ba = table_start_ba;

	/* Adjust mapping_blocks_off */
	ni->mapping_blocks_ba = table_end_ba;

	nmbm_mark_tables_clean(ni);

	/* The normal limit of main table */
	main_table_limit = ni->backup_table_ba;

	nlog_table_update(ni, false, table_start_ba, table_end_ba);

write_main_table:
	if (!ni->main_table_ba)
		goto rebuild_tables;

	if (!ni->backup_table_ba)
		nmbm_mark_block_color_mgmt(ni, ni->mgmt_start_ba,
					   ni->mapping_blocks_ba - 1);
	else
		nmbm_mark_block_color_mgmt(ni, ni->mgmt_start_ba,
					   ni->backup_table_ba - 1);

	/* Write main info table in its current range */
	success = nmbm_write_info_table(ni, ni->main_table_ba,
					main_table_limit, &table_start_ba,
					&table_end_ba);
	if (!success) {
		/* If failed to write main table, go rescue procedure */
		if (!ni->backup_table_ba)
			goto rebuild_tables;

		return nmbm_rescue_main_info_table(ni);
	}

	/* Main info table is successfully written, record its offset */
	ni->main_table_ba = table_start_ba;

	/* Adjust mapping_blocks_off */
	if (!ni->backup_table_ba)
		ni->mapping_blocks_ba = table_end_ba;

	nmbm_mark_tables_clean(ni);

	nlog_table_update(ni, true, table_start_ba, table_end_ba);

	return true;

rebuild_tables:
	return nmbm_rebuild_info_table(ni);
}

/*
 * nmbm_update_info_table - Update info table
 * @ni: NMBM instance structure
 *
 * Update both main and backup info table. Return true if at least one table
 * has been successfully written.
 * This function will try to update info table repeatedly until no new bad
 * block found during updating.
 */
static bool nmbm_update_info_table(struct nmbm_instance *ni)
{
	bool success;

	if (ni->protected)
		return true;

	while (ni->block_state_changed || ni->block_mapping_changed) {
		success = nmbm_update_info_table_once(ni, false);
		if (!success) {
			nlog_err(ni, "Failed to update info table\n");
			return false;
		}
	}

	return true;
}

/*
 * nmbm_map_block - Map a bad block to a unused spare block
 * @ni: NMBM instance structure
 * @lb: logic block addr to map
 */
static bool nmbm_map_block(struct nmbm_instance *ni, uint32_t lb)
{
	uint32_t pb;
	bool success;

	if (ni->mapping_blocks_ba == ni->mapping_blocks_top_ba) {
		nlog_warn(ni, "No spare unmapped blocks.\n");
		return false;
	}

	success = nmbm_block_walk(ni, false, ni->mapping_blocks_top_ba, &pb, 0,
				  ni->mapping_blocks_ba);
	if (!success) {
		nlog_warn(ni, "No spare unmapped blocks.\n");
		nmbm_update_info_table(ni);
		ni->mapping_blocks_top_ba = ni->mapping_blocks_ba;
		return false;
	}

	ni->block_mapping[lb] = pb;
	ni->mapping_blocks_top_ba--;
	ni->block_mapping_changed++;

	nlog_info(ni, "Logic block %u mapped to physical blcok %u\n", lb, pb);
	nmbm_mark_block_color_mapped(ni, pb);

	return true;
}

/*
 * nmbm_create_info_table - Create info table(s)
 * @ni: NMBM instance structure
 *
 * This function assumes that the chip has no existing info table(s)
 */
static bool nmbm_create_info_table(struct nmbm_instance *ni)
{
	uint32_t lb;
	bool success;

	/* Set initial mapping_blocks_top_off  */
	success = nmbm_block_walk(ni, false, ni->signature_ba,
				  &ni->mapping_blocks_top_ba, 1,
				  ni->mgmt_start_ba);
	if (!success) {
		nlog_err(ni, "No room for spare blocks\n");
		return false;
	}

	/* Generate info table cache */
	nmbm_generate_info_table_cache(ni);

	/* Write info table */
	success = nmbm_rebuild_info_table(ni);
	if (!success) {
		nlog_err(ni, "Failed to build info tables\n");
		return false;
	}

	/* Remap bad block(s) at end of data area */
	for (lb = ni->data_block_count; lb < ni->mgmt_start_ba; lb++) {
		success = nmbm_map_block(ni, lb);
		if (!success)
			break;

		ni->data_block_count++;
	}

	/* If state table and/or mapping table changed, update info table. */
	success = nmbm_update_info_table(ni);
	if (!success)
		return false;

	return true;
}

/*
 * nmbm_create_new - Create NMBM on a new chip
 * @ni: NMBM instance structure
 */
static bool nmbm_create_new(struct nmbm_instance *ni)
{
	bool success;

	/* Determine the boundary of management blocks */
	ni->mgmt_start_ba = ni->block_count * (NMBM_MGMT_DIV - ni->lower.max_ratio) / NMBM_MGMT_DIV;

	if (ni->lower.max_reserved_blocks && ni->block_count - ni->mgmt_start_ba > ni->lower.max_reserved_blocks)
		ni->mgmt_start_ba = ni->block_count - ni->lower.max_reserved_blocks;

	nlog_info(ni, "NMBM management region starts at block %u [0x%08llx]\n",
		  ni->mgmt_start_ba, ba2addr(ni, ni->mgmt_start_ba));
	nmbm_mark_block_color_mgmt(ni, ni->mgmt_start_ba, ni->block_count - 1);

	/* Fill block state table & mapping table */
	nmbm_scan_badblocks(ni);
	nmbm_build_mapping_table(ni);

	/* Write signature */
	ni->signature.header.magic = NMBM_MAGIC_SIGNATURE;
	ni->signature.header.version = NMBM_VER;
	ni->signature.header.size = sizeof(ni->signature);
	ni->signature.nand_size = ni->lower.size;
	ni->signature.block_size = ni->lower.erasesize;
	ni->signature.page_size = ni->lower.writesize;
	ni->signature.spare_size = ni->lower.oobsize;
	ni->signature.mgmt_start_pb = ni->mgmt_start_ba;
	ni->signature.max_try_count = NMBM_TRY_COUNT;
	nmbm_update_checksum(&ni->signature.header);

	if (ni->lower.flags & NMBM_F_READ_ONLY) {
		nlog_info(ni, "NMBM has been initialized in read-only mode\n");
		return true;
	}

	success = nmbm_write_signature(ni, ni->mgmt_start_ba,
				       &ni->signature, &ni->signature_ba);
	if (!success) {
		nlog_err(ni, "Failed to write signature to a proper offset\n");
		return false;
	}

	nlog_info(ni, "Signature has been written to block %u [0x%08llx]\n",
		 ni->signature_ba, ba2addr(ni, ni->signature_ba));
	nmbm_mark_block_color_signature(ni, ni->signature_ba);

	/* Write info table(s) */
	success = nmbm_create_info_table(ni);
	if (success) {
		nlog_info(ni, "NMBM has been successfully created\n");
		return true;
	}

	return false;
}

/*
 * nmbm_check_info_table_header - Check if a info table header is valid
 * @ni: NMBM instance structure
 * @data: pointer to the info table header
 */
static bool nmbm_check_info_table_header(struct nmbm_instance *ni, void *data)
{
	struct nmbm_info_table_header *ifthdr = data;

	if (ifthdr->header.magic != NMBM_MAGIC_INFO_TABLE)
		return false;

	if (ifthdr->header.size != ni->info_table_size)
		return false;

	if (ifthdr->mapping_table_off - ifthdr->state_table_off < ni->state_table_size)
		return false;

	if (ni->info_table_size - ifthdr->mapping_table_off < ni->mapping_table_size)
		return false;

	return true;
}

/*
 * nmbm_check_info_table - Check if a whole info table is valid
 * @ni: NMBM instance structure
 * @start_ba: start block address of this table
 * @end_ba: end block address of this table
 * @data: pointer to the info table header
 * @mapping_blocks_top_ba: return the block address of top remapped block
 */
static bool nmbm_check_info_table(struct nmbm_instance *ni, uint32_t start_ba,
				  uint32_t end_ba, void *data,
				  uint32_t *mapping_blocks_top_ba)
{
	struct nmbm_info_table_header *ifthdr = data;
	int32_t *block_mapping = (int32_t *)((uintptr_t)data + ifthdr->mapping_table_off);
	nmbm_bitmap_t *block_state = (nmbm_bitmap_t *)((uintptr_t)data + ifthdr->state_table_off);
	uint32_t minimum_mapping_pb = ni->signature_ba;
	uint32_t ba;

	for (ba = 0; ba < ni->data_block_count; ba++) {
		if ((block_mapping[ba] >= ni->data_block_count && block_mapping[ba] < end_ba) ||
		    block_mapping[ba] == ni->signature_ba)
			return false;

		if (block_mapping[ba] >= end_ba && block_mapping[ba] < minimum_mapping_pb)
			minimum_mapping_pb = block_mapping[ba];
	}

	for (ba = start_ba; ba < end_ba; ba++) {
		if (nmbm_get_block_state(ni, ba) != BLOCK_ST_GOOD)
			continue;

		if (nmbm_get_block_state_raw(block_state, ba) != BLOCK_ST_GOOD)
			return false;
	}

	*mapping_blocks_top_ba = minimum_mapping_pb - 1;

	return true;
}

/*
 * nmbm_try_load_info_table - Try to load info table from a address
 * @ni: NMBM instance structure
 * @ba: start block address of the info table
 * @eba: return the block address after end of the table
 * @write_count: return the write count of this table
 * @mapping_blocks_top_ba: return the block address of top remapped block
 * @table_loaded: used to record whether ni->info_table has valid data
 */
static bool nmbm_try_load_info_table(struct nmbm_instance *ni, uint32_t ba,
				     uint32_t *eba, uint32_t *write_count,
				     uint32_t *mapping_blocks_top_ba,
				     bool table_loaded)
{
	struct nmbm_info_table_header *ifthdr = (void *)ni->info_table_cache;
	uint8_t *off = ni->info_table_cache;
	uint32_t limit = ba + size2blk(ni, ni->info_table_size);
	uint32_t start_ba = 0, chunksize, sizeremain = ni->info_table_size;
	bool success, checkhdr = true;
	int ret;

	while (sizeremain && ba < limit) {
		WATCHDOG_RESET();

		if (nmbm_get_block_state(ni, ba) != BLOCK_ST_GOOD)
			goto next_block;

		if (nmbm_check_bad_phys_block(ni, ba)) {
			nmbm_set_block_state(ni, ba, BLOCK_ST_BAD);
			goto next_block;
		}

		chunksize = sizeremain;
		if (chunksize > ni->lower.erasesize)
			chunksize = ni->lower.erasesize;

		/* Assume block with ECC error has no info table data */
		ret = nmbn_read_data(ni, ba2addr(ni, ba), off, chunksize);
		if (ret < 0)
			goto skip_bad_block;
		else if (ret > 0)
			return false;

		if (checkhdr) {
			success = nmbm_check_info_table_header(ni, off);
			if (!success)
				return false;

			start_ba = ba;
			checkhdr = false;
		}

		off += chunksize;
		sizeremain -= chunksize;

		goto next_block;

	skip_bad_block:
		/* Only mark bad in memory */
		nmbm_set_block_state(ni, ba, BLOCK_ST_BAD);

	next_block:
		ba++;
	}

	if (sizeremain)
		return false;

	success = nmbm_check_header(ni->info_table_cache, ni->info_table_size);
	if (!success)
		return false;

	*eba = ba;
	*write_count = ifthdr->write_count;

	success = nmbm_check_info_table(ni, start_ba, ba, ni->info_table_cache,
					mapping_blocks_top_ba);
	if (!success)
		return false;

	if (!table_loaded || ifthdr->write_count > ni->info_table.write_count) {
		memcpy(&ni->info_table, ifthdr, sizeof(ni->info_table));
		memcpy(ni->block_state,
		       (uint8_t *)ifthdr + ifthdr->state_table_off,
		       ni->state_table_size);
		memcpy(ni->block_mapping,
		       (uint8_t *)ifthdr + ifthdr->mapping_table_off,
		       ni->mapping_table_size);
		ni->info_table.write_count = ifthdr->write_count;
	}

	return true;
}

/*
 * nmbm_search_info_table - Search info table from specific address
 * @ni: NMBM instance structure
 * @ba: start block address to search
 * @limit: highest block address allowed for searching
 * @table_start_ba: return the start block address of this table
 * @table_end_ba: return the block address after end of this table
 * @write_count: return the write count of this table
 * @mapping_blocks_top_ba: return the block address of top remapped block
 * @table_loaded: used to record whether ni->info_table has valid data
 */
static bool nmbm_search_info_table(struct nmbm_instance *ni, uint32_t ba,
				   uint32_t limit, uint32_t *table_start_ba,
				   uint32_t *table_end_ba,
				   uint32_t *write_count,
				   uint32_t *mapping_blocks_top_ba,
				   bool table_loaded)
{
	bool success;

	while (ba < limit - size2blk(ni, ni->info_table_size)) {
		WATCHDOG_RESET();

		success = nmbm_try_load_info_table(ni, ba, table_end_ba,
						   write_count,
						   mapping_blocks_top_ba,
						   table_loaded);
		if (success) {
			*table_start_ba = ba;
			return true;
		}

		ba++;
	}

	return false;
}

/*
 * nmbm_load_info_table - Load info table(s) from a chip
 * @ni: NMBM instance structure
 * @ba: start block address to search info table
 * @limit: highest block address allowed for searching
 */
static bool nmbm_load_info_table(struct nmbm_instance *ni, uint32_t ba,
				 uint32_t limit)
{
	uint32_t main_table_end_ba, backup_table_end_ba, table_end_ba;
	uint32_t main_mapping_blocks_top_ba, backup_mapping_blocks_top_ba;
	uint32_t main_table_write_count, backup_table_write_count;
	uint32_t i;
	bool success;

	/* Set initial value */
	ni->main_table_ba = 0;
	ni->backup_table_ba = 0;
	ni->info_table.write_count = 0;
	ni->mapping_blocks_top_ba = ni->signature_ba - 1;
	ni->data_block_count = ni->signature.mgmt_start_pb;

	/* Find first info table */
	success = nmbm_search_info_table(ni, ba, limit, &ni->main_table_ba,
		&main_table_end_ba, &main_table_write_count,
		&main_mapping_blocks_top_ba, false);
	if (!success) {
		nlog_warn(ni, "No valid info table found\n");
		return false;
	}

	table_end_ba = main_table_end_ba;

	nlog_table_found(ni, true, main_table_write_count, ni->main_table_ba,
			main_table_end_ba);

	/* Find second info table */
	success = nmbm_search_info_table(ni, main_table_end_ba, limit,
		&ni->backup_table_ba, &backup_table_end_ba,
		&backup_table_write_count, &backup_mapping_blocks_top_ba, true);
	if (!success) {
		nlog_warn(ni, "Second info table not found\n");
	} else {
		table_end_ba = backup_table_end_ba;

		nlog_table_found(ni, false, backup_table_write_count,
				ni->backup_table_ba, backup_table_end_ba);
	}

	/* Pick mapping_blocks_top_ba */
	if (!ni->backup_table_ba) {
		ni->mapping_blocks_top_ba= main_mapping_blocks_top_ba;
	} else {
		if (main_table_write_count >= backup_table_write_count)
			ni->mapping_blocks_top_ba = main_mapping_blocks_top_ba;
		else
			ni->mapping_blocks_top_ba = backup_mapping_blocks_top_ba;
	}

	/* Set final mapping_blocks_ba */
	ni->mapping_blocks_ba = table_end_ba;

	/* Set final data_block_count */
	for (i = ni->signature.mgmt_start_pb; i > 0; i--) {
		if (ni->block_mapping[i - 1] >= 0) {
			ni->data_block_count = i;
			break;
		}
	}

	/* Debug purpose: mark mapped blocks and bad blocks */
	for (i = 0; i < ni->data_block_count; i++) {
		if (ni->block_mapping[i] > ni->mapping_blocks_top_ba)
			nmbm_mark_block_color_mapped(ni, ni->block_mapping[i]);
	}

	for (i = 0; i < ni->block_count; i++) {
		if (nmbm_get_block_state(ni, i) == BLOCK_ST_BAD)
			nmbm_mark_block_color_bad(ni, i);
	}

	/* Regenerate the info table cache from the final selected info table */
	nmbm_generate_info_table_cache(ni);

	if (ni->lower.flags & NMBM_F_READ_ONLY)
		return true;

	/*
	 * If only one table exists, try to write another table.
	 * If two tables have different write count, try to update info table
	 */
	if (!ni->backup_table_ba) {
		success = nmbm_rescue_single_info_table(ni);
	} else if (main_table_write_count != backup_table_write_count) {
		/* Mark state & mapping tables changed */
		ni->block_state_changed = 1;
		ni->block_mapping_changed = 1;

		success = nmbm_update_single_info_table(ni,
			main_table_write_count < backup_table_write_count);
	} else {
		success = true;
	}

	/*
	 * If there is no spare unmapped blocks, or still only one table
	 * exists, set the chip to read-only
	 */
	if (ni->mapping_blocks_ba == ni->mapping_blocks_top_ba) {
		nlog_warn(ni, "No spare unmapped blocks. Device is now read-only\n");
		ni->protected = 1;
	} else if (!success) {
		nlog_warn(ni, "Only one info table found. Device is now read-only\n");
		ni->protected = 1;
	}

	return true;
}

/*
 * nmbm_load_existing - Load NMBM from a new chip
 * @ni: NMBM instance structure
 */
static bool nmbm_load_existing(struct nmbm_instance *ni)
{
	bool success;

	/* Calculate the boundary of management blocks */
	ni->mgmt_start_ba = ni->signature.mgmt_start_pb;

	nlog_debug(ni, "NMBM management region starts at block %u [0x%08llx]\n",
		  ni->mgmt_start_ba, ba2addr(ni, ni->mgmt_start_ba));
	nmbm_mark_block_color_mgmt(ni, ni->mgmt_start_ba,
				   ni->signature_ba - 1);

	/* Look for info table(s) */
	success = nmbm_load_info_table(ni, ni->mgmt_start_ba,
		ni->signature_ba);
	if (success) {
		nlog_info(ni, "NMBM has been successfully attached %s\n",
			  (ni->lower.flags & NMBM_F_READ_ONLY) ? "in read-only mode" : "");
		return true;
	}

	if (!(ni->lower.flags & NMBM_F_CREATE))
		return false;

	/* Fill block state table & mapping table */
	nmbm_scan_badblocks(ni);
	nmbm_build_mapping_table(ni);

	if (ni->lower.flags & NMBM_F_READ_ONLY) {
		nlog_info(ni, "NMBM has been initialized in read-only mode\n");
		return true;
	}

	/* Write info table(s) */
	success = nmbm_create_info_table(ni);
	if (success) {
		nlog_info(ni, "NMBM has been successfully created\n");
		return true;
	}

	return false;
}

/*
 * nmbm_find_signature - Find signature in the lower NAND chip
 * @ni: NMBM instance structure
 * @signature_ba: used for storing block address of the signature
 * @signature_ba: return the actual block address of signature block
 *
 * Find a valid signature from a specific range in the lower NAND chip,
 * from bottom (highest address) to top (lowest address)
 *
 * Return true if found.
 */
static bool nmbm_find_signature(struct nmbm_instance *ni,
				struct nmbm_signature *signature,
				uint32_t *signature_ba)
{
	struct nmbm_signature sig;
	uint64_t off, addr;
	uint32_t block_count, ba, limit;
	bool success;
	int ret;

	/* Calculate top and bottom block address */
	block_count = ni->lower.size >> ni->erasesize_shift;
	ba = block_count;
	limit = (block_count / NMBM_MGMT_DIV) * (NMBM_MGMT_DIV - ni->lower.max_ratio);
	if (ni->lower.max_reserved_blocks && block_count - limit > ni->lower.max_reserved_blocks)
		limit = block_count - ni->lower.max_reserved_blocks;

	while (ba >= limit) {
		WATCHDOG_RESET();

		ba--;
		addr = ba2addr(ni, ba);

		if (nmbm_check_bad_phys_block(ni, ba))
			continue;

		/* Check every page.
		 * As long as at leaset one page contains valid signature,
		 * the block is treated as a valid signature block.
		 */
		for (off = 0; off < ni->lower.erasesize;
		     off += ni->lower.writesize) {
			WATCHDOG_RESET();

			ret = nmbn_read_data(ni, addr + off, &sig,
					     sizeof(sig));
			if (ret)
				continue;

			/* Check for header size and checksum */
			success = nmbm_check_header(&sig, sizeof(sig));
			if (!success)
				continue;

			/* Check for header magic */
			if (sig.header.magic == NMBM_MAGIC_SIGNATURE) {
				/* Found it */
				memcpy(signature, &sig, sizeof(sig));
				*signature_ba = ba;
				return true;
			}
		}
	};

	return false;
}

/*
 * is_power_of_2_u64 - Check whether a 64-bit integer is power of 2
 * @n: number to check
 */
static bool is_power_of_2_u64(uint64_t n)
{
	return (n != 0 && ((n & (n - 1)) == 0));
}

/*
 * nmbm_check_lower_members - Validate the members of lower NAND device
 * @nld: Lower NAND chip structure
 */
static bool nmbm_check_lower_members(struct nmbm_lower_device *nld)
{

	if (!nld->size || !is_power_of_2_u64(nld->size)) {
		nmbm_log_lower(nld, NMBM_LOG_ERR,
			       "Chip size %llu is not valid\n", nld->size);
		return false;
	}

	if (!nld->erasesize || !is_power_of_2(nld->erasesize)) {
		nmbm_log_lower(nld, NMBM_LOG_ERR,
			       "Block size %u is not valid\n", nld->erasesize);
		return false;
	}

	if (!nld->writesize || !is_power_of_2(nld->writesize)) {
		nmbm_log_lower(nld, NMBM_LOG_ERR,
			       "Page size %u is not valid\n", nld->writesize);
		return false;
	}

	if (!nld->oobsize) {
		nmbm_log_lower(nld, NMBM_LOG_ERR,
			       "Page spare size %u is not valid\n", nld->oobsize);
		return false;
	}

	if (!nld->read_page) {
		nmbm_log_lower(nld, NMBM_LOG_ERR, "read_page() is required\n");
		return false;
	}

	if (!(nld->flags & NMBM_F_READ_ONLY) && (!nld->write_page || !nld->erase_block)) {
		nmbm_log_lower(nld, NMBM_LOG_ERR,
			       "write_page() and erase_block() are required\n");
		return false;
	}

	/* Data sanity check */
	if (!nld->max_ratio)
		nld->max_ratio = 1;

	if (nld->max_ratio >= NMBM_MGMT_DIV - 1) {
		nmbm_log_lower(nld, NMBM_LOG_ERR,
			       "max ratio %u is invalid\n", nld->max_ratio);
		return false;
	}

	if (nld->max_reserved_blocks && nld->max_reserved_blocks < NMBM_MGMT_BLOCKS_MIN) {
		nmbm_log_lower(nld, NMBM_LOG_ERR,
			       "max reserved blocks %u is too small\n", nld->max_reserved_blocks);
		return false;
	}

	return true;
}

/*
 * nmbm_calc_structure_size - Calculate the instance structure size
 * @nld: NMBM lower device structure
 */
size_t nmbm_calc_structure_size(struct nmbm_lower_device *nld)
{
	uint32_t state_table_size, mapping_table_size, info_table_size;
	uint32_t block_count;

	block_count = nmbm_lldiv(nld->size, nld->erasesize);

	/* Calculate info table size */
	state_table_size = ((block_count + NMBM_BITMAP_BLOCKS_PER_UNIT - 1) /
		NMBM_BITMAP_BLOCKS_PER_UNIT) * NMBM_BITMAP_UNIT_SIZE;
	mapping_table_size = block_count * sizeof(int32_t);

	info_table_size = NMBM_ALIGN(sizeof(struct nmbm_info_table_header),
				     nld->writesize);
	info_table_size += NMBM_ALIGN(state_table_size, nld->writesize);
	info_table_size += NMBM_ALIGN(mapping_table_size, nld->writesize);

	return info_table_size + state_table_size + mapping_table_size +
		nld->writesize + nld->oobsize + sizeof(struct nmbm_instance);
}

/*
 * nmbm_init_structure - Initialize members of instance structure
 * @ni: NMBM instance structure
 */
static void nmbm_init_structure(struct nmbm_instance *ni)
{
	uint32_t pages_per_block, blocks_per_chip;
	uintptr_t ptr;

	pages_per_block = ni->lower.erasesize / ni->lower.writesize;
	blocks_per_chip = nmbm_lldiv(ni->lower.size, ni->lower.erasesize);

	ni->rawpage_size = ni->lower.writesize + ni->lower.oobsize;
	ni->rawblock_size = pages_per_block * ni->rawpage_size;
	ni->rawchip_size = blocks_per_chip * ni->rawblock_size;

	ni->writesize_mask = ni->lower.writesize - 1;
	ni->erasesize_mask = ni->lower.erasesize - 1;

	ni->writesize_shift = ffs(ni->lower.writesize) - 1;
	ni->erasesize_shift = ffs(ni->lower.erasesize) - 1;

	/* Calculate number of block this chip */
	ni->block_count = ni->lower.size >> ni->erasesize_shift;

	/* Calculate info table size */
	ni->state_table_size = ((ni->block_count + NMBM_BITMAP_BLOCKS_PER_UNIT - 1) /
		NMBM_BITMAP_BLOCKS_PER_UNIT) * NMBM_BITMAP_UNIT_SIZE;
	ni->mapping_table_size = ni->block_count * sizeof(*ni->block_mapping);

	ni->info_table_size = NMBM_ALIGN(sizeof(ni->info_table),
					 ni->lower.writesize);
	ni->info_table.state_table_off = ni->info_table_size;

	ni->info_table_size += NMBM_ALIGN(ni->state_table_size,
					  ni->lower.writesize);
	ni->info_table.mapping_table_off = ni->info_table_size;

	ni->info_table_size += NMBM_ALIGN(ni->mapping_table_size,
					  ni->lower.writesize);

	ni->info_table_spare_blocks = nmbm_get_spare_block_count(
		size2blk(ni, ni->info_table_size));

	/* Assign memory to members */
	ptr = (uintptr_t)ni + sizeof(*ni);

	ni->info_table_cache = (void *)ptr;
	ptr += ni->info_table_size;

	ni->block_state = (void *)ptr;
	ptr += ni->state_table_size;

	ni->block_mapping = (void *)ptr;
	ptr += ni->mapping_table_size;

	ni->page_cache = (uint8_t *)ptr;

	/* Initialize block state table */
	ni->block_state_changed = 0;
	memset(ni->block_state, 0xff, ni->state_table_size);

	/* Initialize block mapping table */
	ni->block_mapping_changed = 0;
}

/*
 * nmbm_attach - Attach to a lower device
 * @nld: NMBM lower device structure
 * @ni: NMBM instance structure
 */
int nmbm_attach(struct nmbm_lower_device *nld, struct nmbm_instance *ni)
{
	bool success;

	if (!nld || !ni)
		return -EINVAL;

	/* Set default log level */
	ni->log_display_level = NMBM_DEFAULT_LOG_LEVEL;

	/* Check lower members */
	success = nmbm_check_lower_members(nld);
	if (!success)
		return -EINVAL;

	/* Initialize NMBM instance */
	memcpy(&ni->lower, nld, sizeof(struct nmbm_lower_device));
	nmbm_init_structure(ni);

	success = nmbm_find_signature(ni, &ni->signature, &ni->signature_ba);
	if (!success) {
		if (!(nld->flags & NMBM_F_CREATE)) {
			nlog_err(ni, "Signature not found\n");
			return -ENODEV;
		}

		success = nmbm_create_new(ni);
		if (!success)
			return -ENODEV;

		return 0;
	}

	nlog_info(ni, "Signature found at block %u [0x%08llx]\n",
		 ni->signature_ba, ba2addr(ni, ni->signature_ba));
	nmbm_mark_block_color_signature(ni, ni->signature_ba);

	if (ni->signature.header.version != NMBM_VER) {
		nlog_err(ni, "NMBM version %u.%u is not supported\n",
			NMBM_VERSION_MAJOR_GET(ni->signature.header.version),
			NMBM_VERSION_MINOR_GET(ni->signature.header.version));
		return -EINVAL;
	}

	if (ni->signature.nand_size != nld->size ||
	    ni->signature.block_size != nld->erasesize ||
	    ni->signature.page_size != nld->writesize ||
	    ni->signature.spare_size != nld->oobsize) {
		nlog_err(ni, "NMBM configuration mismatch\n");
		return -EINVAL;
	}

	success = nmbm_load_existing(ni);
	if (!success)
		return -ENODEV;

	return 0;
}

/*
 * nmbm_detach - Detach from a lower device, and save all tables
 * @ni: NMBM instance structure
 */
int nmbm_detach(struct nmbm_instance *ni)
{
	if (!ni)
		return -EINVAL;

	if (!(ni->lower.flags & NMBM_F_READ_ONLY))
		nmbm_update_info_table(ni);

	nmbm_mark_block_color_normal(ni, 0, ni->block_count - 1);

	return 0;
}

/*
 * nmbm_erase_logic_block - Erase a logic block
 * @ni: NMBM instance structure
 * @nmbm_erase_logic_block: logic block address
 *
 * Logic block will be mapped to physical block before erasing.
 * Bad block found during erasinh will be remapped to a good block if there is
 * still at least one good spare block available.
 */
static int nmbm_erase_logic_block(struct nmbm_instance *ni, uint32_t block_addr)
{
	uint32_t pb;
	bool success;

retry:
	/* Map logic block to physical block */
	pb = ni->block_mapping[block_addr];

	/* Whether the logic block is good (has valid mapping) */
	if ((int32_t)pb < 0) {
		nlog_debug(ni, "Logic block %u is a bad block\n", block_addr);
		return -EIO;
	}

	/* Remap logic block if current physical block is a bad block */
	if (nmbm_get_block_state(ni, pb) == BLOCK_ST_BAD ||
	    nmbm_get_block_state(ni, pb) == BLOCK_ST_NEED_REMAP)
		goto remap_logic_block;

	/* Insurance to detect unexpected bad block marked by user */
	if (nmbm_check_bad_phys_block(ni, pb)) {
		nlog_warn(ni, "Found unexpected bad block possibly marked by user\n");
		nmbm_set_block_state(ni, pb, BLOCK_ST_BAD);
		goto remap_logic_block;
	}

	success = nmbm_erase_block_and_check(ni, pb);
	if (success)
		return 0;

	/* Mark bad block */
	nmbm_mark_phys_bad_block(ni, pb);
	nmbm_set_block_state(ni, pb, BLOCK_ST_BAD);

remap_logic_block:
	/* Try to assign a new block */
	success = nmbm_map_block(ni, block_addr);
	if (!success) {
		/* Mark logic block unusable, and update info table */
		ni->block_mapping[block_addr] = -1;
		if (nmbm_get_block_state(ni, pb) != BLOCK_ST_NEED_REMAP)
			nmbm_set_block_state(ni, pb, BLOCK_ST_BAD);
		nmbm_update_info_table(ni);
		return -EIO;
	}

	/* Update info table before erasing */
	if (nmbm_get_block_state(ni, pb) != BLOCK_ST_NEED_REMAP)
		nmbm_set_block_state(ni, pb, BLOCK_ST_BAD);
	nmbm_update_info_table(ni);

	goto retry;
}

/*
 * nmbm_erase_block_range - Erase logic blocks
 * @ni: NMBM instance structure
 * @addr: logic linear address
 * @size: erase range
 * @failed_addr: return failed block address if error occurs
 */
int nmbm_erase_block_range(struct nmbm_instance *ni, uint64_t addr,
			   uint64_t size, uint64_t *failed_addr)
{
	uint32_t start_ba, end_ba;
	int ret;

	if (!ni)
		return -EINVAL;

	/* Sanity check */
	if (ni->protected || (ni->lower.flags & NMBM_F_READ_ONLY)) {
		nlog_debug(ni, "Device is forced read-only\n");
		return -EROFS;
	}

	if (addr >= ba2addr(ni, ni->data_block_count)) {
		nlog_err(ni, "Address 0x%llx is invalid\n", addr);
		return -EINVAL;
	}

	if (addr + size > ba2addr(ni, ni->data_block_count)) {
		nlog_err(ni, "Erase range 0xllxu is too large\n", size);
		return -EINVAL;
	}

	if (!size) {
		nlog_warn(ni, "No blocks to be erased\n");
		return 0;
	}

	start_ba = addr2ba(ni, addr);
	end_ba = addr2ba(ni, addr + size - 1);

	while (start_ba <= end_ba) {
		WATCHDOG_RESET();

		ret = nmbm_erase_logic_block(ni, start_ba);
		if (ret) {
			if (failed_addr)
				*failed_addr = ba2addr(ni, start_ba);
			return ret;
		}

		start_ba++;
	}

	return 0;
}

/*
 * nmbm_read_logic_page - Read page based on logic address
 * @ni: NMBM instance structure
 * @addr: logic linear address
 * @data: buffer to store main data. optional.
 * @oob: buffer to store oob data. optional.
 * @mode: read mode
 *
 * Return 0 for success, positive value for corrected bitflip count,
 * -EBADMSG for ecc error, other negative values for other errors
 */
static int nmbm_read_logic_page(struct nmbm_instance *ni, uint64_t addr,
				void *data, void *oob, enum nmbm_oob_mode mode)
{
	uint32_t lb, pb, offset;
	uint64_t paddr;

	/* Extract block address and in-block offset */
	lb = addr2ba(ni, addr);
	offset = addr & ni->erasesize_mask;

	/* Map logic block to physical block */
	pb = ni->block_mapping[lb];

	/* Whether the logic block is good (has valid mapping) */
	if ((int32_t)pb < 0) {
		nlog_debug(ni, "Logic block %u is a bad block\n", lb);
		return -EIO;
	}

	/* Fail if physical block is marked bad */
	if (nmbm_get_block_state(ni, pb) == BLOCK_ST_BAD)
		return -EIO;

	/* Assemble new address */
	paddr = ba2addr(ni, pb) + offset;

	return nmbm_read_phys_page(ni, paddr, data, oob, mode);
}

/*
 * nmbm_read_single_page - Read one page based on logic address
 * @ni: NMBM instance structure
 * @addr: logic linear address
 * @data: buffer to store main data. optional.
 * @oob: buffer to store oob data. optional.
 * @mode: read mode
 *
 * Return 0 for success, positive value for corrected bitflip count,
 * -EBADMSG for ecc error, other negative values for other errors
 */
int nmbm_read_single_page(struct nmbm_instance *ni, uint64_t addr, void *data,
			  void *oob, enum nmbm_oob_mode mode)
{
	if (!ni)
		return -EINVAL;

	/* Sanity check */
	if (ni->protected) {
		nlog_debug(ni, "Device is forced read-only\n");
		return -EROFS;
	}

	if (addr >= ba2addr(ni, ni->data_block_count)) {
		nlog_err(ni, "Address 0x%llx is invalid\n", addr);
		return -EINVAL;
	}

	return nmbm_read_logic_page(ni, addr, data, oob, mode);
}

/*
 * nmbm_read_range - Read data without oob
 * @ni: NMBM instance structure
 * @addr: logic linear address
 * @size: data size to read
 * @data: buffer to store main data to be read
 * @mode: read mode
 * @retlen: return actual data size read
 *
 * Return 0 for success, positive value for corrected bitflip count,
 * -EBADMSG for ecc error, other negative values for other errors
 */
int nmbm_read_range(struct nmbm_instance *ni, uint64_t addr, size_t size,
		    void *data, enum nmbm_oob_mode mode, size_t *retlen)
{
	uint64_t off = addr;
	uint8_t *ptr = data;
	size_t sizeremain = size, chunksize, leading;
	bool has_ecc_err = false;
	int ret, max_bitflips = 0;

	if (!ni)
		return -EINVAL;

	/* Sanity check */
	if (ni->protected) {
		nlog_debug(ni, "Device is forced read-only\n");
		return -EROFS;
	}

	if (addr >= ba2addr(ni, ni->data_block_count)) {
		nlog_err(ni, "Address 0x%llx is invalid\n", addr);
		return -EINVAL;
	}

	if (addr + size > ba2addr(ni, ni->data_block_count)) {
		nlog_err(ni, "Read range 0x%llx is too large\n", size);
		return -EINVAL;
	}

	if (!size) {
		nlog_warn(ni, "No data to be read\n");
		return 0;
	}

	while (sizeremain) {
		WATCHDOG_RESET();

		leading = off & ni->writesize_mask;
		chunksize = ni->lower.writesize - leading;
		if (chunksize > sizeremain)
			chunksize = sizeremain;

		if (chunksize == ni->lower.writesize) {
			ret = nmbm_read_logic_page(ni, off - leading, ptr,
							NULL, mode);
			if (ret < 0 && ret != -EBADMSG)
				break;
		} else {
			ret = nmbm_read_logic_page(ni, off - leading,
							ni->page_cache, NULL,
							mode);
			if (ret < 0 && ret != -EBADMSG)
				break;

			memcpy(ptr, ni->page_cache + leading, chunksize);
		}

		if (ret == -EBADMSG)
			has_ecc_err = true;

		if (ret > max_bitflips)
			max_bitflips = ret;

		off += chunksize;
		ptr += chunksize;
		sizeremain -= chunksize;
	}

	if (retlen)
		*retlen = size - sizeremain;

	if (ret < 0 && ret != -EBADMSG)
		return ret;

	if (has_ecc_err)
		return -EBADMSG;

	return max_bitflips;
}

/*
 * nmbm_write_logic_page - Read page based on logic address
 * @ni: NMBM instance structure
 * @addr: logic linear address
 * @data: buffer contains main data. optional.
 * @oob: buffer contains oob data. optional.
 * @mode: write mode
 */
static int nmbm_write_logic_page(struct nmbm_instance *ni, uint64_t addr,
				  const void *data, const void *oob,
				  enum nmbm_oob_mode mode)
{
	uint32_t lb, pb, offset;
	uint64_t paddr;
	bool success;

	/* Extract block address and in-block offset */
	lb = addr2ba(ni, addr);
	offset = addr & ni->erasesize_mask;

	/* Map logic block to physical block */
	pb = ni->block_mapping[lb];

	/* Whether the logic block is good (has valid mapping) */
	if ((int32_t)pb < 0) {
		nlog_debug(ni, "Logic block %u is a bad block\n", lb);
		return -EIO;
	}

	/* Fail if physical block is marked bad */
	if (nmbm_get_block_state(ni, pb) == BLOCK_ST_BAD)
		return -EIO;

	/* Assemble new address */
	paddr = ba2addr(ni, pb) + offset;

	success = nmbm_write_phys_page(ni, paddr, data, oob, mode);
	if (success)
		return 0;

	/*
	 * Do not remap bad block here. Just mark this block in state table.
	 * Remap this block on erasing.
	 */
	nmbm_set_block_state(ni, pb, BLOCK_ST_NEED_REMAP);
	nmbm_update_info_table(ni);

	return -EIO;
}

/*
 * nmbm_write_single_page - Write one page based on logic address
 * @ni: NMBM instance structure
 * @addr: logic linear address
 * @data: buffer contains main data. optional.
 * @oob: buffer contains oob data. optional.
 * @mode: write mode
 */
int nmbm_write_single_page(struct nmbm_instance *ni, uint64_t addr,
			   const void *data, const void *oob,
			   enum nmbm_oob_mode mode)
{
	if (!ni)
		return -EINVAL;

	/* Sanity check */
	if (ni->protected || (ni->lower.flags & NMBM_F_READ_ONLY)) {
		nlog_debug(ni, "Device is forced read-only\n");
		return -EROFS;
	}

	if (addr >= ba2addr(ni, ni->data_block_count)) {
		nlog_err(ni, "Address 0x%llx is invalid\n", addr);
		return -EINVAL;
	}

	return nmbm_write_logic_page(ni, addr, data, oob, mode);
}

/*
 * nmbm_write_range - Write data without oob
 * @ni: NMBM instance structure
 * @addr: logic linear address
 * @size: data size to write
 * @data: buffer contains data to be written
 * @mode: write mode
 * @retlen: return actual data size written
 */
int nmbm_write_range(struct nmbm_instance *ni, uint64_t addr, size_t size,
		     const void *data, enum nmbm_oob_mode mode,
		     size_t *retlen)
{
	uint64_t off = addr;
	const uint8_t *ptr = data;
	size_t sizeremain = size, chunksize, leading;
	int ret;

	if (!ni)
		return -EINVAL;

	/* Sanity check */
	if (ni->protected || (ni->lower.flags & NMBM_F_READ_ONLY)) {
		nlog_debug(ni, "Device is forced read-only\n");
		return -EROFS;
	}

	if (addr >= ba2addr(ni, ni->data_block_count)) {
		nlog_err(ni, "Address 0x%llx is invalid\n", addr);
		return -EINVAL;
	}

	if (addr + size > ba2addr(ni, ni->data_block_count)) {
		nlog_err(ni, "Write size 0x%zx is too large\n", size);
		return -EINVAL;
	}

	if (!size) {
		nlog_warn(ni, "No data to be written\n");
		return 0;
	}

	while (sizeremain) {
		WATCHDOG_RESET();

		leading = off & ni->writesize_mask;
		chunksize = ni->lower.writesize - leading;
		if (chunksize > sizeremain)
			chunksize = sizeremain;

		if (chunksize == ni->lower.writesize) {
			ret = nmbm_write_logic_page(ni, off - leading, ptr,
							 NULL, mode);
			if (ret)
				break;
		} else {
			memset(ni->page_cache, 0xff, leading);
			memcpy(ni->page_cache + leading, ptr, chunksize);

			ret = nmbm_write_logic_page(ni, off - leading,
							 ni->page_cache, NULL,
							 mode);
			if (ret)
				break;
		}

		off += chunksize;
		ptr += chunksize;
		sizeremain -= chunksize;
	}

	if (retlen)
		*retlen = size - sizeremain;

	return ret;
}

/*
 * nmbm_check_bad_block - Check whether a logic block is usable
 * @ni: NMBM instance structure
 * @addr: logic linear address
 */
int nmbm_check_bad_block(struct nmbm_instance *ni, uint64_t addr)
{
	uint32_t lb, pb;

	if (!ni)
		return -EINVAL;

	if (addr >= ba2addr(ni, ni->data_block_count)) {
		nlog_err(ni, "Address 0x%llx is invalid\n", addr);
		return -EINVAL;
	}

	lb = addr2ba(ni, addr);

	/* Map logic block to physical block */
	pb = ni->block_mapping[lb];

	if ((int32_t)pb < 0)
		return 1;

	if (nmbm_get_block_state(ni, pb) == BLOCK_ST_BAD)
		return 1;

	return 0;
}

/*
 * nmbm_mark_bad_block - Mark a logic block unusable
 * @ni: NMBM instance structure
 * @addr: logic linear address
 */
int nmbm_mark_bad_block(struct nmbm_instance *ni, uint64_t addr)
{
	uint32_t lb, pb;

	if (!ni)
		return -EINVAL;

	/* Sanity check */
	if (ni->protected || (ni->lower.flags & NMBM_F_READ_ONLY)) {
		nlog_debug(ni, "Device is forced read-only\n");
		return -EROFS;
	}

	if (addr >= ba2addr(ni, ni->data_block_count)) {
		nlog_err(ni, "Address 0x%llx is invalid\n", addr);
		return -EINVAL;
	}

	lb = addr2ba(ni, addr);

	/* Map logic block to physical block */
	pb = ni->block_mapping[lb];

	if ((int32_t)pb < 0)
		return 0;

	ni->block_mapping[lb] = -1;
	nmbm_mark_phys_bad_block(ni, pb);
	nmbm_set_block_state(ni, pb, BLOCK_ST_BAD);
	nmbm_update_info_table(ni);

	return 0;
}

/*
 * nmbm_get_avail_size - Get available user data size
 * @ni: NMBM instance structure
 */
uint64_t nmbm_get_avail_size(struct nmbm_instance *ni)
{
	if (!ni)
		return 0;

	return (uint64_t)ni->data_block_count << ni->erasesize_shift;
}

/*
 * nmbm_get_lower_device - Get lower device structure
 * @ni: NMBM instance structure
 * @nld: pointer to hold the data of lower device structure
 */
int nmbm_get_lower_device(struct nmbm_instance *ni, struct nmbm_lower_device *nld)
{
	if (!ni)
		return -EINVAL;

	if (nld)
		memcpy(nld, &ni->lower, sizeof(*nld));

	return 0;
}

#include "nmbm-debug.inl"
