/* Copyright (c) 2000, 2018, Oracle and/or its affiliates.
   Copyright (c) 2010, 2020, MariaDB Corporation.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1335  USA */

#include "heapdef.h"
#include <my_bit.h>

static int keys_compare(void *heap_rb, const void *key1, const void *key2);
static void init_block(HP_BLOCK *block, size_t reclength, ulong min_records,
		       ulong max_records);


/*
  In how many parts are we going to do allocations of memory and indexes
  If we assign 1M to the heap table memory, we will allocate roughly
  (1M/16) bytes per allocation
*/
static const int heap_allocation_parts= 16;

/* min block allocation */
static const ulong heap_min_allocation_block= 16384;

/* Create a heap table */

int heap_create(const char *name, HP_CREATE_INFO *create_info,
                HP_SHARE **res, my_bool *created_new_share)
{
  uint i, j, key_segs, max_length, length;
  HP_SHARE *share= 0;
  HA_KEYSEG *keyseg;
  HP_KEYDEF *keydef= create_info->keydef;
  uint reclength= create_info->reclength;
  uint keys= create_info->keys;
  ulong min_records= create_info->min_records;
  ulong max_records= create_info->max_records;
  uint visible_offset;
  DBUG_ENTER("heap_create");

  if (!create_info->internal_table)
  {
    mysql_mutex_lock(&THR_LOCK_heap);
    share= hp_find_named_heap(name);
    if (share && share->open_count == 0)
    {
      hp_free(share);
      share= 0;
    }
  }  
  else
  {
    DBUG_PRINT("info", ("Creating internal (no named) temporary table"));
  }
  *created_new_share= (share == NULL);

  if (!share)
  {
    HP_KEYDEF *keyinfo;
    DBUG_PRINT("info",("Initializing new table"));
    
    /*
      We have to store sometimes uchar* del_link in records,
      so the visible_offset must be least at sizeof(uchar*)
    */
    visible_offset= MY_MAX(reclength, sizeof (char*));
    
    for (i= key_segs= max_length= 0, keyinfo= keydef; i < keys; i++, keyinfo++)
    {
      bzero((char*) &keyinfo->block,sizeof(keyinfo->block));
      bzero((char*) &keyinfo->rb_tree ,sizeof(keyinfo->rb_tree));
      for (j= length= 0; j < keyinfo->keysegs; j++)
      {
	length+= keyinfo->seg[j].length;
	if (keyinfo->seg[j].null_bit)
	{
	  length++;
	  if (!(keyinfo->flag & HA_NULL_ARE_EQUAL))
	    keyinfo->flag|= HA_NULL_PART_KEY;
	  if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
	    keyinfo->rb_tree.size_of_element++;
	}
	switch (keyinfo->seg[j].type) {
	case HA_KEYTYPE_SHORT_INT:
	case HA_KEYTYPE_LONG_INT:
	case HA_KEYTYPE_FLOAT:
	case HA_KEYTYPE_DOUBLE:
	case HA_KEYTYPE_USHORT_INT:
	case HA_KEYTYPE_ULONG_INT:
	case HA_KEYTYPE_LONGLONG:
	case HA_KEYTYPE_ULONGLONG:
	case HA_KEYTYPE_INT24:
	case HA_KEYTYPE_UINT24:
	case HA_KEYTYPE_INT8:
	  keyinfo->seg[j].flag|= HA_SWAP_KEY;
          break;
        case HA_KEYTYPE_VARBINARY1:
          /* Case-insensitiveness is handled in hash_sort */
          keyinfo->seg[j].type= HA_KEYTYPE_VARTEXT1;
          /* fall through */
        case HA_KEYTYPE_VARTEXT1:
          keyinfo->flag|= HA_VAR_LENGTH_KEY;
          /*
            For BTREE algorithm, key length, greater than or equal
            to 255, is packed on 3 bytes.
          */
          if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
            length+= size_to_store_key_length(keyinfo->seg[j].length);
          else
            length+= 2;
          /* Save number of bytes used to store length */
          keyinfo->seg[j].bit_start= 1;
          break;
        case HA_KEYTYPE_VARBINARY2:
          /* Case-insensitiveness is handled in hash_sort */
          /* fall_through */
        case HA_KEYTYPE_VARTEXT2:
          keyinfo->flag|= HA_VAR_LENGTH_KEY;
          /*
            For BTREE algorithm, key length, greater than or equal
            to 255, is packed on 3 bytes.
          */
          if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
            length+= size_to_store_key_length(keyinfo->seg[j].length);
          else
            length+= 2;
          /* Save number of bytes used to store length */
          keyinfo->seg[j].bit_start= 2;
          /*
            Make future comparison simpler by only having to check for
            one type
          */
          keyinfo->seg[j].type= HA_KEYTYPE_VARTEXT1;
          break;
        case HA_KEYTYPE_BIT:
          /*
            The odd bits which stored separately (if they are present
            (bit_pos, bit_length)) are already present in seg[j].length as
            additional byte.
            See field.h, function key_length()
          */
          break;
	default:
	  break;
	}
      }
      keyinfo->length= length;
      length+= keyinfo->rb_tree.size_of_element + 
	       ((keyinfo->algorithm == HA_KEY_ALG_BTREE) ? sizeof(uchar*) : 0);
      if (length > max_length)
	max_length= length;
      key_segs+= keyinfo->keysegs;
      if (keyinfo->algorithm == HA_KEY_ALG_BTREE)
      {
        key_segs++; /* additional HA_KEYTYPE_END segment */
        if (keyinfo->flag & HA_VAR_LENGTH_KEY)
          keyinfo->get_key_length= hp_rb_var_key_length;
        else if (keyinfo->flag & HA_NULL_PART_KEY)
          keyinfo->get_key_length= hp_rb_null_key_length;
        else
          keyinfo->get_key_length= hp_rb_key_length;
      }
    }
    if (!(share= (HP_SHARE*) my_malloc(hp_key_memory_HP_SHARE,
                                       sizeof(HP_SHARE)+
				       keys*sizeof(HP_KEYDEF)+
				       key_segs*sizeof(HA_KEYSEG),
				       MYF(MY_ZEROFILL |
                                           (create_info->internal_table ?
                                            MY_THREAD_SPECIFIC : 0)))))
      goto err;
    share->keydef= (HP_KEYDEF*) (share + 1);
    share->key_stat_version= 1;
    keyseg= (HA_KEYSEG*) (share->keydef + keys);
    init_block(&share->block, hp_memory_needed_per_row(reclength),
               min_records, max_records);
	/* Fix keys */
    memcpy(share->keydef, keydef, (size_t) (sizeof(keydef[0]) * keys));
    for (i= 0, keyinfo= share->keydef; i < keys; i++, keyinfo++)
    {
      keyinfo->seg= keyseg;
      memcpy(keyseg, keydef[i].seg,
	     (size_t) (sizeof(keyseg[0]) * keydef[i].keysegs));
      keyseg+= keydef[i].keysegs;

      if (keydef[i].algorithm == HA_KEY_ALG_BTREE)
      {
	/* additional HA_KEYTYPE_END keyseg */
	keyseg->type=     HA_KEYTYPE_END;
	keyseg->length=   sizeof(uchar*);
	keyseg->flag=     0;
	keyseg->null_bit= 0;
	keyseg++;

	init_tree(&keyinfo->rb_tree, 0, 0, sizeof(uchar*),
		  keys_compare, NULL, NULL,
                  MYF((create_info->internal_table ? MY_THREAD_SPECIFIC : 0) |
                      MY_TREE_WITH_DELETE));
	keyinfo->delete_key= hp_rb_delete_key;
	keyinfo->write_key= hp_rb_write_key;
      }
      else
      {
	init_block(&keyinfo->block, sizeof(HASH_INFO), min_records,
		   max_records);
	keyinfo->delete_key= hp_delete_key;
	keyinfo->write_key= hp_write_key;
        keyinfo->hash_buckets= 0;
      }
      if ((keyinfo->flag & HA_AUTO_KEY) && create_info->with_auto_increment)
        share->auto_key= i + 1;
    }
    share->min_records= min_records;
    share->max_records= max_records;
    share->max_table_size= create_info->max_table_size;
    share->data_length= share->index_length= 0;
    share->reclength= reclength;
    share->visible= visible_offset;
    share->blength= 1;
    share->keys= keys;
    share->max_key_length= max_length;
    share->changed= 0;
    share->auto_key= create_info->auto_key;
    share->auto_key_type= create_info->auto_key_type;
    share->auto_increment= create_info->auto_increment;
    share->create_time= (long) time((time_t*) 0);
    share->internal= create_info->internal_table;
    /* Must be allocated separately for rename to work */
    if (!(share->name= my_strdup(hp_key_memory_HP_SHARE, name, MYF(0))))
    {
      my_free(share);
      goto err;
    }

    if (!create_info->internal_table)
    {
      thr_lock_init(&share->lock);
      share->open_list.data= (void*) share;
      heap_share_list= list_add(heap_share_list,&share->open_list);
    }
    else
      share->delete_on_close= 1;
  }
  if (!create_info->internal_table)
  {
    if (create_info->pin_share)
      ++share->open_count;
    mysql_mutex_unlock(&THR_LOCK_heap);
  }

  *res= share;
  DBUG_RETURN(0);

err:
  if (!create_info->internal_table)
    mysql_mutex_unlock(&THR_LOCK_heap);
  DBUG_RETURN(1);
} /* heap_create */


