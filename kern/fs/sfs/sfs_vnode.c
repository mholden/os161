/*
 * SFS filesystem
 *
 * File-level (vnode) interface routines.
 */
#include <types.h>
#include <lib.h>
#include <synch.h>
#include <array.h>
#include <bitmap.h>
#include <kern/stat.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <uio.h>
#include <dev.h>
#include <sfs.h>

/* At bottom of file */
static int 
sfs_loadvnode(struct sfs_fs *sfs, u_int32_t ino, int type,
		 struct sfs_vnode **ret);

////////////////////////////////////////////////////////////
//
// Simple stuff

/* Zero out a disk block. */
static
int
sfs_clearblock(struct sfs_fs *sfs, u_int32_t block)
{
	/* static -> automatically initialized to zero */
	static char zeros[SFS_BLOCKSIZE];
	return sfs_wblock(sfs, zeros, block);
}

/* Write an on-disk inode structure back out to disk. */
static
int
sfs_sync_inode(struct sfs_vnode *sv)
{
	if (sv->sv_dirty) {
		struct sfs_fs *sfs = sv->sv_v.vn_fs->fs_data;
		int result = sfs_wblock(sfs, &sv->sv_i, sv->sv_ino);
		if (result) {
			return result;
		}
		sv->sv_dirty = 0;
	}
	return 0;
}

////////////////////////////////////////////////////////////
//
// Space allocation

/*
 * Allocate a block.
 */
static
int
sfs_balloc(struct sfs_fs *sfs, u_int32_t *diskblock)
{
	int result;

	lock_acquire(sfs->sfs_freemap_lock);

	result = bitmap_alloc(sfs->sfs_freemap, diskblock);
	if (result) {
		return result;
	}
	sfs->sfs_freemapdirty = 1;

	lock_release(sfs->sfs_freemap_lock);

	if (*diskblock >= sfs->sfs_super.sp_nblocks) {
		panic("sfs: balloc: invalid block %u\n", *diskblock);
	}

	/* Clear block before returning it */
	return sfs_clearblock(sfs, *diskblock);
}

/*
 * Free a block.
 */
static
void
sfs_bfree(struct sfs_fs *sfs, u_int32_t diskblock)
{
	/* Don't think we need to synchronize this */
	bitmap_unmark(sfs->sfs_freemap, diskblock);
	sfs->sfs_freemapdirty = 1;
}

/*
 * Check if a block is in use.
 */
static
int
sfs_bused(struct sfs_fs *sfs, u_int32_t diskblock)
{
	/* Don't think we need to synchronize this */
	if (diskblock >= sfs->sfs_super.sp_nblocks) {
		panic("sfs: sfs_bused called on out of range block %u\n", 
		      diskblock);
	}
	return bitmap_isset(sfs->sfs_freemap, diskblock);
}

////////////////////////////////////////////////////////////
//
// Block mapping/inode maintenance

/*
 * Look up the disk block number (from 0 up to the number of blocks on
 * the disk) given a file and the logical block number within that
 * file. If DOALLOC is set, and no such block exists, one will be
 * allocated.
 */
static
int
sfs_bmap(struct sfs_vnode *sv, u_int32_t fileblock, int doalloc,
	    u_int32_t *diskblock)
{
	/*
	 * I/O buffer for handling indirect blocks.
	 *
	 * Note: in real life (and when you've done the fs assignment)
	 * you would get space from the disk buffer cache for this,
	 * not use a static area.
	 */
	static u_int32_t idbuf[SFS_DBPERIDB];

	struct sfs_fs *sfs = sv->sv_v.vn_fs->fs_data;
	u_int32_t block;
	u_int32_t idblock;
	u_int32_t idnum, idoff;
	int result;

	assert(sizeof(idbuf)==SFS_BLOCKSIZE);

	/*
	 * If the block we want is one of the direct blocks...
	 */
	if (fileblock < SFS_NDIRECT) {
		/*
		 * Get the block number
		 */
		block = sv->sv_i.sfi_direct[fileblock];

		/*
		 * Do we need to allocate?
		 */
		if (block==0 && doalloc) {
			result = sfs_balloc(sfs, &block);
			if (result) {
				return result;
			}

			/* Remember what we allocated; mark inode dirty */
			sv->sv_i.sfi_direct[fileblock] = block;
			sv->sv_dirty = 1;
		}

		/*
		 * Hand back the block
		 */
		if (block != 0 && !sfs_bused(sfs, block)) {
			panic("sfs: Data block %u (block %u of file %u) "
			      "marked free\n", block, fileblock, sv->sv_ino);
		}
		*diskblock = block;
		return 0;
	}

	/*
	 * It's not a direct block; it must be in the indirect block.
	 * Subtract off the number of direct blocks, so FILEBLOCK is
	 * now the offset into the indirect block space.
	 */

	fileblock -= SFS_NDIRECT;

	/* Get the indirect block number and offset w/i that indirect block */
	idnum = fileblock / SFS_DBPERIDB;
	idoff = fileblock % SFS_DBPERIDB;

	/*
	 * We only have one indirect block. If the offset we were asked for
	 * is too large, we can't handle it, so fail.
	 */
	if (idnum > 0) {
		return EINVAL;
	}

	/* 
	 * Probably need to synchronize the stuff below. Maybe not though
	 * since no two threads can be writing to the same file at the same
	 * time (this is protected with file locks in the VFS layer).
	 */

	/* Get the disk block number of the indirect block. */
	idblock = sv->sv_i.sfi_indirect;

	if (idblock==0 && !doalloc) {
		/*
		 * There's no indirect block allocated. We weren't
		 * asked to allocate anything, so pretend the indirect
		 * block was filled with all zeros.
		 */
		*diskblock = 0;
		return 0;
	}
	else if (idblock==0) {
		/*
		 * There's no indirect block allocated, but we need to
		 * allocate a block whose number needs to be stored in
		 * the indirect block. Thus, we need to allocate an
		 * indirect block.
		 */
		result = sfs_balloc(sfs, &idblock);
		if (result) {
			return result;
		}

		/* Remember the block we just allocated */
		sv->sv_i.sfi_indirect = idblock;

		/* Mark the inode dirty */
		sv->sv_dirty = 1;

		/* Clear the indirect block buffer */
		bzero(idbuf, sizeof(idbuf));
	}
	else {
		/*
		 * We already have an indirect block allocated; load it.
		 */
		result = sfs_rblock(sfs, idbuf, idblock);
		if (result) {
			return result;
		}
	}

	/* Get the block out of the indirect block buffer */
	block = idbuf[idoff];

	/* If there's no block there, allocate one */
	if (block==0 && doalloc) {
		result = sfs_balloc(sfs, &block);
		if (result) {
			return result;
		}

		/* Remember the block we allocated */
		idbuf[idoff] = block;

		/* The indirect block is now dirty; write it back */
		result = sfs_wblock(sfs, idbuf, idblock);
		if (result) {
			return result;
		}
	}

	/* Hand back the result and return. */
	if (block != 0 && !sfs_bused(sfs, block)) {
		panic("sfs: Data block %u (block %u of file %u) marked free\n",
		      block, fileblock, sv->sv_ino);
	}
	*diskblock = block;
	return 0;
}

