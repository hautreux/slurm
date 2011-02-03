/*****************************************************************************\
 *  bridge_linker.cc
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

extern "C" {
#include "bridge_linker.h"
#include "../block_allocator/block_allocator.h"
}

#include "bridge_status.h"

#if defined HAVE_BG_FILES && defined HAVE_BGQ

#include <bgsched/bgsched.h>
#include <bgsched/Block.h>
#include <bgsched/core/core.h>
#include <boost/foreach.hpp>

using namespace std;
using namespace bgsched;
using namespace bgsched::core;

#endif

#define MAX_POLL_RETRIES    220
#define POLL_INTERVAL        3

/* Global variables */

bg_config_t *bg_conf = NULL;
bg_lists_t *bg_lists = NULL;
bool agent_fini = false;
time_t last_bg_update;
pthread_mutex_t block_state_mutex = PTHREAD_MUTEX_INITIALIZER;
int blocks_are_created = 0;
int num_unused_cpus = 0;

/* local vars */
//static pthread_mutex_t api_file_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool initialized = false;

#if defined HAVE_BG_FILES && defined HAVE_BGQ
static void _setup_ba_switch_int(ba_switch_t *ba_switch)
{

}

static void _setup_ba_mp(ComputeHardware::ConstPtr bgq, ba_mp_t *ba_mp)
{
	// int i;
	Midplane::Coordinates coords = {{ba_mp->coord[A], ba_mp->coord[X],
					 ba_mp->coord[Y], ba_mp->coord[Z]}};
	Midplane::ConstPtr mp_ptr = bgq->getMidplane(coords);

	ba_mp->loc = xstrdup(mp_ptr->getLocation().c_str());

	// info("%s which is %c%c%c%c is setup",
	//      ba_mp->loc,
	//      alpha_num[ba_mp->coord[A]],
	//      alpha_num[ba_mp->coord[X]],
	//      alpha_num[ba_mp->coord[Y]],
	//      alpha_num[ba_mp->coord[Z]]);
	// for (i=0; i < SYSTEM_DIMENSIONS; i++) {
	// 	Switch::ConstPtr bg_switch = mp_ptr->getSwitch(
	// 		(bgsched::Dimension::Value)i);
	// 	switch (bg_switch.getInUse().Value()) {
	// 	case Switch::NotInUse:
	// 		ba_mp->axis_switch[i].usage = BG_SWITCH_NONE;
	// 		break;
	// 	case Switch::IncludedBothPortsInUse:
	// 		ba_mp->axis_switch[i].usage = BG_SWITCH_TORUS;
	// 		break;
	// 	case Switch::IncludedOutputPortInUse:
	// 		ba_mp->axis_switch[i].usage =
	// 			(BG_SWITCH_OUT | BG_SWITCH_OUT_PASS);
	// 		break;
	// 	case Switch::IncludedInputPortInUse:
	// 		ba_mp->axis_switch[i].usage =
	// 			(BG_SWITCH_IN | BG_SWITCH_IN_PASS);
	// 		break;
	// 	case Switch::Wrapped:
	// 		ba_mp->axis_switch[i].usage = BG_SWITCH_WRAPPED;
	// 		break;
	// 	case Switch::Passthrough:
	// 		ba_mp->axis_switch[i].usage = BG_SWITCH_PASS;
	// 		break;
	// 	case Switch::WrappedPassthrough:
	// 		ba_mp->axis_switch[i].usage = BG_SWITCH_WRAPPED_PASS;
	// 		break;
	// 	default:
	// 		error("unknown switch conf");
	// 		break;
	// 	}
	// 	info("mp %s(%d) usage is %s", ba_mp->coord_str,
	// 	     i, ba_switch_usage_str(ba_mp->axis_switch[i].usage);

	// }
}
#endif

extern int bridge_init(char *properties_file)
{
	if (initialized)
		return 1;

#if defined HAVE_BG_FILES && defined HAVE_BGQ
	bgsched::init(properties_file);
#endif
	bridge_status_init();
	initialized = true;

	return 1;

}

