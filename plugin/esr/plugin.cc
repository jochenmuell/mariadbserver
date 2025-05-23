#define MYSQL_SERVER
#include "mariadb.h"
#include "sql_class.h"
#include "item_esrfunc.h"
#include <mysql/plugin_data_type.h>
#include <mysql/plugin_function.h>

/*************************************************************************/

// bool date_cmpr(tile_a.created, tile_b.created, box_evalmode); 0 = a is primary, 1 = b is primary
class Create_func_date_cmpr : public Create_func_arg3
{
public:
  virtual Item *create_3_arg(THD *thd, Item *arg1, Item *arg2,
                             Item *arg3) override
  {
    return new (thd->mem_root) Item_func_date_cmpr(thd, arg1, arg2, arg3);
  }
  static Create_func_date_cmpr s_singleton;

protected:
  Create_func_date_cmpr() {}
  ~Create_func_date_cmpr() override {}
};

// bytes present_cmpr(tile_a.present, tile_b.present, date_cmpr(...)); 0 = a is choosen, 1 = b is choosen
class Create_func_present_cmpr : public Create_func_arg3
{
public:
  virtual Item *create_3_arg(THD *thd, Item *arg1, Item *arg2,
                             Item *arg3) override
  {
    return new (thd->mem_root) Item_func_present_cmpr(thd, arg1, arg2, arg3);
  }
  static Create_func_present_cmpr s_singleton;

protected:
  Create_func_present_cmpr() {}
  ~Create_func_present_cmpr() override {}
};

// bytes present_or(tile_a.present, tile_b.present)
class Create_func_present_or : public Create_func_arg2
{
public:
  virtual Item *create_2_arg(THD *thd, Item *arg1, Item *arg2) override
  {
    return new (thd->mem_root) Item_func_present_or(thd, arg1, arg2);
  }
  static Create_func_present_or s_singleton;

protected:
  Create_func_present_or() {}
  ~Create_func_present_or() override {}
};

// bytes data_ovr(tile_a.data, tile_b.data, present_cmpr())
class Create_func_data_ovr : public Create_func_arg3
{
public:
  virtual Item *create_3_arg(THD *thd, Item *arg1, Item *arg2,
                             Item *arg3) override
  {
      return new (thd->mem_root) Item_func_data_ovr(thd, arg1, arg2, arg3);
  }
  static Create_func_data_ovr s_singleton;

protected:
  Create_func_data_ovr() {}
  ~Create_func_data_ovr() override {}
};
// bytes data_acc(tile_a.data, tile_b.data, evalmode)
class Create_func_data_acc : public Create_func_arg3
{
public:
  virtual Item *create_3_arg(THD *thd, Item *arg1, Item *arg2,
                             Item *arg3) override
  {
    return new (thd->mem_root) Item_func_data_acc(thd, arg1, arg2, arg3);
  }
  static Create_func_data_acc s_singleton;

protected:
  Create_func_data_acc() {}
  ~Create_func_data_acc() override {}
};


Create_func_date_cmpr Create_func_date_cmpr::s_singleton;
Create_func_present_cmpr Create_func_present_cmpr::s_singleton;
Create_func_present_or Create_func_present_or::s_singleton;
Create_func_data_ovr Create_func_data_ovr::s_singleton;
Create_func_data_acc Create_func_data_acc::s_singleton;


#define BUILDER(F) & F::s_singleton


static Plugin_function
    plugin_descriptor_function_date_cmpr(BUILDER(Create_func_date_cmpr)),
    plugin_descriptor_function_present_cmpr(BUILDER(Create_func_present_cmpr)), 
    plugin_descriptor_function_present_or(BUILDER(Create_func_present_or)), 
    plugin_descriptor_function_data_ovr(BUILDER(Create_func_data_ovr)), 
    plugin_descriptor_function_data_acc(BUILDER(Create_func_data_acc));


/*************************************************************************/