////////////////////////////////////////////////////////////
//
// File-level I/O

/*
 * Do I/O to a block of a file that doesn't cover the whole block.  We
 * need to read in the original block first, even if we're writing, so
 * we don't clobber the portion of the block we're not intending to
 * write over.
 *
 * skipstart is the number of bytes to skip past at the beginning of
 * the sector; len is the number of bytes to actually read or write.
 * uio is the area to do the I/O into.
 */
static
int
sfs_partialio(struct sfs_vnode *sv, struct uio *uio,
	      u_int32_t skipstart, u_int32_t len)
{
	/*
	 * I/O buffer for handling partial sectors.
	 *
	 * Note: in real life (and when you've done the fs assignment)
	 * you would get space from the disk buffer cache for this,
	 * not use a static area.
	 */
	static char iobuf[SFS_BLOCKSIZE];

	struct sfs_fs *sfs = sv->sv_v.vn_fs->fs_data;
	u_int32_t diskblock;
	u_int32_t fileblock;
	int result;
	
	/* Allocate missing blocks if and only if we're writing */
	int doalloc = (uio->uio_rw==UIO_WRITE);

	assert(skipstart + len <= SFS_BLOCKSIZE);

	/* Compute the block offset of this block in the file */
	fileblock = uio->uio_offset / SFS_BLOCKSIZE;

	/* Get the disk block number */
	result = sfs_bmap(sv, fileblock, doalloc, &diskblock);
	if (result) {
		return result;
	}

	/* f_test 3 debug */
	u_int32_t i;
	char ch;

	if (diskblock == 0) {
		/*
		 * There was no block mapped at this point in the file.
		 * Zero the buffer.
		 */
		assert(uio->uio_rw == UIO_READ);
		bzero(iobuf, sizeof(iobuf));
	}
	else {
		/*
		 * Read the block.
		 */
		result = sfs_rblock(sfs, iobuf, diskblock);
		if (result) {
			return result;
		}

		/* f_test 3 debug
		if(uio->uio_rw == UIO_READ && diskblock > 6){
                	for(i=0; i<len; i++){
                        	ch = (iobuf+skipstart)[i];
                        	if(ch != 'w' && ch != 'r') kprintf("Read Error on diskblock %d - iobuf[%d]: %c\n", diskblock, i, ch);
                	}       
        	}
		*/
	}

	/*
	 * Now perform the requested operation into/out of the buffer.
	 */
	result = uiomove(iobuf+skipstart, len, uio);
	if (result) {
		return result;
	}
	
	/*
	 * If it was a write, write back the modified block.
	 */
	if (uio->uio_rw == UIO_WRITE) {
		result = sfs_wblock(sfs, iobuf, diskblock);
		if (result) {
			return result;
		}
	}

	return 0;
}

/*
 * Do I/O (either read or write) of a single whole block.
 */
static
int
sfs_blockio(struct sfs_vnode *sv, struct uio *uio)
{
	struct sfs_fs *sfs = sv->sv_v.vn_fs->fs_data;
	u_int32_t diskblock;
	u_int32_t fileblock;
	int result;
	int doalloc = (uio->uio_rw==UIO_WRITE);
	off_t saveoff;
	off_t diskoff;
	off_t saveres;
	off_t diskres;

	/* Get the block number within the file */
	fileblock = uio->uio_offset / SFS_BLOCKSIZE;

	/* Look up the disk block number */
	result = sfs_bmap(sv, fileblock, doalloc, &diskblock);
	if (result) {
		return result;
	}

	if (diskblock == 0) {
		/*
		 * No block - fill with zeros.
		 *
		 * We must be reading, or sfs_bmap would have
		 * allocated a block for us.
		 */
		assert(uio->uio_rw == UIO_READ);
		return uiomovezeros(SFS_BLOCKSIZE, uio);
	}

	/*
	 * Do the I/O directly to the uio region. Save the uio_offset,
	 * and substitute one that makes sense to the device.
	 */
	saveoff = uio->uio_offset;
	diskoff = diskblock * SFS_BLOCKSIZE;
	uio->uio_offset = diskoff;

	/*
	 * Temporarily set the residue to be one block size.
	 */
	assert(uio->uio_resid >= SFS_BLOCKSIZE);
	saveres = uio->uio_resid;
	diskres = SFS_BLOCKSIZE;
	uio->uio_resid = diskres;
	
	result = sfs_rwblock(sfs, uio);

	/*
	 * Now, restore the original uio_offset and uio_resid and update 
	 * them by the amount of I/O done.
	 */
	uio->uio_offset = (uio->uio_offset - diskoff) + saveoff;
	uio->uio_resid = (uio->uio_resid - diskres) + saveres;

	return result;
}

/*
 * Do I/O of a whole region of data, whether or not it's block-aligned.
 */
static
int
sfs_io(struct sfs_vnode *sv, struct uio *uio)
{
	u_int32_t blkoff;
	u_int32_t nblocks, i;
	int result = 0;
	u_int32_t extraresid = 0;

	/*
	 * If reading, check for EOF. If we can read a partial area,
	 * remember how much extra there was in EXTRARESID so we can
	 * add it back to uio_resid at the end.
	 */
	if (uio->uio_rw == UIO_READ) {
		off_t size = sv->sv_i.sfi_size;
		off_t endpos = uio->uio_offset + uio->uio_resid;

		if (uio->uio_offset >= size) {
			/* At or past EOF - just return */
			return 0;
		}

		if (endpos > size) {
			extraresid = endpos - size;
			assert(uio->uio_resid > extraresid);
			uio->uio_resid -= extraresid;
		}
	}
	
	/*
	 * First, do any leading partial block.
	 */
	blkoff = uio->uio_offset % SFS_BLOCKSIZE;
	if (blkoff != 0) {
		/* Number of bytes at beginning of block to skip */
		u_int32_t skip = blkoff;

		/* Number of bytes to read/write after that point */
		u_int32_t len = SFS_BLOCKSIZE - blkoff;

		/* ...which might be less than the rest of the block */
		if (len > uio->uio_resid) {
			len = uio->uio_resid;
		}

		/* Call sfs_partialio() to do it. */
		result = sfs_partialio(sv, uio, skip, len);
		if (result) {
			goto out;
		}
	}

	/* If we're done, quit. */
	if (uio->uio_resid==0) {
		goto out;
	}

	/*
	 * Now we should be block-aligned. Do the remaining whole blocks.
	 */
	assert(uio->uio_offset % SFS_BLOCKSIZE == 0);
	nblocks = uio->uio_resid / SFS_BLOCKSIZE;
	for (i=0; i<nblocks; i++) {
		result = sfs_blockio(sv, uio);
		if (result) {
			goto out;
		}
	}

	/*
	 * Now do any remaining partial block at the end.
	 */
	assert(uio->uio_resid < SFS_BLOCKSIZE);

	if (uio->uio_resid > 0) {
		result = sfs_partialio(sv, uio, 0, uio->uio_resid);
		if (result) {
			goto out;
		}
	}

 out:

	/* If writing, adjust file length */
	if (uio->uio_rw == UIO_WRITE && 
	    uio->uio_offset > (off_t)sv->sv_i.sfi_size) {
		sv->sv_i.sfi_size = uio->uio_offset;
		sv->sv_dirty = 1;
	}

	/* Add in any extra amount we couldn't read because of EOF */
	uio->uio_resid += extraresid;

	/* Done */
	return result;
}