static int keys_compare(void *heap_rb_, const void *key1_,
                        const void *key2_)
{
  heap_rb_param *heap_rb= heap_rb_;
  const uchar *key1= key1_;
  const uchar *key2= key2_;
  uint not_used[2];
  return ha_key_cmp(heap_rb->keyseg, key1, key2, heap_rb->key_length,
                    heap_rb->search_flag, not_used);
}


/*
  Calculate length needed for storing one row
*/

size_t hp_memory_needed_per_row(size_t reclength)
{
  /* Data needed for storing record + pointer to records */
  reclength= MY_MAX(reclength, sizeof(char*));
  /* The + 1 below is for the delete marker at the end of record*/
  reclength= MY_ALIGN(reclength+1, sizeof(char*));
  return reclength;
}

/*
  Calculate the number of rows that fits into a given memory size
*/

ha_rows hp_rows_in_memory(size_t reclength, size_t index_size,
                          size_t memory_limit)
{
  reclength= hp_memory_needed_per_row(reclength);
  if ((memory_limit < index_size + reclength + sizeof(HP_PTRS)))
    return 0;                                   /* Wrong arguments */
  return (ha_rows) ((memory_limit - sizeof(HP_PTRS)) /
                    (index_size + reclength));
}


static void init_block(HP_BLOCK *block, size_t reclength, ulong min_records,
		       ulong max_records)
{
  ulong i,records_in_block;
  ulong recbuffer= (ulong) MY_ALIGN(reclength, sizeof(uchar*));
  ulong extra;
  ulonglong memory_needed;
  size_t alloc_size;

  /*
    If not min_records and max_records are given, optimize for 1000 rows
  */
  if (!min_records)
    min_records= MY_MIN(1000, max_records / heap_allocation_parts);
  if (!max_records)
    max_records= MY_MAX(min_records, 1000);
  min_records= MY_MIN(min_records, max_records);

 /*
    We don't want too few records_in_block as otherwise the overhead of
    of the HP_PTRS block will be too notable
  */
  records_in_block= MY_MAX(min_records, max_records / heap_allocation_parts);

  /*
    Align allocation sizes to power of 2 to get less memory fragmentation from
    system alloc().
    As long as we have less than 128 allocations, all but one of the
    allocations will have an extra HP_PTRS size structure at the start
    of the block.

    We ensure that the block is not smaller than heap_min_allocation_block
    as otherwise we get strange results when max_records <
    heap_allocation_parts)
  */
  extra= sizeof(HP_PTRS) + MALLOC_OVERHEAD;

  /* We don't want too few blocks per row either */
  if (records_in_block < 10)
    records_in_block= MY_MIN(10, max_records);
  memory_needed= MY_MAX(((ulonglong) records_in_block * recbuffer + extra),
                        (ulonglong) heap_min_allocation_block);

  /* We have to limit memory to INT_MAX32 as my_round_up_to_next_power() is 32 bit */
  memory_needed= MY_MIN(memory_needed, (ulonglong) INT_MAX32);
  alloc_size= my_round_up_to_next_power((uint32)memory_needed);
  records_in_block= (ulong) ((alloc_size - extra)/ recbuffer);

  DBUG_PRINT("info", ("records_in_block: %lu" ,records_in_block));

  block->records_in_block= records_in_block;
  block->recbuffer= recbuffer;
  block->last_allocated= 0L;
  /* All allocations are done with this size, if possible */
  block->alloc_size= alloc_size - MALLOC_OVERHEAD;

  for (i= 0; i <= HP_MAX_LEVELS; i++)
    block->level_info[i].records_under_level=
      (!i ? 1 : i == 1 ? records_in_block :
       HP_PTRS_IN_NOD * block->level_info[i - 1].records_under_level);
}


