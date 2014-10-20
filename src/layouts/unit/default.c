/** TODO: copyright notice */

#include "slurm/slurm.h"

#include "src/common/layouts_mgr.h"
#include "src/common/entity.h"
#include "src/common/log.h"

const char plugin_name[] = "Unit Tests layouts plugin";
const char plugin_type[] = "layouts/unit";
const uint32_t plugin_version = 100;

/* specific options for unit tests layout */
s_p_options_t entity_options[] = {
	{"string", S_P_STRING},
	{"long", S_P_LONG},
	{"uint16", S_P_UINT16},
	{"uint32", S_P_UINT32},
	{"float", S_P_FLOAT},
	{"double", S_P_DOUBLE},
	{"ldouble", S_P_LONG_DOUBLE},
	{NULL}
};
s_p_options_t options[] = {
	{"Entity", S_P_EXPLINE, NULL, NULL, entity_options},
	{NULL}
};

const layouts_keyspec_t keyspec[] = {
	{"string", L_T_STRING},
	{"long", L_T_LONG},
	{"uint16", L_T_UINT16},
	{"uint32", L_T_UINT32},
	{"float", L_T_FLOAT},
	{"double", L_T_DOUBLE},
	{"ldouble", L_T_LONG_DOUBLE},
	{NULL}
};

/* types allowed in the entity's "type" field */
const char* etypes[] = {
	"UnitTestPass",
	"UnitTest",
	NULL
};

const layouts_plugin_spec_t plugin_spec = {
	options,
	keyspec,
	LAYOUT_STRUCT_TREE,
	etypes,
	true /* if this evalued to true, keys inside plugin_keyspec present in
	      * plugin_options having corresponding types, are automatically
	      * handled by the layouts manager.
	      */
};

/* manager is lock when this function is called */
/* disable this callback by setting it to NULL, warn: not every callback can
 * be desactivated this way */
int layouts_p_conf_done(
		xhash_t* entities, layout_t* layout, s_p_hashtbl_t* tbl)
{
	return 1;
}


/* disable this callback by setting it to NULL, warn: not every callback can
 * be desactivated this way */
void layouts_p_entity_parsing(
		entity_t* e, s_p_hashtbl_t* etbl, layout_t* layout)
{
}

/* manager is lock then this function is called */
/* disable this callback by setting it to NULL, warn: not every callback can
 * be desactivated this way */
int layouts_p_update_done(layout_t* layout, entity_t** e_array, int e_cnt)
{
	int i;
	debug3("layouts/unit: receiving update callback for %d entities",
	       e_cnt);
	for (i=0; i < e_cnt; i++) {
		if (e_array[i] == NULL) {
			debug3("layouts/unit: skipping update of nullified"
			       "entity[%d]", i);
		} else {
			debug3("layouts/unit: updating entity[%d]=%s",
			       i, e_array[i]->name);
		}
	}
	return 1;
}

int init(void)
{
	return SLURM_SUCCESS;
}

int fini(void)
{
	return SLURM_SUCCESS;
}