extern int bridge_fini()
{
	initialized = false;
	bridge_status_fini();

	return SLURM_SUCCESS;
}

/*
 * Convert a BG API error code to a string
 * IN inx - error code from any of the BG Bridge APIs
 * RET - string describing the error condition
 */
extern const char *bridge_err_str(int inx)
{
	// switch (inx) {
	// }

	return "?";
}

extern int bridge_get_size(int *size)
{
#if defined HAVE_BG_FILES && defined HAVE_BGQ
        Midplane::Coordinates bgq_size;
	Dimension dim;
#else
	int dim;
#endif
	if (!bridge_init(NULL))
		return SLURM_ERROR;
	memset(size, 0, sizeof(int) * SYSTEM_DIMENSIONS);

#if defined HAVE_BG_FILES && defined HAVE_BGQ
	bgq_size = core::getMachineSize();
	for (dim=Dimension::A; dim<=Dimension::D; ++dim)
		size[dim] = bgq_size[dim];
#else
	for (dim=0; dim<SYSTEM_DIMENSIONS; dim++)
		size[dim] = DIM_SIZE[dim];
#endif

	return SLURM_SUCCESS;
}

extern int bridge_setup_system()
{
	static bool inited = false;

	if (inited)
		return SLURM_SUCCESS;

	if (!bridge_init(NULL))
		return SLURM_ERROR;

	inited = true;
#if defined HAVE_BG_FILES && defined HAVE_BGQ
	ComputeHardware::ConstPtr bgq = getComputeHardware();

	for (int a = 0; a < DIM_SIZE[A]; a++)
		for (int x = 0; x < DIM_SIZE[X]; x++)
			for (int y = 0; y < DIM_SIZE[Y]; y++)
				for (int z = 0; z < DIM_SIZE[Z]; z++)
					_setup_ba_mp(bgq, &ba_system_ptr->
						     grid[a][x][y][z]);
#endif

	return SLURM_SUCCESS;
}

extern int bridge_block_create(bg_record_t *bg_record)
{
	ListIterator itr = NULL;
	int rc = SLURM_SUCCESS;

#if defined HAVE_BG_FILES && defined HAVE_BGQ
	Block::Ptr block_ptr;
	Block::Midplanes midplanes;
        Block::PassthroughMidplanes pt_midplanes;
        Block::DimensionConnectivity conn_type;
	Midplane::Ptr midplane;
	Dimension dim;
	ba_mp_t *ba_mp = NULL;
#endif

	if (!bridge_init(NULL))
		return SLURM_ERROR;

	if (bg_record->small) {
		info("we can't make small blocks yet");
		return SLURM_ERROR;
	}

	if (!bg_record->ba_mp_list || !list_count(bg_record->ba_mp_list)) {
		error("There are no midplanes in this block?");
		return SLURM_ERROR;
	}

#if defined HAVE_BG_FILES && defined HAVE_BGQ
	itr = list_iterator_create(bg_record->ba_mp_list);
	while ((ba_mp = (ba_mp_t *)list_next(itr))) {
		if (ba_mp->used)
			midplanes.push_back(ba_mp->loc);
		else
			pt_midplanes.push_back(ba_mp->loc);
	}
	list_iterator_destroy(itr);

        for (dim=Dimension::A; dim<=Dimension::D; ++dim) {
		switch (bg_record->conn_type[dim]) {
		case SELECT_MESH:
			conn_type[dim] =
				Block::Connectivity::Mesh;
			break;
		case SELECT_TORUS:
		default:
			conn_type[dim] =
				Block::Connectivity::Torus;
			break;
		}
	}
	block_ptr = Block::create(midplanes, pt_midplanes, conn_type);
	block_ptr->setName(bg_record->bg_block_id);
	block_ptr->addUser(bg_record->bg_block_id, bg_record->user_name);
	block_ptr->add(NULL);

#endif

	return rc;
}

