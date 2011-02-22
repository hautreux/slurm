/*****************************************************************************\
 *  ba_common.c
 *
 *****************************************************************************
 *  Copyright (C) 2011 Lawrence Livermore National Security.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Danny Auble <da@llnl.gov>
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <https://computing.llnl.gov/linux/slurm/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#include "ba_common.h"

#if (SYSTEM_DIMENSIONS == 1)
int cluster_dims = 1;
int cluster_base = 10;
#else
int cluster_dims = 3;
int cluster_base = 36;
#endif
uint32_t cluster_flags = 0;
uint16_t ba_deny_pass = 0;

bool ba_initialized = false;

static int _coord(char coord)
{
	if ((coord >= '0') && (coord <= '9'))
		return (coord - '0');
	if ((coord >= 'A') && (coord <= 'Z'))
		return (coord - 'A') + 10;
	return -1;
}

/**
 * Initialize internal structures by either reading previous block
 * configurations from a file or by running the graph solver.
 *
 * IN: node_info_msg_t * can be null,
 *     should be from slurm_load_node().
 *
 * return: void.
 */
extern void ba_init(node_info_msg_t *node_info_ptr, bool sanity_check)
{
	node_info_t *node_ptr = NULL;
	int number, count;
	char *numeric = NULL;
	int i, j, k;
	slurm_conf_node_t **ptr_array;
	int coords[HIGHEST_DIMENSIONS];
	char *p = '\0';
	int num_cpus = 0;
	int real_dims[HIGHEST_DIMENSIONS];
	int dims[HIGHEST_DIMENSIONS];
	char dim_str[HIGHEST_DIMENSIONS+1];

	/* We only need to initialize once, so return if already done so. */
	if (ba_initialized)
		return;

	cluster_dims = slurmdb_setup_cluster_dims();
	cluster_flags = slurmdb_setup_cluster_flags();
	set_ba_debug_flags(slurm_get_debug_flags());

	bridge_init("");

	memset(coords, 0, sizeof(coords));
	memset(dims, 0, sizeof(dims));
	memset(real_dims, 0, sizeof(real_dims));
	memset(dim_str, 0, sizeof(dim_str));
	/* cluster_dims is already set up off of working_cluster_rec */
	if (cluster_dims == 1) {
		if (node_info_ptr) {
			real_dims[0] = dims[0] = node_info_ptr->record_count;
			for (i=1; i<cluster_dims; i++)
				real_dims[i] = dims[i] = 1;
			num_cpus = node_info_ptr->record_count;
		}
		goto setup_done;
	} else if (working_cluster_rec && working_cluster_rec->dim_size) {
		for(i=0; i<cluster_dims; i++) {
			real_dims[i] = dims[i] =
				working_cluster_rec->dim_size[i];
		}
		goto setup_done;
	}


	if (node_info_ptr) {
		for (i = 0; i < (int)node_info_ptr->record_count; i++) {
			node_ptr = &node_info_ptr->node_array[i];
			number = 0;

			if (!node_ptr->name) {
				memset(dims, 0, sizeof(dims));
				goto node_info_error;
			}

			numeric = node_ptr->name;
			while (numeric) {
				if (numeric[0] < '0' || numeric[0] > 'D'
				    || (numeric[0] > '9'
					&& numeric[0] < 'A')) {
					numeric++;
					continue;
				}
				number = xstrntol(numeric, &p, cluster_dims,
						  cluster_base);
				break;
			}
			hostlist_parse_int_to_array(
				number, coords, cluster_dims, cluster_base);

			memcpy(dims, coords, sizeof(dims));
		}
		for (j=0; j<cluster_dims; j++) {
			dims[j]++;
			/* this will probably be reset below */
			real_dims[j] = dims[j];
		}
		num_cpus = node_info_ptr->record_count;
	}
node_info_error:
	for (j=0; j<cluster_dims; j++)
		if (!dims[j])
			break;

	if (j < cluster_dims) {
		debug("Setting dimensions from slurm.conf file");
		count = slurm_conf_nodename_array(&ptr_array);
		if (count == 0)
			fatal("No NodeName information available!");

		for (i = 0; i < count; i++) {
			char *nodes = ptr_array[i]->nodenames;
			j = 0;
			while (nodes[j] != '\0') {
				int mid = j   + cluster_dims + 1;
				int fin = mid + cluster_dims + 1;

				if (((nodes[j] == '[') || (nodes[j] == ','))
				    && ((nodes[mid] == 'x')
					|| (nodes[mid] == '-'))
				    && ((nodes[fin] == ']')
					|| (nodes[fin] == ',')))
					j = mid + 1; /* goto the mid
						      * and skip it */
				else if ((nodes[j] >= '0' && nodes[j] <= '9')
					 || (nodes[j] >= 'A'
					     && nodes[j] <= 'Z')) {
					/* suppose to be blank, just
					   making sure this is the
					   correct alpha num
					*/
				} else {
					j++;
					continue;
				}

				for (k = 0; k < cluster_dims; k++, j++)
					dims[k] = MAX(dims[k],
						      _coord(nodes[j]));
				if (nodes[j] != ',')
					break;
			}

			/* j = 0; */
			/* while (node->nodenames[j] != '\0') { */
			/* 	if ((node->nodenames[j] == '[' */
			/* 	     || node->nodenames[j] == ',') */
			/* 	    && (node->nodenames[j+10] == ']' */
			/* 		|| node->nodenames[j+10] == ',') */
			/* 	    && (node->nodenames[j+5] == 'x' */
			/* 		|| node->nodenames[j+5] == '-')) { */
			/* 		j+=6; */
			/* 	} else if ((node->nodenames[j] >= '0' */
			/* 		    && node->nodenames[j] <= '9') */
			/* 		   || (node->nodenames[j] >= 'A' */
			/* 		       && node->nodenames[j] <= 'Z')) { */
			/* 		/\* suppose to be blank, just */
			/* 		   making sure this is the */
			/* 		   correct alpha num */
			/* 		*\/ */
			/* 	} else { */
			/* 		j++; */
			/* 		continue; */
			/* 	} */
			/* 	number = xstrntol(node->nodenames + j, */
			/* 			  &p, cluster_dims, */
			/* 			  cluster_base); */
			/* 	hostlist_parse_int_to_array( */
			/* 		number, coords, cluster_dims, */
			/* 		cluster_base); */
			/* 	j += 4; */

			/* 	for(k=0; k<cluster_dims; k++) */
			/* 		dims[k] = MAX(dims[k], */
			/* 				  coords[k]); */

			/* 	if (node->nodenames[j] != ',') */
			/* 		break; */
			/* } */
		}

		for (j=0; j<cluster_dims; j++)
			if (dims[j])
				break;

		if (j >= cluster_dims)
			info("are you sure you only have 1 midplane? %s",
			     ptr_array[i]->nodenames);

		for (j=0; j<cluster_dims; j++) {
			dims[j]++;
			/* this will probably be reset below */
			real_dims[j] = dims[j];
		}
	}

	/* sanity check.  We can only request part of the system, but
	   we don't want to allow more than we have. */
	if (sanity_check) {
		verbose("Attempting to contact MMCS");
		if (bridge_get_size(real_dims) == SLURM_SUCCESS) {
			char real_dim_str[cluster_dims+1];
			memset(real_dim_str, 0, sizeof(real_dim_str));
			for (i=0; i<cluster_dims; i++) {
				dim_str[i] = alpha_num[dims[i]];
				real_dim_str[i] = alpha_num[real_dims[i]];
			}
			verbose("BlueGene configured with %s midplanes",
				real_dim_str);
			for (i=0; i<cluster_dims; i++)
				if (dims[i] > real_dims[i])
					fatal("You requested a %s system, "
					      "but we only have a "
					      "system of %s.  "
					      "Change your slurm.conf.",
					      dim_str, real_dim_str);
		}
	}

setup_done:
	if (cluster_dims == 1) {
		if (!dims[0]) {
			debug("Setting default system dimensions");
			real_dims[0] = dims[0] = 100;
			for (i=1; i<cluster_dims; i++)
				real_dims[i] = dims[i] = 1;
		}
	} else {
		for (i=0; i<cluster_dims; i++)
			dim_str[i] = alpha_num[dims[i]];
		debug("We are using %s of the system.", dim_str);
	}

	if (!num_cpus) {
		num_cpus = 1;
		for(i=0; i<cluster_dims; i++)
			num_cpus *= dims[i];
	}

	ba_create_system(num_cpus, real_dims, dims);

	bridge_setup_system();

	ba_initialized = true;
	init_grid(node_info_ptr);
}


