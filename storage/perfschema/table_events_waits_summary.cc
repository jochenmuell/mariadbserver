/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA */

/**
  @file storage/perfschema/table_events_waits_summary.cc
  Table EVENTS_WAITS_SUMMARY_BY_xxx (implementation).
*/

#include "my_global.h"
#include "my_thread.h"
#include "pfs_instr_class.h"
#include "pfs_column_types.h"
#include "pfs_column_values.h"
#include "table_events_waits_summary.h"
#include "pfs_global.h"
#include "field.h"

THR_LOCK table_events_waits_summary_by_instance::m_table_lock;

PFS_engine_table_share_state
table_events_waits_summary_by_instance::m_share_state = {
  false /* m_checked */
};

PFS_engine_table_share
table_events_waits_summary_by_instance::m_share=
{
  { C_STRING_WITH_LEN("events_waits_summary_by_instance") },
  &pfs_truncatable_acl,
  table_events_waits_summary_by_instance::create,
  NULL, /* write_row */
  table_events_waits_summary_by_instance::delete_all_rows,
  table_all_instr::get_row_count,
  sizeof(pos_all_instr),
  &m_table_lock,
  { C_STRING_WITH_LEN("CREATE TABLE events_waits_summary_by_instance("
                      "EVENT_NAME VARCHAR(128) not null comment 'Event name. Used together with OBJECT_INSTANCE_BEGIN for grouping events.',"
                      "OBJECT_INSTANCE_BEGIN BIGINT unsigned not null comment 'If an instrument creates multiple instances, each instance has a unique OBJECT_INSTANCE_BEGIN value to allow for grouping by instance.',"
                      "COUNT_STAR BIGINT unsigned not null comment 'Number of summarized events',"
                      "SUM_TIMER_WAIT BIGINT unsigned not null comment 'Total wait time of the summarized events that are timed.',"
                      "MIN_TIMER_WAIT BIGINT unsigned not null comment 'Minimum wait time of the summarized events that are timed.',"
                      "AVG_TIMER_WAIT BIGINT unsigned not null comment 'Average wait time of the summarized events that are timed.',"
                      "MAX_TIMER_WAIT BIGINT unsigned not null comment 'Maximum wait time of the summarized events that are timed.')") },
  false, /* m_perpetual */
  false, /* m_optional */
  &m_share_state
};

PFS_engine_table* table_events_waits_summary_by_instance::create(void)
{
  return new table_events_waits_summary_by_instance();
}

int table_events_waits_summary_by_instance::delete_all_rows(void)
{
  reset_events_waits_by_instance();
  return 0;
}

table_events_waits_summary_by_instance
::table_events_waits_summary_by_instance()
  : table_all_instr(&m_share), m_row_exists(false)
{
  m_normalizer = time_normalizer::get_wait();
}

void table_events_waits_summary_by_instance
::make_instr_row(PFS_instr *pfs, PFS_instr_class *klass,
                 const void *object_instance_begin,
                 PFS_single_stat *pfs_stat)
{
  pfs_optimistic_state lock;
  m_row_exists= false;

  /*
    Protect this reader against a mutex/rwlock/cond destroy,
    file delete, table drop.
  */
  pfs->m_lock.begin_optimistic_lock(&lock);

  m_row.m_name= klass->m_name;
  m_row.m_name_length= klass->m_name_length;
  m_row.m_object_instance_addr= (intptr) object_instance_begin;

  m_row.m_stat.set(m_normalizer, pfs_stat);

  if (pfs->m_lock.end_optimistic_lock(&lock))
    m_row_exists= true;
}

/**
  Build a row, for mutex statistics in a thread.
  @param pfs              the mutex this cursor is reading
*/
void table_events_waits_summary_by_instance::make_mutex_row(PFS_mutex *pfs)
{
  PFS_mutex_class *safe_class;
  safe_class= sanitize_mutex_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
    return;

  make_instr_row(pfs, safe_class, pfs->m_identity, &pfs->m_mutex_stat.m_wait_stat);
}

/**
  Build a row, for rwlock statistics in a thread.
  @param pfs              the rwlock this cursor is reading
*/
void table_events_waits_summary_by_instance::make_rwlock_row(PFS_rwlock *pfs)
{
  PFS_rwlock_class *safe_class;
  safe_class= sanitize_rwlock_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
    return;

  make_instr_row(pfs, safe_class, pfs->m_identity, &pfs->m_rwlock_stat.m_wait_stat);
}

/**
  Build a row, for condition statistics in a thread.
  @param pfs              the condition this cursor is reading
*/
void table_events_waits_summary_by_instance::make_cond_row(PFS_cond *pfs)
{
  PFS_cond_class *safe_class;
  safe_class= sanitize_cond_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
    return;

  make_instr_row(pfs, safe_class, pfs->m_identity, &pfs->m_cond_stat.m_wait_stat);
}

/**
  Build a row, for file statistics in a thread.
  @param pfs              the file this cursor is reading
*/
void table_events_waits_summary_by_instance::make_file_row(PFS_file *pfs)
{
  PFS_file_class *safe_class;
  safe_class= sanitize_file_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
    return;

  PFS_single_stat sum;
  pfs->m_file_stat.m_io_stat.sum_waits(& sum);
  /*
    Files don't have a in memory structure associated to it,
    so we use the address of the PFS_file buffer as object_instance_begin
  */
  make_instr_row(pfs, safe_class, pfs, & sum);
}

/**
  Build a row, for socket statistics in a thread.
  @param pfs              the socket this cursor is reading
*/
void table_events_waits_summary_by_instance::make_socket_row(PFS_socket *pfs)
{
  PFS_socket_class *safe_class;
  safe_class= sanitize_socket_class(pfs->m_class);
  if (unlikely(safe_class == NULL))
    return;

  /*
     Consolidate wait times and byte counts for individual operations. This is
     done by the consumer in order to reduce overhead on the socket instrument.
  */
  PFS_byte_stat pfs_stat;
  pfs->m_socket_stat.m_io_stat.sum(&pfs_stat);

  /*
    Sockets don't have an associated in-memory structure, so use the address of
    the PFS_socket buffer as object_instance_begin.
  */
  make_instr_row(pfs, safe_class, pfs, &pfs_stat);
}

int table_events_waits_summary_by_instance
::read_row_values(TABLE *table, unsigned char *, Field **fields,
                  bool read_all)
{
  Field *f;

  if (unlikely(! m_row_exists))
    return HA_ERR_RECORD_DELETED;

  /* Set the null bits */
  assert(table->s->null_bytes == 0);

  for (; (f= *fields) ; fields++)
  {
    if (read_all || bitmap_is_set(table->read_set, f->field_index))
    {
      switch(f->field_index)
      {
      case 0: /* NAME */
        set_field_varchar_utf8(f, m_row.m_name, m_row.m_name_length);
        break;
      case 1: /* OBJECT_INSTANCE */
        set_field_ulonglong(f, m_row.m_object_instance_addr);
        break;
      case 2: /* COUNT */
        set_field_ulonglong(f, m_row.m_stat.m_count);
        break;
      case 3: /* SUM */
        set_field_ulonglong(f, m_row.m_stat.m_sum);
        break;
      case 4: /* MIN */
        set_field_ulonglong(f, m_row.m_stat.m_min);
        break;
      case 5: /* AVG */
        set_field_ulonglong(f, m_row.m_stat.m_avg);
        break;
      case 6: /* MAX */
        set_field_ulonglong(f, m_row.m_stat.m_max);
        break;
      default:
        assert(false);
      }
    }
  }

  return 0;
}