/*
 * Boot a block. Block state expected to be FREE upon entry.
 * NOTE: This function does not wait for the boot to complete.
 * the slurm prolog script needs to perform the waiting.
 * NOTE: block_state_mutex needs to be locked before entering.
 */
extern int bridge_block_boot(bg_record_t *bg_record)
{
	int rc = SLURM_SUCCESS;

	if (bg_record->magic != BLOCK_MAGIC) {
		error("boot_block: magic was bad");
		return SLURM_ERROR;
	}

	if (!bg_record || !bg_record->bg_block_id)
		return SLURM_ERROR;

	if (!bridge_init(NULL))
		return SLURM_ERROR;

#if defined HAVE_BG_FILES && defined HAVE_BGQ
	if (bridge_block_set_owner(
		    bg_record, bg_conf->slurm_user_name) != SLURM_SUCCESS)
		return SLURM_ERROR;

        try {
		Block::initiateBoot(bg_record->bg_block_id);
	} catch(...) {
                error("Boot block request failed ... continuing.");
		rc = SLURM_ERROR;
	}
	/* Set this here just to make sure we know we are suppose to
	   be booting.  Just incase the block goes free before we
	   notice we are configuring.
	*/
	bg_record->boot_state = BG_BLOCK_BOOTING;
#else
	if (!block_ptr_exist_in_list(bg_lists->booted, bg_record))
	 	list_push(bg_lists->booted, bg_record);
	bg_record->state = BG_BLOCK_INITED;
	last_bg_update = time(NULL);
#endif
	return rc;
}

extern int bridge_block_free(bg_record_t *bg_record)
{
	int rc = SLURM_SUCCESS;
	if (!bridge_init(NULL))
		return SLURM_ERROR;

	if (!bg_record || !bg_record->bg_block_id)
		return SLURM_ERROR;

#if defined HAVE_BG_FILES && defined HAVE_BGQ
	try {
		Block::initiateFree(bg_record->bg_block_id);
	} catch(...) {
                error("Free block request failed ... continuing.");
		rc = SLURM_ERROR;
	}
#else
	bg_record->state = BG_BLOCK_FREE;
#endif
	return rc;
}

extern int bridge_block_remove(bg_record_t *bg_record)
{
	int rc = SLURM_SUCCESS;
	if (!bridge_init(NULL))
		return SLURM_ERROR;

	if (!bg_record || !bg_record->bg_block_id)
		return SLURM_ERROR;

#if defined HAVE_BG_FILES && defined HAVE_BGQ
	try {
		Block::remove(bg_record->bg_block_id);
	} catch(...) {
                error("Remove block request failed ... continuing.");
		rc = SLURM_ERROR;
	}
#endif
	return rc;
}

extern int bridge_block_add_user(bg_record_t *bg_record, char *user_name)
{
	int rc = SLURM_SUCCESS;
	if (!bridge_init(NULL))
		return SLURM_ERROR;

	if (!bg_record || !bg_record->bg_block_id || !user_name)
		return SLURM_ERROR;

#if defined HAVE_BG_FILES && defined HAVE_BGQ
        try {
		Block::addUser(bg_record->bg_block_id, user_name);
	} catch(...) {
                error("Remove block request failed ... continuing.");
		rc = SLURM_ERROR;
	}
#endif
	return rc;
}

extern int bridge_block_remove_user(bg_record_t *bg_record, char *user_name)
{
	int rc = SLURM_SUCCESS;
	if (!bridge_init(NULL))
		return SLURM_ERROR;

	if (!bg_record || !bg_record->bg_block_id || !user_name)
		return SLURM_ERROR;

#if defined HAVE_BG_FILES && defined HAVE_BGQ
        try {
		Block::removeUser(bg_record->bg_block_id, user_name);
	} catch(...) {
                error("Remove block request failed ... continuing.");
		rc = REMOVE_USER_ERR;
	}
#endif
	return rc;
}