/**
 * destroy all the internal (global) data structs.
 */
extern void ba_fini()
{
	if (!ba_initialized){
		return;
	}

	bridge_fini();
	ba_destroy_system();
	ba_initialized = false;

//	debug3("pa system destroyed");
}

extern void destroy_ba_mp(void *ptr)
{
	ba_mp_t *ba_mp = (ba_mp_t *)ptr;
	if (ba_mp) {
		xfree(ba_mp->loc);
		xfree(ba_mp);
	}
}

extern ba_mp_t *str2ba_mp(char *coords)
{
	int coord[cluster_dims];
	int len, dim;
	int number;
	char *p = '\0';
	static int *dims = NULL;

	if (!dims)
		dims = select_g_ba_get_dims();

	if (!coords)
		return NULL;
	len = strlen(coords) - cluster_dims;
	if (len < 0)
		return NULL;
	number = xstrntol(coords + len, &p, cluster_dims, cluster_base);

	hostlist_parse_int_to_array(number, coord, cluster_dims, cluster_base);

	for (dim=0; dim<cluster_dims; dim++)
		if (coord[dim] > dims[dim])
			break;
	if (dim < cluster_dims) {
		char tmp_char[cluster_dims+1];
		memset(tmp_char, 0, sizeof(tmp_char));
		for (dim=0; dim<cluster_dims; dim++)
			tmp_char[dim] = alpha_num[dims[dim]];
		error("This location %s is not possible in our system %s",
		      coords, tmp_char);
		return NULL;
	}

	return coord2ba_mp(coord);
}