////////////////////////////////////////////////////////////
//
// Directory I/O

/*
 * Read the directory entry out of slot SLOT of a directory vnode.
 * The "slot" is the index of the directory entry, starting at 0.
 */
static
int
sfs_readdir(struct sfs_vnode *sv, struct sfs_dir *sd, int slot)
{
	struct uio ku;
	off_t actualpos;
	int result;

	/* Compute the actual position in the directory to read. */
	actualpos = slot * sizeof(struct sfs_dir);

	/* Set up a uio to do the read */ 
	mk_kuio(&ku, sd, sizeof(struct sfs_dir), actualpos, UIO_READ);

	/* do it */
	result = sfs_io(sv, &ku);
	if (result) {
		return result;
	}

	/* We should not hit EOF in the middle of a directory entry */
	if (ku.uio_resid > 0) {
		panic("sfs: readdir: Short entry (inode %u)\n", sv->sv_ino);
	}

	/* Done */
	return 0;
}

/*
 * Write (overwrite) the directory entry in slot SLOT of a directory
 * vnode.
 */
static
int
sfs_writedir(struct sfs_vnode *sv, struct sfs_dir *sd, int slot)
{
	struct uio ku;
	off_t actualpos;
	int result;

	/* Compute the actual position in the directory. */
	assert(slot>=0);
	actualpos = slot * sizeof(struct sfs_dir);

	/* Set up a uio to do the write */ 
	mk_kuio(&ku, sd, sizeof(struct sfs_dir), actualpos, UIO_WRITE);

	/* do it */
	result = sfs_io(sv, &ku);
	if (result) {
		return result;
	}

	/* Should not end up with a partial entry! */
	if (ku.uio_resid > 0) {
		panic("sfs: writedir: Short write (ino %u)\n", sv->sv_ino);
	}

	/* Done */
	return 0;
}

/*
 * Compute the number of entries in a directory.
 * This actually computes the number of existing slots, and does not
 * account for empty slots.
 */
static
int
sfs_dir_nentries(struct sfs_vnode *sv)
{
	off_t size;

	assert(sv->sv_i.sfi_type == SFS_TYPE_DIR);

	size = sv->sv_i.sfi_size;
	if (size % sizeof(struct sfs_dir) != 0) {
		panic("sfs: directory %u: Invalid size %u\n",
		      sv->sv_ino, size);
	}

	return size / sizeof(struct sfs_dir);
}

/*
 * Search a directory for a particular filename in a directory, and
 * return its inode number, its slot, and/or the slot number of an
 * empty directory slot if one is found.
 */

static
int
sfs_dir_findname(struct sfs_vnode *sv, const char *name,
		    u_int32_t *ino, int *slot, int *emptyslot)
{
	struct sfs_dir tsd;
	int found = 0;
	int nentries = sfs_dir_nentries(sv);
	int i, result;

	/* For each slot... */
	for (i=0; i<nentries; i++) {

		/* Read the entry from that slot */
		result = sfs_readdir(sv, &tsd, i);
		if (result) {
			return result;
		}
		if (tsd.sfd_ino == SFS_NOINO) {
			/* Free slot - report it back if one was requested */
			if (emptyslot != NULL) {
				*emptyslot = i;
			}
		}
		else {
			/* Ensure null termination, just in case */
			tsd.sfd_name[sizeof(tsd.sfd_name)-1] = 0;
			if (!strcmp(tsd.sfd_name, name)) {

				/* Each name may legally appear only once... */
				assert(found==0);

				found = 1;
				if (slot != NULL) {
					*slot = i;
				}
				if (ino != NULL) {
					*ino = tsd.sfd_ino;
				}
			}
		}
	}

	return found ? 0 : ENOENT;
}

/*
 * Create a link in a directory to the specified inode by number, with
 * the specified name, and optionally hand back the slot.
 */
static
int
sfs_dir_link(struct sfs_vnode *sv, const char *name, u_int32_t ino, int *slot)
{
	int emptyslot = -1;
	int result;
	struct sfs_dir sd;

	/* Look up the name. We want to make sure it *doesn't* exist. */
	result = sfs_dir_findname(sv, name, NULL, NULL, &emptyslot);
	if (result!=0 && result!=ENOENT) {
		return result;
	}
	if (result==0) {
		return EEXIST;
	}

	if (strlen(name)+1 > sizeof(sd.sfd_name)) {
		return ENAMETOOLONG;
	}

	/* If we didn't get an empty slot, add the entry at the end. */
	if (emptyslot < 0) {
		emptyslot = sfs_dir_nentries(sv);
	}

	/* Set up the entry. */
	bzero(&sd, sizeof(sd));
	sd.sfd_ino = ino;
	strcpy(sd.sfd_name, name);

	/* Hand back the slot, if so requested. */
	if (slot) {
		*slot = emptyslot;
	}

	/* Write the entry. */
	return sfs_writedir(sv, &sd, emptyslot);	
}

/*
 * Unlink a name in a directory, by slot number.
 */
static
int
sfs_dir_unlink(struct sfs_vnode *sv, int slot)
{
	struct sfs_dir sd;

	/* Initialize a suitable directory entry... */ 
	bzero(&sd, sizeof(sd));
	sd.sfd_ino = SFS_NOINO;

	/* ... and write it */
	return sfs_writedir(sv, &sd, slot);
}

/*
 * Look for a name in a directory and hand back a vnode for the
 * file, if there is one.
 */
static
int
sfs_lookonce(struct sfs_vnode *sv, const char *name, 
		struct sfs_vnode **ret,
		int *slot)
{
	struct sfs_fs *sfs = sv->sv_v.vn_fs->fs_data;
	u_int32_t ino;
	int result;

	result = sfs_dir_findname(sv, name, &ino, slot, NULL);
	if (result) {
		return result;
	}

	result = sfs_loadvnode(sfs, ino, SFS_TYPE_INVAL, ret);
	if (result) {
		return result;
	}

	if ((*ret)->sv_i.sfi_linkcount == 0) {
		panic("sfs: Link count of file %u found in dir %u is 0\n",
		      (*ret)->sv_ino, sv->sv_ino);
	}

	return 0;
}

////////////////////////////////////////////////////////////
//
// Object creation

/*
 * Create a new filesystem object and hand back its vnode.
 */