extern "C"
{
  maria_declare_plugin(esr){
      MariaDB_FUNCTION_PLUGIN, // the plugin type (see include/mysql/plugin.h)
      &plugin_descriptor_function_date_cmpr, // pointer to type-specific plugin
                                             // descriptor
      "esr_date_cmpr",                       // plugin name
      "Rohmann GmbH",                        // plugin author
      "Function DATE_CMPR()",                // the plugin description
      PLUGIN_LICENSE_GPL, // the plugin license (see include/mysql/plugin.h)
      0,                  // Pointer to plugin initialization function
      0,                  // Pointer to plugin deinitialization function
      0x0100,             // Numeric version 0xAABB means AA.BB version
      NULL,               // Status variables
      NULL,               // System variables
      "1.0",              // String version representation
      MariaDB_PLUGIN_MATURITY_STABLE // Maturity(see include/mysql/plugin.h)*/
  },
      {
          MariaDB_FUNCTION_PLUGIN,                  // the plugin type (see
                                                    // include/mysql/plugin.h)
          &plugin_descriptor_function_present_cmpr, // pointer to type-specific
                                                    // plugin
                                                    // descriptor
          "esr_present_cmpr",                       // plugin name
          "Rohmann GmbH",                           // plugin author
          "Function PRESENT_CMPR()",                // the plugin description
          PLUGIN_LICENSE_GPL,                       // the plugin license (see
                                                    // include/mysql/plugin.h)
          0,      // Pointer to plugin initialization function
          0,      // Pointer to plugin deinitialization function
          0x0100, // Numeric version 0xAABB means AA.BB version
          NULL,   // Status variables
          NULL,   // System variables
          "1.0",  // String version representation
          MariaDB_PLUGIN_MATURITY_STABLE // Maturity(see
                                         // include/mysql/plugin.h)*/
      },
      {
          MariaDB_FUNCTION_PLUGIN,                // the plugin type (see
                                                  // include/mysql/plugin.h)
          &plugin_descriptor_function_present_or, // pointer to type-specific
                                                  // plugin
                                                  // descriptor
          "esr_present_or",                       // plugin name
          "Rohmann GmbH",                         // plugin author
          "Function PRESENT_OR()",                // the plugin description
          PLUGIN_LICENSE_GPL,                     // the plugin license (see
                                                  // include/mysql/plugin.h)
          0,      // Pointer to plugin initialization function
          0,      // Pointer to plugin deinitialization function
          0x0100, // Numeric version 0xAABB means AA.BB version
          NULL,   // Status variables
          NULL,   // System variables
          "1.0",  // String version representation
          MariaDB_PLUGIN_MATURITY_STABLE // Maturity(see
                                         // include/mysql/plugin.h)*/
      },
      {
          MariaDB_FUNCTION_PLUGIN,              // the plugin type (see
                                                // include/mysql/plugin.h)
          &plugin_descriptor_function_data_ovr, // pointer to type-specific
                                                // plugin
                                                // descriptor
          "esr_data_ovr",                       // plugin name
          "Rohmann GmbH",                       // plugin author
          "Function DATA_OVR()",                // the plugin description
          PLUGIN_LICENSE_GPL,                   // the plugin license (see
                                                // include/mysql/plugin.h)
          0,      // Pointer to plugin initialization function
          0,      // Pointer to plugin deinitialization function
          0x0100, // Numeric version 0xAABB means AA.BB version
          NULL,   // Status variables
          NULL,   // System variables
          "1.0",  // String version representation
          MariaDB_PLUGIN_MATURITY_STABLE // Maturity(see
                                         // include/mysql/plugin.h)*/
      },
      {
          MariaDB_FUNCTION_PLUGIN,              // the plugin type (see
                                                // include/mysql/plugin.h)
          &plugin_descriptor_function_data_acc, // pointer to type-specific
                                                // plugin
                                                // descriptor
          "esr_data_acc",                       // plugin name
          "Rohmann GmbH",                       // plugin author
          "Function DATA_ACC()",                // the plugin description
          PLUGIN_LICENSE_GPL,                   // the plugin license (see
                                                // include/mysql/plugin.h)
          0,      // Pointer to plugin initialization function
          0,      // Pointer to plugin deinitialization function
          0x0100, // Numeric version 0xAABB means AA.BB version
          NULL,   // Status variables
          NULL,   // System variables
          "1.0",  // String version representation
          MariaDB_PLUGIN_MATURITY_STABLE // Maturity(see
                                         // include/mysql/plugin.h)*/
      } maria_declare_plugin_end;
}