extern int bridge_block_remove_all_users(bg_record_t *bg_record,
					 char *user_name)
{
	int rc = SLURM_SUCCESS;
#if defined HAVE_BG_FILES && defined HAVE_BGQ
	std::vector<std::string> vec;
	vector<std::string>::iterator iter;
#endif

	if (!bridge_init(NULL))
		return SLURM_ERROR;

	if (!bg_record || !bg_record->bg_block_id)
		return SLURM_ERROR;

#if defined HAVE_BG_FILES && defined HAVE_BGQ
	vec = Block::getUsers(bg_record->bg_block_id);
	if (vec.empty())
		return REMOVE_USER_NONE;

	BOOST_FOREACH(const std::string& user, vec) {
		if (user_name && (user == user_name))
			continue;
		if ((rc = bridge_block_remove_user(bg_record, user_name)
		     != SLURM_SUCCESS))
			break;
	}

#endif
	return rc;
}

extern int bridge_block_set_owner(bg_record_t *bg_record, char *user_name)
{
	int rc = SLURM_SUCCESS;
	if (!bridge_init(NULL))
		return SLURM_ERROR;

	if (!bg_record || !bg_record->bg_block_id || !user_name)
		return SLURM_ERROR;

	if ((rc = bridge_block_remove_all_users(
		     bg_record, user_name)) == REMOVE_USER_ERR) {
		error("bridge_block_set_owner: Something happened removing "
		      "users from block %s",
		      bg_record->bg_block_id);
		return SLURM_ERROR;
	} else if (rc == REMOVE_USER_NONE && user_name)
		rc = bridge_block_add_user(bg_record, user_name);

	return rc;
}

extern int bridge_block_get_and_set_mps(bg_record_t *bg_record)
{

	return SLURM_SUCCESS;
}


extern int bridge_block_wait_for_jobs(char *bg_block_id)
{
#if defined HAVE_BG_FILES && defined HAVE_BGQ
	std::vector<Job::ConstPtr> job_vec;
	JobFilter job_filter;
	JobFilter::Statuses job_statuses;
	int count = 0;
#endif

	if (!bridge_init(NULL))
		return SLURM_ERROR;

	if (!bg_block_id) {
		error("no block name given");
		return SLURM_ERROR;
	}

#if defined HAVE_BG_FILES && defined HAVE_BGQ

	job_filter.setComputeBlockName(bg_block_id);

	/* I think these are all the states we need. */
	job_statuses.insert(Job::Setup);
	job_statuses.insert(Job::Loading);
	job_statuses.insert(Job::Starting);
	job_statuses.insert(Job::Running);
	job_statuses.insert(Job::Cleanup);
	job_filter.setStatuses(&job_statuses);

	while (1) {
		if (count)
			sleep(POLL_INTERVAL);
		count++;
		job_vec = getJobs(job_filter);
		if (job_vec.empty())
			return SLURM_SUCCESS;

		BOOST_FOREACH(const Job::ConstPtr& job_ptr, job_vec) {
			debug("waiting on job %lu to finish on block %s",
			      job_ptr->getId(), bg_block_id);
		}
	}
#endif
	return SLURM_SUCCESS;
}

// extern int bridge_set_log_params(char *api_file_name, unsigned int level)
// {
// 	static FILE *fp = NULL;
//         FILE *fp2 = NULL;
// 	int rc = SLURM_SUCCESS;

// 	if (!bridge_init())
// 		return SLURM_ERROR;

// 	slurm_mutex_lock(&api_file_mutex);
// 	if (fp)
// 		fp2 = fp;

// 	fp = fopen(api_file_name, "a");

// 	if (fp == NULL) {
// 		error("can't open file for bridgeapi.log at %s: %m",
// 		      api_file_name);
// 		rc = SLURM_ERROR;
// 		goto end_it;
// 	}


// 	(*(bridge_api.set_log_params))(fp, level);
// 	/* In the libraries linked to from the bridge there are stderr
// 	   messages send which we would miss unless we dup this to the
// 	   log */
// 	//(void)dup2(fileno(fp), STDERR_FILENO);

// 	if (fp2)
// 		fclose(fp2);
// end_it:
// 	slurm_mutex_unlock(&api_file_mutex);
//  	return rc;
// }