static
int
sfs_makeobj(struct sfs_fs *sfs, int type, struct sfs_vnode **ret)
{
	u_int32_t ino;
	int result;

	/*
	 * First, get an inode. (Each inode is a block, and the inode 
	 * number is the block number, so just get a block.)
	 */

	result = sfs_balloc(sfs, &ino);
	if (result) {
		return result;
	}

	/*
	 * Now load a vnode for it.
	 */

	return sfs_loadvnode(sfs, ino, type, ret);
}

////////////////////////////////////////////////////////////
//
// Vnode ops

/*
 * This is called on *each* open().
 */
static
int
sfs_open(struct vnode *v, int openflags)
{
	/*
	 * At this level we do not need to handle O_CREAT, O_EXCL, or O_TRUNC.
	 * We *would* need to handle O_APPEND, but we don't support it.
	 *
	 * Any of O_RDONLY, O_WRONLY, and O_RDWR are valid, so we don't need
	 * to check that either.
	 */

	if (openflags & O_APPEND) {
		return EUNIMP;
	}

	(void)v;

	return 0;
}

/*
 * This is called on *each* open() of a directory.
 * Directories may only be open for read.
 */
static
int
sfs_opendir(struct vnode *v, int openflags)
{
	switch (openflags & O_ACCMODE) {
	    case O_RDONLY:
		break;
	    case O_WRONLY:
	    case O_RDWR:
	    default:
		return EISDIR;
	}
	if (openflags & O_APPEND) {
		return EISDIR;
	}

	(void)v;
	return 0;
}

/*
 * Called on the *last* close().
 *
 * This function should attempt to avoid returning errors, as handling
 * them usefully is often not possible.
 */
static
int
sfs_close(struct vnode *v)
{
	/* Sync it. */
	return VOP_FSYNC(v);
}

/*
 * Called when the vnode refcount (in-memory usage count) hits zero.
 *
 * This function should try to avoid returning errors other than EBUSY.
 */
static
int
sfs_reclaim(struct vnode *v)
{
	struct sfs_vnode *sv = v->vn_data;
	struct sfs_fs *sfs = v->vn_fs->fs_data;
	int ix, i, num, result;

	lock_acquire(sfs->sfs_vnodes_lock);

	/*
	 * Make sure someone else hasn't picked up the vnode since the
	 * decision was made to reclaim it. (You must also synchronize
	 * this with sfs_loadvnode.)
	 */
	lock_acquire(v->vn_countlock);
	if (v->vn_refcount != 1) {

		/* consume the reference VOP_DECREF gave us */
		assert(v->vn_refcount>1);
		v->vn_refcount--;

		lock_release(v->vn_countlock);
		lock_release(sfs->sfs_vnodes_lock);

		return EBUSY;
	}
	lock_release(v->vn_countlock);
	

	/* If there are no on-disk references to the file either, erase it. */
	if (sv->sv_i.sfi_linkcount==0 && sv->sv_i.sfi_type==SFS_TYPE_FILE) {
		/* 
		 * VOP_TRUNCATE doesn't work on directories, which is why I added
		 * the second requirement to the above if statement.
		 */
		result = VOP_TRUNCATE(&sv->sv_v, 0);
		if (result) {
			lock_release(sfs->sfs_vnodes_lock);
			return result;
		}
	}

	/* Sync the inode to disk */
	result = sfs_sync_inode(sv);
	if (result) {
		lock_release(sfs->sfs_vnodes_lock);
		return result;
	}

	/* If there are no on-disk references, discard the inode */
	if (sv->sv_i.sfi_linkcount==0) {
		sfs_bfree(sfs, sv->sv_ino);
	}

	/* Remove the vnode structure from the table in the struct sfs_fs. */
	ix = -1;
	num = array_getnum(sfs->sfs_vnodes);
	for (i=0; i<num; i++) {
		struct sfs_vnode *sv2 = array_getguy(sfs->sfs_vnodes, i);
		if (sv2==sv) {
			ix = i;
			break;
		}
	}
	if (ix<0) {
		panic("sfs: reclaim vnode %u not in vnode pool\n",
		      sv->sv_ino);
	}
	array_remove(sfs->sfs_vnodes, ix);

	VOP_KILL(&sv->sv_v);

	/* Release the storage for the vnode structure itself. */
	kfree(sv);

	lock_release(sfs->sfs_vnodes_lock);

	/* Done */
	return 0;
}

/*
 * Called for read(). sfs_io() does the work.
 */
static
int
sfs_read(struct vnode *v, struct uio *uio)
{
	struct sfs_vnode *sv = v->vn_data;
	assert(uio->uio_rw==UIO_READ);
	return sfs_io(sv, uio);
}

/*
 * Called for write(). sfs_io() does the work.
 */
static
int
sfs_write(struct vnode *v, struct uio *uio)
{
	struct sfs_vnode *sv = v->vn_data;
	assert(uio->uio_rw==UIO_WRITE);
	return sfs_io(sv, uio);
}

/*
 * Called for ioctl()
 */
static
int
sfs_ioctl(struct vnode *v, int op, userptr_t data)
{
	/*
	 * No ioctls.
	 */

	(void)v;
	(void)op;
	(void)data;

	return EINVAL;
}

/*
 * Called for stat/fstat/lstat.
 */
static
int
sfs_stat(struct vnode *v, struct stat *statbuf)
{
	struct sfs_vnode *sv = v->vn_data;
	int result;

	/* Fill in the stat structure */
	bzero(statbuf, sizeof(struct stat));

	result = VOP_GETTYPE(v, &statbuf->st_mode);
	if (result) {
		return result;
	}

	statbuf->st_size = sv->sv_i.sfi_size;

	/* We don't support these yet; you get to implement them */
	statbuf->st_nlink = 0;
	statbuf->st_blocks = 0;

	return 0;
}

/*
 * Return the type of the file (types as per kern/stat.h)
 */
static
int
sfs_gettype(struct vnode *v, u_int32_t *ret)
{
	struct sfs_vnode *sv = v->vn_data;
	switch (sv->sv_i.sfi_type) {
	case SFS_TYPE_FILE:
		*ret = S_IFREG;
		return 0;
	case SFS_TYPE_DIR:
		*ret = S_IFDIR;
		return 0;
	}
	panic("sfs: gettype: Invalid inode type (inode %u, type %u)\n",
	      sv->sv_ino, sv->sv_i.sfi_type);
	return EINVAL;
}

/*
 * Check for legal seeks on files. Allow anything non-negative.
 * We could conceivably, here, prohibit seeking past the maximum
 * file size our inode structure can support, but we don't - few
 * people ever bother to check lseek() for failure and having 
 * read() or write() fail is sufficient.
 */
static
int
sfs_tryseek(struct vnode *v, off_t pos)
{
	if (pos<0) {
		return EINVAL;
	}

	/* Allow anything else */
	(void)v;

	return 0;
}

/*
 * Called for fsync(), and also on filesystem unmount, global sync(),
 * and some other cases.
 */
static
int
sfs_fsync(struct vnode *v)
{
	struct sfs_vnode *sv = v->vn_data;
	return sfs_sync_inode(sv);
}

/*
 * Called for mmap().
 */
static
int
sfs_mmap(struct vnode *v   /* add stuff as needed */)
{
	(void)v;
	return EUNIMP;
}

