#pragma once

#include "item.h"


class Item_func_date_cmpr : public Item_bool_func
{
public:
  inline Item_func_date_cmpr(THD *thd, Item *a, Item *b, Item *mode)
      : Item_bool_func(thd, a, b, mode)
  {
  }
  bool val_bool() override;
  Item *do_get_copy(THD *thd) const override;
  LEX_CSTRING func_name_cstring() const override;
};

class Item_func_present_cmpr : public Item_str_ascii_func
{
public:
  inline Item_func_present_cmpr(THD *thd, Item *a, Item *b, Item *mode)
      : Item_str_ascii_func(thd, a, b, mode)
  {
  }
  String *val_str_ascii(String *) override;
  bool fix_length_and_dec(THD *thd) override;
  Item *do_get_copy(THD *thd) const override;
  LEX_CSTRING func_name_cstring() const override;
};

class Item_func_present_or : public Item_str_ascii_func
{
public:
  inline Item_func_present_or(THD *thd, Item *a, Item *b)
      : Item_str_ascii_func(thd, a, b)
  {
  }
  String *val_str_ascii(String *) override;
  bool fix_length_and_dec(THD *thd) override;
  Item *do_get_copy(THD *thd) const override;
  LEX_CSTRING func_name_cstring() const override;
};

class Item_func_data_ovr : public Item_str_ascii_func
{
public:
  inline Item_func_data_ovr(THD *thd, Item *a, Item *b, Item *mode)
      : Item_str_ascii_func(thd, a, b, mode)
  {
  }
  String *val_str_ascii(String *) override;
  bool fix_length_and_dec(THD *thd) override;
  Item *do_get_copy(THD *thd) const override;
  LEX_CSTRING func_name_cstring() const override;
};

class Item_func_data_acc : public Item_str_ascii_func
{
public:
  inline Item_func_data_acc(THD *thd, Item *a, Item *b, Item *mode)
      : Item_str_ascii_func(thd, a, b, mode)
  {
  }
  String *val_str_ascii(String *) override;
  bool fix_length_and_dec(THD *thd) override;
  Item *do_get_copy(THD *thd) const override;
  LEX_CSTRING func_name_cstring() const override;
};