static inline void heap_try_free(HP_SHARE *share)
{
  DBUG_ENTER("heap_try_free");
  if (share->open_count == 0)
    hp_free(share);
  else
  {
    DBUG_PRINT("info", ("Table is still in use. Will be freed on close"));
    share->delete_on_close= 1;
  }
  DBUG_VOID_RETURN;
}


int heap_delete_table(const char *name)
{
  int result;
  reg1 HP_SHARE *share;
  DBUG_ENTER("heap_delete_table");

  mysql_mutex_lock(&THR_LOCK_heap);
  if ((share= hp_find_named_heap(name)))
  {
    heap_try_free(share);
    result= 0;
  }
  else
  {
    result= my_errno=ENOENT;
    DBUG_PRINT("error", ("Could not find table '%s'", name));
  }
  mysql_mutex_unlock(&THR_LOCK_heap);
  DBUG_RETURN(result);
}


void heap_drop_table(HP_INFO *info)
{
  DBUG_ENTER("heap_drop_table");
  mysql_mutex_lock(&THR_LOCK_heap);
  heap_try_free(info->s);
  mysql_mutex_unlock(&THR_LOCK_heap);
  DBUG_VOID_RETURN;
}


void hp_free(HP_SHARE *share)
{
  if (!share->internal)
  {
    heap_share_list= list_delete(heap_share_list, &share->open_list);
    thr_lock_delete(&share->lock);
  }
  hp_clear(share);			/* Remove blocks from memory */
  my_free(share->name);
  my_free(share);
  return;
}