/*
 * Called for ftruncate() and from sfs_reclaim.
 */
static
int
sfs_truncate(struct vnode *v, off_t len)
{
	/*
	 * I/O buffer for handling the indirect block.
	 *
	 * Note: in real life (and when you've done the fs assignment)
	 * you would get space from the disk buffer cache for this,
	 * not use a static area.
	 */
	static u_int32_t idbuf[SFS_DBPERIDB];

	struct sfs_vnode *sv = v->vn_data;
	struct sfs_fs *sfs = sv->sv_v.vn_fs->fs_data;

	/* Length in blocks (divide rounding up) */
	u_int32_t blocklen = DIVROUNDUP(len, SFS_BLOCKSIZE);

	u_int32_t i, j, block;
	u_int32_t idblock, baseblock, highblock;
	int result;
	int hasnonzero, iddirty;

	assert(sizeof(idbuf)==SFS_BLOCKSIZE);

	/*
	 * Go through the direct blocks. Discard any that are
	 * past the limit we're truncating to.
	 */
	for (i=0; i<SFS_NDIRECT; i++) {
		block = sv->sv_i.sfi_direct[i];
		if (i >= blocklen && block != 0) {
			sfs_bfree(sfs, block);
			sv->sv_i.sfi_direct[i] = 0;
			sv->sv_dirty = 1;
		}
	}

	/* Indirect block number */
	idblock = sv->sv_i.sfi_indirect;

	/* The lowest block in the indirect block */
	baseblock = SFS_NDIRECT;

	/* The highest block in the indirect block */
	highblock = baseblock + SFS_DBPERIDB - 1;

	if (blocklen < highblock && idblock != 0) {
		/* We're past the proposed EOF; may need to free stuff */

		/* Read the indirect block */
		result = sfs_rblock(sfs, idbuf, idblock);
		if (result) {
			return result;
		}
		
		hasnonzero = 0;
		iddirty = 0;
		for (j=0; j<SFS_DBPERIDB; j++) {
			/* Discard any blocks that are past the new EOF */
			if (blocklen < baseblock+j && idbuf[j] != 0) {
				sfs_bfree(sfs, idbuf[j]);
				idbuf[j] = 0;
				iddirty = 1;
			}
			/* Remember if we see any nonzero blocks in here */
			if (idbuf[j]!=0) {
				hasnonzero=1;
			}
		}

		if (!hasnonzero) {
			/* The whole indirect block is empty now; free it */
			sfs_bfree(sfs, idblock);
			sv->sv_i.sfi_indirect = 0;
			sv->sv_dirty = 1;
		}
		else if (iddirty) {
			/* The indirect block is dirty; write it back */
			result = sfs_wblock(sfs, idbuf, idblock);
			if (result) {
				return result;
			}
		}
	}

	/* Set the file size */
	sv->sv_i.sfi_size = len;

	/* Mark the inode dirty */
	sv->sv_dirty = 1;
	
	return 0;
}

/*
 * Get the full pathname for a file. This only needs to work on directories.
 * Since we don't support subdirectories, assume it's the root directory
 * and hand back the empty string. (The VFS layer takes care of the
 * device name, leading slash, etc.)
 */
static
int
sfs_namefile(struct vnode *vv, struct uio *uio)
{
	/* 
	 * 1. All you really have is inode number of directory passed in.
	 * 2. Get inode number of parent by reading directory slot 0 (..)
	 * 3. loadvnode of parent
	 * 4. Get our name by reading through parent directory for our inode
	 *    number
	 * 5. Add this name to string
	 * 6. Repeat 2 through 5 until we hit root
	 * 7. uiomove
	 *	Deal with reference counters as you need to, as loadvnode
	 *	increments refcount on vnode that we load. 
	 */
	
	struct sfs_vnode *sv = vv->vn_data;
	struct sfs_vnode *child_dir, *parent_dir;
	struct sfs_dir tsd;
	int err, nentries, slot;
	char to_add[SFS_NAMELEN + 1], pathname[(SFS_NAMELEN + 1) * SFS_DIR_DEPTH];
	
	/* If we're root, do nothing */
	if(sv->sv_ino == SFS_ROOT_LOCATION) return 0;

	child_dir = sv;
	VOP_INCREF(&child_dir->sv_v);

	while(1){
		err = sfs_readdir(child_dir, &tsd, 1);
        	if(err) return err;	

		assert(!strcmp(tsd.sfd_name, ".."));

		err = sfs_loadvnode(child_dir->sv_v.vn_fs->fs_data, tsd.sfd_ino, SFS_TYPE_INVAL, &parent_dir);
		if(err) return err;

		nentries = sfs_dir_nentries(parent_dir);
		slot = 2;
		while (slot < nentries){
			err = sfs_readdir(parent_dir, &tsd, slot);
        		if(err) return err;

			if(tsd.sfd_ino == child_dir->sv_ino) break;
                	slot++;
        	}

		/* 
	 	 * Doesn't make sense if we don't find our directory listed in our
	 	 * parent directory..
	 	 */
        	assert(slot < nentries);

		strcpy(to_add, tsd.sfd_name);
		strcat(to_add, "/");
		strcat(to_add, pathname);
		strcpy(pathname, to_add);
	
		VOP_DECREF(&child_dir->sv_v);
		if(parent_dir->sv_ino == SFS_ROOT_LOCATION){
			VOP_DECREF(&parent_dir->sv_v);
                        break;
		} else child_dir = parent_dir;
	}

	err = uiomove(pathname, strlen(pathname) + 1, uio);
	if(err) return err;

	return 0;
}

/*
 * Create a file. If EXCL is set, insist that the filename not already
 * exist; otherwise, if it already exists, just open it.
 */
static
int
sfs_creat(struct vnode *v, const char *name, int excl, struct vnode **ret)
{
	struct sfs_fs *sfs = v->vn_fs->fs_data;
	struct sfs_vnode *sv = v->vn_data;
	struct sfs_vnode *newguy;
	u_int32_t ino;
	int result;

	/* Look up the name */
	result = sfs_dir_findname(sv, name, &ino, NULL, NULL);
	if (result!=0 && result!=ENOENT) {
		return result;
	}

	/* If it exists and we didn't want it to, fail */
	if (result==0 && excl) {
		return EEXIST;
	}

	if (result==0) {
		/* We got a file; load its vnode and return */
		result = sfs_loadvnode(sfs, ino, SFS_TYPE_INVAL, &newguy);
		if (result) {
			return result;
		}
		*ret = &newguy->sv_v;
		return 0;
	}

	/* Didn't exist - create it */
	result = sfs_makeobj(sfs, SFS_TYPE_FILE, &newguy);
	if (result) {
		return result;
	}

	/* Link it into the directory */
	result = sfs_dir_link(sv, name, newguy->sv_ino, NULL);
	if (result) {
		VOP_DECREF(&newguy->sv_v);
		return result;
	}

	/* Update the linkcount of the new file */
	newguy->sv_i.sfi_linkcount++;

	/* and consequently mark it dirty. */
	newguy->sv_dirty = 1;

	*ret = &newguy->sv_v;
	return 0;
}