extern void ba_setup_mp(ba_mp_t *ba_mp, bool track_down_mps)
{
	int i;
	uint16_t node_base_state = ba_mp->state & NODE_STATE_BASE;

	if (!track_down_mps ||((node_base_state != NODE_STATE_DOWN)
			       && !(ba_mp->state & NODE_STATE_DRAIN)))
		ba_mp->used = BA_MP_USED_FALSE;

	for (i=0; i<cluster_dims; i++){
#ifdef HAVE_BG_L_P
		int j;
		for(j=0;j<NUM_PORTS_PER_NODE;j++) {
			ba_mp->axis_switch[i].int_wire[j].used = 0;
			if (i!=0) {
				if (j==3 || j==4)
					ba_mp->axis_switch[i].int_wire[j].
						used = 1;
			}
			ba_mp->axis_switch[i].int_wire[j].port_tar = j;
		}
#endif
		ba_mp->axis_switch[i].usage = BG_SWITCH_NONE;
	}
}

/*
 * copy info from a ba_mp, a direct memcpy of the ba_mp_t
 *
 * IN ba_mp: mp to be copied
 * Returned ba_mp_t *: copied info must be freed with destroy_ba_mp
 */
extern ba_mp_t *ba_copy_mp(ba_mp_t *ba_mp)
{
	ba_mp_t *new_ba_mp = (ba_mp_t *)xmalloc(sizeof(ba_mp_t));

	memcpy(new_ba_mp, ba_mp, sizeof(ba_mp_t));
	new_ba_mp->loc = xstrdup(ba_mp->loc);
	/* we have to set this or we would be pointing to the original */
	memset(new_ba_mp->next_mp, 0, sizeof(new_ba_mp->next_mp));

	return new_ba_mp;
}
