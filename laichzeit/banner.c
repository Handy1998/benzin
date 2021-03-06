/*****************************************************************************
 *  Laichzeit                                                                *
 *  Part of the Benzin Project                                               *
 *  Handles some XML stuff.                                                  *
 *  Copyright (C)2009 SquidMan (Alex Marshall)       <SquidMan72@gmail.com>  *
 *  Copyright (C)2009 megazig  (Stephen Simpson)      <megazig@hotmail.com>  *
 *  Copyright (C)2009 Matt_P   (Matthew Parlane)                             *
 *  Copyright (C)2009 comex                                                  *
 *  Copyright (C)2009 booto                                                  *
 *  All Rights Reserved, HACKERCHANNEL.                                      *
 * ------------------------------------------------------------------------- *
 *  banner.c                                                                 *
 *  The code for all the final banner stuff. Compile this!                   *
 * ------------------------------------------------------------------------- *
 *  Compilation code contributed by:                                         *
 *          icefire                                                          *
 *  Intermediation code contributed by:                                      *
 *          SquidMan                                                         *
 *****************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>

#include "types.h"
#include "banner.h"
#include "c_oop.h"

#define inlinepad(x, y) ((x) + ((y) - ((x) % (y))))

typedef struct
{
	u16 type;		// This is 0x0000 for files, 0x0100 for folders
	u16 string_offset;	// Offset into the string table,
				// from the start of the string table.
	u32 data_offset;	// Absolute offset from U.8- header for files.
				// Recursion for directories.
	u32 size;		// Size of file for files.
				// Last file entry number for directories.
} u8node;

typedef struct
{
	u32 tag;		// 0x55AA382D "U.8-"
	u32 root_offset;	// Offset to root_node, always 0x20.
	u32 header_size;	// Size of header from root_node to 
				// end of string table.
	u32 data_offset;	// Offset to data 
				// root_offset + header_size, aligned to 0x40.
	u8 zeroes[16];		// Padding
} u8header;

typedef struct
{
	u8header header;
	u8node rootnode;
	u32 numnodes;
	u8node* nodes;
	u32 stringsz;
	u8* string_table;
	u32 datasz;
	u8* data;
} u8archive;

static void AddFileToDir(U8Dir* arc, U8File file)
{
	AddObjectToList((u8**)&arc->files, \
			(u32*)&arc->filecnt, \
			(u8*)&file, sizeof(U8File));
}

static U8Dir* AddDirToDir(U8Dir* arc, U8Dir dir)
{
	return &arc->dirs[AddObjectToList((u8**)&arc->dirs, \
					  (u32*)&arc->dircnt, \
					  (u8*)&dir, sizeof(U8Dir))];
}



IMET* LaichMeta_IMET(char** names, \
		     u32 iconsize, u32 bannersize, u32 soundsize)
{
	IMET* header		= malloc(sizeof(IMET));
	memset(header, 0, sizeof(IMET));
	header->imet[0]		= 'I';
	header->imet[1]		= 'M';
	header->imet[2]		= 'E';
	header->imet[3]		= 'T';
	header->unk		= htobll(0x0000060000000003);
	header->sizes[0]	= htobl(iconsize);
	header->sizes[1]	= htobl(bannersize);
	header->sizes[2]	= htobl(soundsize);
	header->flag1		= 0x00;
	
	int i;
	for(i = 0; i < 8; i++)
		memcpy(header->names[i], names[i], 2 * 42);
	
	MD5(((u8*)header) + 0x40, 0x600, (u8*)header->crypto);
	return header;
}

IMD5* LaichMeta_IMD5(u8* file, u32 size)
{
	IMD5* header = malloc(sizeof(IMD5));
	memset(header, 0, sizeof(IMD5));
	header->imd5[0]		= 'I';
	header->imd5[1]		= 'M';
	header->imd5[2]		= 'D';
	header->imd5[3]		= '5';
	header->filesize	= htobl(size);
	MD5(file, size, header->crypto);
	return header;
}

U8* LaichMeta_StartU8(U8Dir* root)
{
	U8* arc = malloc(sizeof(U8));
	arc->rootdir.filecnt	= 0;
	arc->rootdir.files	= NULL;
	arc->rootdir.dircnt	= 0;
	arc->rootdir.dirs	= NULL;
	memset(arc->rootdir.name, 0, 256);
	*root = arc->rootdir;
	return arc;
}

U8Dir* LaichMeta_CreateDirInDirU8(U8Dir* arc, char* name)
{
	U8Dir dir;
	dir.filecnt	= 0;
	dir.files	= NULL;
	dir.dircnt	= 0;
	dir.dirs	= NULL;
	memset(dir.name, 0, 256);
	strncpy(dir.name, name, 256);
	return AddDirToDir(arc, dir);
}

void LaichMeta_CreateFileInDirU8(U8Dir* arc, char* src, char* dst)
{
	U8File file;
	memset(file.src, 0, 256);
	strncpy(file.src, src, 256);
	memset(file.dst, 0, 256);
	strncpy(file.dst, dst, 256);
	AddFileToDir(arc, file);
}

static void U8_packdir(u8archive * arc, U8Dir dir, u32 recursion)
{
	u32 i, oldnumnodes;
	U8File curfile;
	U8Dir curdir;
	u8node curnode;
	
	oldnumnodes = arc->numnodes;
	
	arc->nodes = realloc(arc->nodes, arc->numnodes * sizeof(u8node));
	curnode = arc->nodes[arc->numnodes];
	
	curnode.type = 0x0001; // Is a folder.
	
	curnode.string_offset = htobl(arc->stringsz);
	arc->string_table = realloc(arc->string_table, strlen(curdir.name) \
				    + 1);
	memcpy(arc->string_table, curdir.name, strlen(curdir.name) + 1);
	arc->stringsz += strlen(curdir.name) + 1;
	arc->numnodes++;
	
	for(i = 0; i < dir.filecnt; i++) {	// Handle files.
		curfile = dir.files[i];
		
		arc->nodes = realloc(arc->nodes, \
				     arc->numnodes * sizeof(u8node));
		curnode = arc->nodes[arc->numnodes];
		
		curnode.type = 0; //file
		curnode.size = htobl(curfile.size);
		
		curnode.string_offset = htobl(arc->stringsz);
		arc->string_table = realloc(arc->string_table, \
					    strlen(curfile.dst) + 1);
		memcpy(arc->string_table, curfile.dst, \
		       strlen(curfile.dst) + 1);
		arc->stringsz += strlen(curfile.dst) + 1;
		
		curnode.data_offset = htobl(arc->datasz);
		arc->data = realloc(arc->data, inlinepad(curfile.size, 32));
		memcpy(arc->data, curfile.data, curfile.size);
		arc->datasz += inlinepad(curfile.size, 32);
		
		arc->numnodes++;
	}
	
	for(i = 0; i < dir.dircnt; i++) {	// Handle folders.
		curdir = dir.dirs[i];
		U8_packdir(arc, curdir, ++recursion);
	}
	
	// add one for root node
	arc->nodes[oldnumnodes].size = htobl(arc->numnodes + 1);
}

u8* LaichMeta_CompileU8(U8* input, u32* size)
{
	u8archive arc;
	u32 offset = 0, data_offset, i;
	u8* outbuf;
	
	arc.string_table = calloc(1, 1); //Initial NULL
	arc.stringsz = 1; //Initial NULL
	arc.data = NULL;
	arc.nodes = NULL;
	
	U8_packdir(&arc, input->rootdir, 0);
	
	arc.rootnode.type		= htobs(0x0100);
	arc.rootnode.string_offset	= 0;
	arc.rootnode.data_offset	= 0;
	arc.rootnode.size		= htobl(arc.numnodes);
	
	arc.header.tag			= htobl(0x55AA382D);
	arc.header.root_offset		= htobl(0x20);
	arc.header.header_size		= htobl( \
						(sizeof(u8node) * arc.numnodes) + \
						arc.stringsz);
	data_offset			= inlinepad(0x20 + \
						    (sizeof(u8node) * arc.numnodes) + \
						    arc.stringsz, 0x40);
	arc.header.data_offset		= htobl(data_offset);
	memset(arc.header.zeroes, 0, 16);
	
	for(i = 0; i < arc.numnodes; i++)	   // fix the data offset
		if (htobs(arc.nodes[i].type) == 0) // Checking if it is a file
			arc.nodes[i].data_offset = htobl(\
							 btohl(arc.nodes[i].data_offset) + \
							 data_offset);
	
	outbuf = calloc(1, sizeof(u8header) + sizeof(u8node) + \
			(sizeof(u8node) * arc.numnodes) + arc.stringsz + \
			arc.datasz);
	
	memcpy(outbuf + offset, &arc.header, sizeof(u8header));
	offset += sizeof(u8header);
	memcpy(outbuf + offset, &arc.rootnode, sizeof(u8node));
	offset += sizeof(u8node);
	memcpy(outbuf + offset, arc.nodes, sizeof(u8node) * arc.numnodes);
	offset += sizeof(u8node) * arc.numnodes;
	memcpy(outbuf + offset, arc.string_table, arc.stringsz);
	offset += arc.stringsz;
	memcpy(outbuf + offset, arc.data, arc.datasz);
	offset += arc.datasz;
	
	*size = offset;
	return outbuf;
}