/*
 * Make a hard link to a file.
 * The VFS layer should prevent this being called unless both
 * vnodes are ours.
 */
static
int
sfs_link(struct vnode *dir, const char *name, struct vnode *file)
{
	struct sfs_vnode *sv = dir->vn_data;
	struct sfs_vnode *f = file->vn_data;
	int result;

	assert(file->vn_fs == dir->vn_fs);

	/* Just create a link */
	result = sfs_dir_link(sv, name, f->sv_ino, NULL);
	if (result) {
		return result;
	}

	/* and update the link count, marking the inode dirty */
	f->sv_i.sfi_linkcount++;
	f->sv_dirty = 1;

	return 0;
}

/*
 * Delete a file.
 */
static
int
sfs_remove(struct vnode *dir, const char *name)
{
	struct sfs_vnode *sv = dir->vn_data;
	struct sfs_vnode *victim;
	int slot;
	int result;

	/* Look for the file and fetch a vnode for it. */
	result = sfs_lookonce(sv, name, &victim, &slot);
	if (result) {
		return result;
	}

	/* Erase its directory entry. */
	result = sfs_dir_unlink(sv, slot);
	if (result==0) {
		/* If we succeeded, decrement the link count. */
		assert(victim->sv_i.sfi_linkcount > 0);
		victim->sv_i.sfi_linkcount--;
		victim->sv_dirty = 1;
	}

	/* Discard the reference that sfs_lookonce got us */
	VOP_DECREF(&victim->sv_v);

	return result;
}

/*
 * Rename a file.
 */
static
int
sfs_rename(struct vnode *d1, const char *n1, 
	   struct vnode *d2, const char *n2)
{
	struct sfs_vnode *sv1 = d1->vn_data;
	struct sfs_vnode *sv2 = d2->vn_data;
	struct sfs_vnode *g1;
	int slot1, slot2;
	int result, result2;

	/* Look up the old name of the file and get its inode and slot number*/
	result = sfs_lookonce(sv1, n1, &g1, &slot1);
	if (result) {
		return result;
	}

	/*
	 * Link it to the 'new' directory under the new name.
	 *
	 * We could theoretically just overwrite the original
	 * directory entry, except that we need to check to make sure
	 * the new name doesn't already exist; might as well use the
	 * existing link routine.
	 */
	result = sfs_dir_link(sv2, n2, g1->sv_ino, &slot2);
	if (result) {
		goto puke;
	}
	
	/* Increment the link count, and mark inode dirty */
	g1->sv_i.sfi_linkcount++;
	g1->sv_dirty = 1;

	/* Unlink the old slot */
	result = sfs_dir_unlink(sv1, slot1);
	if (result) {
		goto puke_harder;
	}

	/*
	 * Decrement the link count again, and mark the inode dirty again,
	 * in case it's been synced behind our back.
	 */
	assert(g1->sv_i.sfi_linkcount>0);
	g1->sv_i.sfi_linkcount--;
	g1->sv_dirty = 1;

	/* Let go of the reference to g1 */
	VOP_DECREF(&g1->sv_v);

	return 0;

 puke_harder:
	/*
	 * Error recovery: try to undo what we already did
	 */
	result2 = sfs_dir_unlink(sv2, slot2);
	if (result2) {
		kprintf("sfs: rename: %s\n", strerror(result));
		kprintf("sfs: rename: while cleaning up: %s\n", 
			strerror(result2));
		panic("sfs: rename: Cannot recover\n");
	}
	g1->sv_i.sfi_linkcount--;
 puke:
	/* Let go of the reference to g1 */
	VOP_DECREF(&g1->sv_v);
	return result;
}

/*
 * lookparent returns the last path component as a string and the
 * directory it's in as a vnode.
 */
static
int
sfs_lookparent(struct vnode *v, char *path, struct vnode **ret,
		  char *buf, size_t buflen)
{
	struct sfs_vnode *sv = v->vn_data;
	struct sfs_vnode *parent, *child;
	char name[SFS_NAMELEN];
	int i, err;

	if (sv->sv_i.sfi_type != SFS_TYPE_DIR) {
		return ENOTDIR;
	}

	parent = sv;
	VOP_INCREF(&parent->sv_v);
	child = NULL; //just to be safe

	while(1){
		i = 0;
		while(path[i] != '/' && path[i] != '\0' && i < SFS_NAMELEN) i++;
		if(i >= SFS_NAMELEN){
			VOP_DECREF(&parent->sv_v);
			return ENAMETOOLONG;
		}
		else if(path[i] == '/'){
			path[i] = 0;
			strcpy(name, path);
			path = &path[i + 1];

			err = sfs_lookonce(parent, name, &child, NULL);
			if(err){
				VOP_DECREF(&parent->sv_v);
				return err;
			}

			VOP_DECREF(&parent->sv_v);
			parent = child;
			if(parent->sv_i.sfi_type != SFS_TYPE_DIR) return ENOTDIR;
		} else{ /* Hit NULL, so this is our last time through */
			assert(path[i] == '\0');
			break;
		}
	}

	if (strlen(path)+1 > buflen) {
		return ENAMETOOLONG;
	}
	strcpy(buf, path);

	*ret = &parent->sv_v;

	return 0;
}

/*
 * Lookup gets a vnode for a pathname.
 */
static
int
sfs_lookup(struct vnode *v, char *path, struct vnode **ret)
{
	struct sfs_vnode *sv = v->vn_data;
	struct sfs_vnode *parent, *child;
        char name[SFS_NAMELEN];
        int i, err;

        if (sv->sv_i.sfi_type != SFS_TYPE_DIR) {
                return ENOTDIR;
        }

        parent = sv; 
        VOP_INCREF(&parent->sv_v);
        child = NULL; //just to be safe

        while(1){
                i = 0;  
                while(path[i] != '/' && path[i] != '\0' && i < SFS_NAMELEN) i++;
                if(i >= SFS_NAMELEN){
                        VOP_DECREF(&parent->sv_v);
                        return ENAMETOOLONG;
                }
                else if(path[i] == '/'){
                        path[i] = 0;
                        strcpy(name, path);
                        path = &path[i + 1];
         
                        err = sfs_lookonce(parent, name, &child, NULL);
                        if(err){
                                VOP_DECREF(&parent->sv_v);
                                return err;
                        }

                        VOP_DECREF(&parent->sv_v);
                        parent = child;
                        if(parent->sv_i.sfi_type != SFS_TYPE_DIR) return ENOTDIR;
                } else{ /* Hit NULL, so this is our last time through */
                        assert(path[i] == '\0');
                        
			err = sfs_lookonce(parent, path, &child, NULL);
                        if(err){
                                VOP_DECREF(&parent->sv_v);
                                return err;
                        }       
         
                        VOP_DECREF(&parent->sv_v);

			break;
                }
        }

	*ret = &child->sv_v;

	return 0;
}

/* 
 * Based on uio->uio_offset, get the name of the next directory entry,
 * if one exists.
 */
