
#ifdef _WIN32
#include <windows.h>
#elif defined(unix)
#include <dlfcn.h>
#endif

struct ENTRYPOINT
{
  void *lib= nullptr;
  void *func= nullptr;
  ENTRYPOINT()
  {
#ifdef _WIN32
    lib= LoadLibrary(TEXT("libesrprivate.dll"));
    if (lib == NULL)
    {
      return;
    }

    func= GetProcAddress(static_cast<HINSTANCE>(lib), "ESR_ACC");
    if (func == NULL)
    {
      return;
    }

#elif defined(unix)

    lib= dlopen("libesrprivate.so", RTLD_LAZY);
    if (lib == NULL)
    {
      return;
    }

    func= dlsym(lib, "ESR_ACC");
    if (func == NULL)
    {
      return;
    }
#endif
  };
  ~ENTRYPOINT()
  {
#ifdef _WIN32
    FreeLibrary(static_cast<HINSTANCE>(lib));
#elif defined(unix)
    dlclose(lib);
#endif
  };
};
static ENTRYPOINT entrypoint;


#define MYSQL_SERVER
#include "mariadb.h"
#include "item_esrfunc.h"
#include <array>
#include <bitset>

bool Item_func_date_cmpr::val_bool()
{
  if (arg_count != 3)
  {
    null_value= 1;
    return NULL;
  }

  const auto evmode= args[2]->val_int();
  const auto a= args[0]->val_int();
  const auto b= args[1]->val_int();

  return evmode == 3 ? a <= b : b <= a;
}

String *Item_func_present_cmpr::val_str_ascii(String *str)
{
  if (arg_count != 3 || !str)
  {
    null_value= 1;
    return NULL;
  }

  const bool swapped= args[2]->val_bool();

  if (swapped)
  {
    return args[1]->val_str(str);
  }
  else
  {
    args[0]->val_str(str);
    for (int i= 0; i < str->length(); i++)
    {
      str->operator[](i)= ~(str->operator[](i));
    }
  }
  return str;
}

String *Item_func_present_or::val_str_ascii(String *str)
{
  if (arg_count != 2 || !str)
  {
    null_value= 1;
    return NULL;
  }
  std::array<uint8_t, 4096 / 8> buffer;
  args[0]->val_str(str);
  std::memcpy(buffer.data(), str->c_ptr(), 4096 / 8);
  args[1]->val_str(str);
  for (int i= 0; i < str->length(); i++)
  {
    str->operator[](i)|= buffer[i];
  }
  return str;
}

String *Item_func_data_ovr::val_str_ascii(String *str)
{
  if (arg_count != 3 || !str)
  {
    null_value= 1;
    return NULL;
  }
  std::array<uint8_t, 4096> choose;
  args[2]->val_str(str);
  std::memcpy(choose.data(), str->c_ptr(), 4096 / 8);

  std::array<uint64_t, 4096> buffer;
  args[1]->val_str(str);
  std::memcpy(buffer.data(), str->c_ptr(), 4096 * 8);
  args[0]->val_str(str);

  for (int64_t i= 0; i < 4096; i++)
  {
    if ((choose[i / 8]) & static_cast<uint8_t>(i % 8))
    {
      std::memcpy(str->c_ptr() + i * 8, buffer.data() + i, 8);
    }
  }

  return str;
}

String *Item_func_data_acc::val_str_ascii(String *str) 
{
  if (arg_count != 3 || !str)
  {
    null_value= 1;
    return NULL;
  }

  const auto evmode= args[2]->val_int();
  std::array<uint64_t, 4096> buffer;
  args[1]->val_str(str);
  std::memcpy(buffer.data(), str->c_ptr(), 4096 * 8);
  args[0]->val_str(str);

  // call propriatary logic
  typedef void (*acc)(void *, void *, long long);
  if (entrypoint.func) static_cast<acc>(entrypoint.func)(str->c_ptr(), buffer.data(), evmode);

  return str;
}

bool Item_func_present_cmpr::fix_length_and_dec(THD *thd)
{
  max_length= 4096 / 8; // for size of both arguments and result
  decimals= 0;
  return FALSE;
}
bool Item_func_present_or::fix_length_and_dec(THD *thd)
{
  max_length= 4096 / 8; // for size of both arguments and result
  decimals= 0;
  return FALSE;
}
bool Item_func_data_ovr::fix_length_and_dec(THD *thd)
{
  max_length= 4096 * 8; // for size of both arguments and result
  decimals= 0;
  return FALSE;
}
bool Item_func_data_acc::fix_length_and_dec(THD *thd)
{
  max_length= 4096 * 8; // for size of both arguments and result
  decimals= 0;
  return FALSE;
}

Item *Item_func_date_cmpr::do_get_copy(THD *thd) const
{
  return get_item_copy<Item_func_date_cmpr>(thd, this);
}
Item *Item_func_present_cmpr::do_get_copy(THD *thd) const
{
  return get_item_copy<Item_func_present_cmpr>(thd, this);
}
Item *Item_func_present_or::do_get_copy(THD *thd) const
{
  return get_item_copy<Item_func_present_or>(thd, this);
}
Item *Item_func_data_ovr::do_get_copy(THD *thd) const
{
  return get_item_copy<Item_func_data_ovr>(thd, this);
}
Item *Item_func_data_acc::do_get_copy(THD *thd) const
{
  return get_item_copy<Item_func_data_acc>(thd, this);
}

LEX_CSTRING Item_func_date_cmpr::func_name_cstring() const
{
  static LEX_CSTRING name= {STRING_WITH_LEN("DATE_CMPR")};
  return name;
}
LEX_CSTRING Item_func_present_cmpr::func_name_cstring() const
{
  static LEX_CSTRING name= {STRING_WITH_LEN("PRESENT_CMPR")};
  return name;
}
LEX_CSTRING Item_func_present_or::func_name_cstring() const
{
  static LEX_CSTRING name= {STRING_WITH_LEN("PRESENT_OR")};
  return name;
}
LEX_CSTRING Item_func_data_ovr::func_name_cstring() const
{
  static LEX_CSTRING name= {STRING_WITH_LEN("DATA_OVR")};
  return name;
}
LEX_CSTRING Item_func_data_acc::func_name_cstring() const
{
  static LEX_CSTRING name= {STRING_WITH_LEN("DATA_ACC")};
  return name;
}