static
int
sfs_getdirentry(struct vnode *vv, struct uio *uio)
{
	struct sfs_vnode *sv;
	struct sfs_dir tsd;
        int nentries, slot, err;

	assert(uio->uio_iovec.iov_ubase != NULL);
	assert(uio->uio_rw == UIO_READ);

	sv = vv->vn_data;
	if(sv->sv_i.sfi_type != SFS_TYPE_DIR) return ENOTDIR;

	nentries = sfs_dir_nentries(sv);
	if(nentries == 0) return ENOENT;	

	/* Get slot of interest */
	if((uio->uio_offset % sizeof(struct sfs_dir)) <= 4){
		/* Use dir entry in which we are currently pointing to */
		slot = uio->uio_offset/sizeof(struct sfs_dir);
	} else{
		/* Use next dir entry */
		slot = uio->uio_offset/sizeof(struct sfs_dir) + 1;
	}

	while(slot < nentries){
		err = sfs_readdir(sv, &tsd, slot);
		if(err) return err;

		if(tsd.sfd_ino != SFS_NOINO) break;
		slot++;
	}

	if(slot >= nentries){
		/* EOF - just return */
		return 0;
	}
	
	/* Set things up and do the move */
	tsd.sfd_name[sizeof(tsd.sfd_name)-1] = 0;
	uio->uio_offset = 0;
	err = uiomove(tsd.sfd_name, sizeof(tsd.sfd_name), uio);
	if(err) return err;

	/* Reset offset */
	uio->uio_offset = ((slot + 1) * sizeof(struct sfs_dir));

	return 0;
}

static
int
sfs_mkdir(struct vnode *vv, const char *name){
	struct sfs_fs *sfs = vv->vn_fs->fs_data;
        struct sfs_vnode *sv = vv->vn_data;
        struct sfs_vnode *newguy;
        u_int32_t ino;
        int result, result2, slot1, slot2;

        /* Look up the name */
        result = sfs_dir_findname(sv, name, &ino, NULL, NULL);
        if (result!=0 && result!=ENOENT) {
                return result;
        }

        if (result==0) {
                /* Directory or file of same name already exists */
                return EINVAL;
        }

        /* Didn't exist - create it */
        result = sfs_makeobj(sfs, SFS_TYPE_DIR, &newguy);
        if (result) {
                return result;
        }

        /* Link it into the directory */
        result = sfs_dir_link(sv, name, newguy->sv_ino, &slot1);
        if (result) {
                VOP_DECREF(&newguy->sv_v);
                return result;
        }

        /* Increment linkcount of the new directory and mark it dirty */
        newguy->sv_i.sfi_linkcount++;
        newguy->sv_dirty = 1;

	/* Link . and .. into our directory */
	result = sfs_dir_link(newguy, ".", newguy->sv_ino, &slot2);
        if (result) {
		result2 = sfs_dir_unlink(sv, slot1);
                if(result2){
			VOP_DECREF(&newguy->sv_v);
			return result2;
		}
		newguy->sv_i.sfi_linkcount--;
        	newguy->sv_dirty = 1;
		VOP_DECREF(&newguy->sv_v);
                return result;
        }

	newguy->sv_i.sfi_linkcount++;
        newguy->sv_dirty = 1;

	result = sfs_dir_link(newguy, "..", sv->sv_ino, NULL);
        if (result) {
		result2 = sfs_dir_unlink(newguy, slot2);
		if(result2){
			VOP_DECREF(&newguy->sv_v);
			return result2;
		}
                newguy->sv_i.sfi_linkcount--; 
                newguy->sv_dirty = 1;
                result2 = sfs_dir_unlink(sv, slot1);
		if(result2){
                        VOP_DECREF(&newguy->sv_v);
                        return result2;
                }
                newguy->sv_i.sfi_linkcount--;
                newguy->sv_dirty = 1;
		VOP_DECREF(&newguy->sv_v);
                return result;
        }

        sv->sv_i.sfi_linkcount++;
        sv->sv_dirty = 1;

	VOP_DECREF(&newguy->sv_v);

	return 0;
}

static
int
sfs_rmdir(struct vnode *vv, const char *name){
	struct sfs_vnode *sv = vv->vn_data;
        struct sfs_vnode *victim;
	struct sfs_dir tsd;
        int slot, slot2, nentries;
        int result;

        /* Look for the directory and fetch a vnode for it. */
        result = sfs_lookonce(sv, name, &victim, &slot);
        if (result) {
                return result;
        }

	/* Make sure it's a directory and that only . and .. are left */
	if(victim->sv_i.sfi_type != SFS_TYPE_DIR){
		VOP_DECREF(&victim->sv_v);
		return ENOTDIR;
	}

	nentries = sfs_dir_nentries(victim);
	if(nentries != 2){
		slot2 = 2; 
		while(slot2 < nentries){
			/* Ensure all other entries are blank */
			result = sfs_readdir(victim, &tsd, slot2);
                	if(result){
				VOP_DECREF(&victim->sv_v);
				return result;
			}

                	if(tsd.sfd_ino != SFS_NOINO){
				VOP_DECREF(&victim->sv_v);
                                return ENOTEMPTY;
			}
                	slot2++;
		}	
	}

	/* Get rid of . and .. (should always be in slot 0 and 1) */
	result = sfs_dir_unlink(victim, 0);
	if(result){
		VOP_DECREF(&victim->sv_v);
		return result;
	}
	assert(victim->sv_i.sfi_linkcount > 0);
        victim->sv_i.sfi_linkcount--;
        victim->sv_dirty = 1;

	result = sfs_dir_unlink(victim, 1);
        if(result){
                VOP_DECREF(&victim->sv_v);
                return result;
        }       
	assert(sv->sv_i.sfi_linkcount > 0);
        sv->sv_i.sfi_linkcount--;
        sv->sv_dirty = 1;

        /* Erase its directory entry. */
        result = sfs_dir_unlink(sv, slot);
        if (result==0) {
                /* If we succeeded, decrement the link count. */
                assert(victim->sv_i.sfi_linkcount > 0);
                victim->sv_i.sfi_linkcount--;
                victim->sv_dirty = 1;
        }

        /* Discard the reference that sfs_lookonce got us */
        VOP_DECREF(&victim->sv_v);

	return result;
}

//////////////////////////////////////////////////

static
int
sfs_notdir(void)
{
	return ENOTDIR;
}

static
int
sfs_isdir(void)
{
	return EISDIR;
}

static
int
sfs_unimp(void)
{
	return EUNIMP;
}

/*
 * Casting through void * prevents warnings.
 * All of the vnode ops return int, and it's ok to cast functions that
 * take args to functions that take no args.
 */

#define ISDIR ((void *)sfs_isdir)
#define NOTDIR ((void *)sfs_notdir)
#define UNIMP ((void *)sfs_unimp)

/*
 * Function table for sfs files.
 */
static const struct vnode_ops sfs_fileops = {
	VOP_MAGIC,	/* mark this a valid vnode ops table */

	sfs_open,
	sfs_close,
	sfs_reclaim,

	sfs_read,
	NOTDIR,  /* readlink */
	NOTDIR,  /* getdirentry */
	sfs_write,
	sfs_ioctl,
	sfs_stat,
	sfs_gettype,
	sfs_tryseek,
	sfs_fsync,
	sfs_mmap,
	sfs_truncate,
	NOTDIR,  /* namefile */

	NOTDIR,  /* creat */
	NOTDIR,  /* symlink */
	NOTDIR,  /* mkdir */
	NOTDIR,  /* link */
	NOTDIR,  /* remove */
	NOTDIR,  /* rmdir */
	NOTDIR,  /* rename */

	NOTDIR,  /* lookup */
	NOTDIR,  /* lookparent */
};

/*
 * Function table for the sfs directory.
 */
static const struct vnode_ops sfs_dirops = {
	VOP_MAGIC,	/* mark this a valid vnode ops table */

	sfs_opendir,
	sfs_close,
	sfs_reclaim,
	
	ISDIR,   /* read */
	ISDIR,   /* readlink */
	sfs_getdirentry,   /* getdirentry */
	ISDIR,   /* write */
	sfs_ioctl,
	sfs_stat,
	sfs_gettype,
	UNIMP,   /* tryseek */
	sfs_fsync,
	ISDIR,   /* mmap */
	ISDIR,   /* truncate */
	sfs_namefile,

	sfs_creat,
	UNIMP,   /* symlink */
	sfs_mkdir,   /* mkdir */
	sfs_link,
	sfs_remove,
	sfs_rmdir,   /* rmdir */
	sfs_rename,

	sfs_lookup,
	sfs_lookparent,
};

/*
 * Function to load a inode into memory as a vnode, or dig up one
 * that's already resident.
 */
static
int
sfs_loadvnode(struct sfs_fs *sfs, u_int32_t ino, int forcetype,
		 struct sfs_vnode **ret)
{
	struct sfs_vnode *sv;
	const struct vnode_ops *ops = NULL;
	int i, num;
	int result;

	lock_acquire(sfs->sfs_vnodes_lock);

	/* Look in the vnodes table */
	num = array_getnum(sfs->sfs_vnodes);

	/* Linear search. Is this too slow? You decide. */
	for (i=0; i<num; i++) {
		sv = array_getguy(sfs->sfs_vnodes, i);

		/* Every inode in memory must be in an allocated block */
		if (!sfs_bused(sfs, sv->sv_ino)) {
			panic("sfs: Found inode %u in unallocated block\n",
			      sv->sv_ino);
		}

		if (sv->sv_ino==ino) {
			/* Found */

			/* May only be set when creating new objects */
			assert(forcetype==SFS_TYPE_INVAL);

			VOP_INCREF(&sv->sv_v);

			lock_release(sfs->sfs_vnodes_lock);

			*ret = sv;
			return 0;
		}
	}

	/* Didn't have it loaded; load it */

	sv = kmalloc(sizeof(struct sfs_vnode));
	if (sv==NULL) {
		lock_release(sfs->sfs_vnodes_lock);
		return ENOMEM;
	}

	/* Must be in an allocated block */
	if (!sfs_bused(sfs, ino)) {
		panic("sfs: Tried to load inode %u from unallocated block\n",
		      ino);
	}

	/* Read the block the inode is in */
	result = sfs_rblock(sfs, &sv->sv_i, ino);
	if (result) {
		kfree(sv);
		lock_release(sfs->sfs_vnodes_lock);
		return result;
	}

	/* Not dirty yet */
	sv->sv_dirty = 0;

	/*
	 * FORCETYPE is set if we're creating a new file, because the
	 * block on disk will have been zeroed out and thus the type
	 * recorded there will be SFS_TYPE_INVAL.
	 */
	if (forcetype != SFS_TYPE_INVAL) {
		assert(sv->sv_i.sfi_type == SFS_TYPE_INVAL);
		sv->sv_i.sfi_type = forcetype;
		sv->sv_dirty = 1;
	}

	/*
	 * Choose the function table based on the object type.
	 */
	switch (sv->sv_i.sfi_type) {
	    case SFS_TYPE_FILE:
		ops = &sfs_fileops;
		break;
	    case SFS_TYPE_DIR:
		ops = &sfs_dirops;
		break;
	    default: 
		panic("sfs: loadvnode: Invalid inode type "
		      "(inode %u, type %u)\n",
		      ino, sv->sv_i.sfi_type);
	}

	/* Call the common vnode initializer */
	result = VOP_INIT(&sv->sv_v, ops, &sfs->sfs_absfs, sv);
	if (result) {
		kfree(sv);
		lock_release(sfs->sfs_vnodes_lock);
		return result;
	}

	/* Set the other fields in our vnode structure */
	sv->sv_ino = ino;

	/* Add it to our table */
	result = array_add(sfs->sfs_vnodes, sv);
	if (result) {
		VOP_KILL(&sv->sv_v);
		kfree(sv);
		lock_release(sfs->sfs_vnodes_lock);
		return result;
	}

	lock_release(sfs->sfs_vnodes_lock);

	/* Hand it back */
	*ret = sv;
	return 0;
}

/*
 * Get vnode for the root of the filesystem.
 * The root vnode is always found in block 1 (SFS_ROOT_LOCATION).
 */
struct vnode *
sfs_getroot(struct fs *fs)
{
	struct sfs_fs *sfs = fs->fs_data;
	struct sfs_vnode *sv;
	int result;

	result = sfs_loadvnode(sfs, SFS_ROOT_LOCATION, SFS_TYPE_INVAL, &sv);
	if (result) {
		panic("sfs: getroot: Cannot load root vnode\n");
	}

	return &sv->sv_v;
}

/* Setup . and .. in root directory if this is the first mount */
int
sfs_setup_root(struct sfs_fs *sfs)
{
	struct fs *fs = &sfs->sfs_absfs;
	struct vnode *root_vv;
	struct sfs_vnode *root_sv;
	int result;

	root_vv = sfs_getroot(fs);
	root_sv = root_vv->vn_data;

	if(sfs_dir_nentries(root_sv) != 0){
		VOP_DECREF(root_vv);
		return 0;
	}

	result = sfs_dir_link(root_sv, ".", SFS_ROOT_LOCATION, NULL);
        if (result) {
                VOP_DECREF(root_vv);
                return result;
        }

        root_sv->sv_i.sfi_linkcount++;
        root_sv->sv_dirty = 1;

        result = sfs_dir_link(root_sv, "..", SFS_ROOT_LOCATION, NULL);
        if (result) {
                sfs_dir_unlink(root_sv, 0); /* Hope this doesn't fail?? */
                root_sv->sv_i.sfi_linkcount--;
                root_sv->sv_dirty = 1;
                VOP_DECREF(root_vv);
                return result;
        }

        root_sv->sv_i.sfi_linkcount++;
        root_sv->sv_dirty = 1;	

	/* Decrement reference that sfs_getroot incremented */
	VOP_DECREF(root_vv);

	return 0;
}
