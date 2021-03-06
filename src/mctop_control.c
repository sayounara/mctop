#include <mctop.h>
#include <darray.h>
#ifdef __x86_64__
#  include <numa.h>
#endif

/* topo getters ******************************************************************* */

inline socket_t*
mctop_get_socket(mctop_t* topo, const uint socket_n)
{
  return topo->sockets + socket_n;
}

inline socket_t*
mctop_get_first_socket(mctop_t* topo)
{
  return topo->sockets;
}

hwc_gs_t*
mctop_get_first_gs_core(mctop_t* topo)
{
  hwc_gs_t* gs = topo->sockets[0].children[0];
  while (gs && gs->type != CORE)
    {
      gs = gs->children[0];
    }
  return gs;
}

inline hwc_gs_t*
mctop_get_first_gs_at_lvl(mctop_t* topo, const uint lvl)
{
  hwc_gs_t* cur = mctop_get_first_socket(topo);
  while (cur != NULL && cur->level != lvl)
    {
      cur = cur->children[0];
    }
  return cur;
}


inline sibling_t*
mctop_get_first_sibling_lvl(mctop_t* topo, const uint lvl)
{
  for (int i = 0; i < topo->n_siblings; i++)
    {
      if (topo->siblings[i]->level == lvl)
	{
	  return topo->siblings[i];
	}
    }
  return NULL;
}

inline uint
mctop_get_num_levels(mctop_t* topo)
{
  return topo->n_levels;
}

inline size_t
mctop_get_num_nodes(mctop_t* topo)
{
  return topo->n_sockets;
}

size_t 
mctop_get_num_cores(mctop_t* topo)
{
  return topo->n_hwcs / topo->n_hwcs_per_core;
}


inline size_t
mctop_get_num_cores_per_socket(mctop_t* topo)
{
  return topo->sockets[0].n_cores;
}

inline size_t
mctop_get_num_hwc_per_socket(mctop_t* topo)
{
  return topo->sockets[0].n_hwcs;
}

inline size_t
mctop_get_num_hwc_per_core(mctop_t* topo)
{
  return topo->n_hwcs_per_core;
}

sibling_t*
mctop_get_sibling_with_sockets(mctop_t* topo, socket_t* s0, socket_t* s1)
{
  for (int i = 0; i < topo->n_siblings; i++)
    {
      sibling_t* sibling = topo->siblings[i];
      if (mctop_sibling_contains_sockets(sibling, s0, s1))
	{
	  return sibling;
	}
    }

  return NULL;
}

size_t
mctop_get_cache_size_kb(mctop_t* topo, mctop_cache_level_t level)
{
  mctop_cache_info_t* mci = topo->cache;
  if (mci)
    {
      return mci->sizes_OS[level] ? mci->sizes_OS[level] : mci->sizes_estimated[level];
    }
  return 0;
}
  /* estimated size and latency not defined for L1I */
size_t
mctop_get_cache_size_estimated_kb(mctop_t* topo, mctop_cache_level_t level)
{
  if (topo->cache)
    {
      return topo->cache->sizes_estimated[level];
    }
  return 0;
}

size_t
mctop_get_cache_latency(mctop_t* topo, mctop_cache_level_t level)
{
  if (topo->cache)
    {
      return topo->cache->latencies[level];
    }
  return 0;
}



/* socket getters ***************************************************************** */

inline hw_context_t*
mctop_socket_get_first_hwc(socket_t* socket)
{
  return socket->hwcs[0];
}

inline hw_context_t*
mctop_socket_get_nth_hwc(socket_t* socket, const uint nth)
{
  return socket->hwcs[nth];
}

hwc_gs_t*
mctop_socket_get_first_gs_core(socket_t* socket)
{
  hwc_gs_t* gs = socket->children[0];
  while (gs && gs->type != CORE)
    {
      gs = gs->children[0];
    }
  return gs;
}

hwc_gs_t*
mctop_socket_get_nth_gs_core(socket_t* socket, const uint nth)
{
  hwc_gs_t* gs = socket->children[0];
  while (gs && gs->type != CORE)
    {
      gs = gs->children[0];
    }
  for (int i = 0; i < nth; i++)
    {
      gs = gs->next;
    }
  return gs;
}

hwc_gs_t*
mctop_socket_get_first_child_lvl(socket_t* socket, const uint lvl)
{
  hwc_gs_t* cur = socket->children[0];
  while (cur != NULL && cur->level != lvl)
    {
      cur = cur->children[0];
    }
  return cur;
}

inline size_t
mctop_socket_get_num_cores(socket_t* socket)
{
  return socket->n_cores;
}

inline size_t
mctop_socket_get_num_hw_contexts(socket_t* socket)
{
  return socket->n_hwcs;
}

inline double
mctop_socket_get_bw_local(socket_t* socket)
{
  return socket->mem_bandwidths_r[socket->local_node];
}

inline double
mctop_socket_get_bw_local_one(socket_t* socket)
{
  return socket->mem_bandwidths1_r[socket->local_node];
}

inline uint
mctop_socket_get_local_node(socket_t* socket)
{
  return socket->local_node;
}

inline double
mctop_socket_get_bw_to(socket_t* socket, socket_t* to)
{
  return socket->mem_bandwidths_r[to->local_node];
}

/* node getters ******************************************************************** */

inline socket_t*
mctop_node_to_socket(mctop_t* topo, const uint nid)
{
  if (likely(topo->has_mem) && nid < mctop_get_num_nodes(topo))
    {
      return &topo->sockets[topo->node_to_socket[nid]];
    }
  return NULL;
}



/* sibling getters ***************************************************************** */

socket_t*
mctop_sibling_get_other_socket(sibling_t* sibling, socket_t* socket)
{
  if (sibling->left == socket)
    {
      return sibling->right;
    }
  return sibling->left;
}

uint
mctop_sibling_contains_sockets(sibling_t* sibling, socket_t* s0, socket_t* s1)
{
  if ((sibling->left == s0 && sibling->right == s1) ||
      (sibling->left == s1 && sibling->right == s0))
    {
      return 1;
    }
  return 0;
}



/* hwcid ************************************************************************ */

int
mctop_hwcid_fix_numa_node(mctop_t* topo, const uint hwcid)
{
#ifdef __x86_64__
  /* printf("# HWID %-3u, SOCKET %-3u, numa_set_preferred(%u)\n", */
  /* 	     hwcid, hwc->socket->id, hwc->socket->local_node); */
  hw_context_t* hwc = &topo->hwcs[hwcid];
  const uint correct_node = hwc->socket->local_node;
  if (unlikely(hwc->local_node_wrong || correct_node != numa_preferred()))
    {
      numa_set_preferred(correct_node);
      return 1;
    }
#endif
  return 0;
}

inline uint
mctop_hwcid_get_local_node(mctop_t* topo, uint hwcid)
{
  return mctop_hwcid_get_socket(topo, hwcid)->local_node;
}

inline socket_t*
mctop_hwcid_get_socket(mctop_t* topo, uint hwcid)
{
  return topo->hwcs[hwcid].socket;
}

hwc_gs_t*
mctop_hwcid_get_core(mctop_t* topo, const uint hwcid)
{
  hw_context_t* hwc = &topo->hwcs[hwcid];
  if (hwc->type == CORE)
    {
      return (hwc_gs_t*) hwc;
    }
  else
    {
      return hwc->parent;
    }
}

uint
mctop_hwcid_get_nth_hwc_in_socket(mctop_t* topo, const uint hwcid)
{
  socket_t* socket = mctop_hwcid_get_socket(topo, hwcid);
  hw_context_t* hwc_first = mctop_socket_get_first_hwc(socket);
  uint i = 0;
  while (hwc_first && hwc_first->id != hwcid)
    {
      i++;
      hwc_first = hwc_first->next;
    }

  return i;
}

uint
mctop_hwcid_get_nth_hwc_in_core(mctop_t* topo, const uint hwcid)
{
  hw_context_t* hwc = &topo->hwcs[hwcid];
  if (hwc->type == CORE)
    {
      return 0;
    }
  hwc_gs_t* core = hwc->parent;
  for (int i = 0; i < core->n_hwcs; i++)
    {
      if (core->hwcs[i]->id == hwc->id)
	{
	  return i;
	}
    }
  assert(0);
}

uint
mctop_hwcid_get_nth_core_in_socket(mctop_t* topo, const uint hwcid)
{
  hwc_gs_t* core = mctop_hwcid_get_core(topo, hwcid);
  hwc_gs_t* core_first = mctop_socket_get_first_gs_core(core->socket);
  uint i = 0;
  while (core_first && core->id != core_first->id)
    {
      i++;
      core_first = core_first->next;
    }

  return i;
}


/* queries ************************************************************************ */

inline uint
mctop_hwcs_are_same_core(hw_context_t* a, hw_context_t* b)
{
  return (a->type == HW_CONTEXT && b->type == HW_CONTEXT && a->parent == b->parent);
}

inline uint
mctop_has_mem_lat(mctop_t* topo)
{
  return topo->has_mem >= LATENCY;
}

inline uint
mctop_has_mem_bw(mctop_t* topo)
{
  return topo->has_mem == BANDWIDTH;
}

hwc_gs_t*
mctop_id_get_hwc_gs(mctop_t* topo, const uint id)
{
  uint lvl = mctop_id_get_lvl(id);
  if (lvl > topo->n_levels)
    {
      return NULL;
    }
  hwc_gs_t* gs = NULL;
  if (lvl == 0)
    {
      gs = (hwc_gs_t*) &topo->hwcs[id];
    }
  else if (lvl == topo->socket_level)
    {
      gs = (hwc_gs_t*) &topo->sockets[mctop_id_no_lvl(id)];
    }
  else
    {
      hwc_gs_t* _gs = mctop_get_first_gs_at_lvl(topo, lvl);
      while (_gs != NULL)
	{
	  if (unlikely(_gs->id == id))
	    {
	      gs = _gs;
	      break;
	    }
	  _gs = _gs->next;
	}
    }

  return gs;
}

uint
mctop_ids_get_latency(mctop_t* topo, const uint id0, const uint id1)
{
  hwc_gs_t* gs0 = mctop_id_get_hwc_gs(topo, id0);
  hwc_gs_t* gs1 = mctop_id_get_hwc_gs(topo, id1);
  if (gs0 == NULL || gs1 == NULL)
    {
      return 0;
    }

  while (gs0->level < gs1->level)
    {
      gs0 = gs0->parent;
    }
  while (gs1->level < gs0->level)
    {
      gs1 = gs1->parent;
    }

  if (unlikely(gs0->id == gs1->id))
    {
      if (gs0->level == 0)
	{
	  return 0;
	}
      else
	{
	  return gs0->latency;
	}
    }

  while (gs0->type != SOCKET && gs1->type != SOCKET)
    {
      gs0 = gs0->parent;
      gs1 = gs1->parent;
      if (gs0->id == gs1->id)
	{
	  return gs0->latency;
	}
    }


  sibling_t* sibling = mctop_get_sibling_with_sockets(topo, gs0, gs1);
  return sibling->latency;
}



/* pining ************************************************************************ */

int
mctop_run_on_socket_ref(socket_t* socket, const uint fix_mem)
{
  int ret = 0;
  if (socket == NULL)
    {
      return -EINVAL;
    }

#ifdef __x86_64__
  struct bitmask* bmask = numa_bitmask_alloc(socket->topo->n_hwcs);
  for (int i = 0; i < socket->n_hwcs; i++) 
    {
      bmask = numa_bitmask_setbit(bmask, socket->hwcs[i]->id);
    }

  ret = numa_sched_setaffinity(0, bmask);
  if (fix_mem && !ret && socket->topo->has_mem)
    {
      numa_set_preferred(socket->local_node);
    }
  numa_bitmask_free(bmask);
#else
  ret = mctop_run_on_node(socket->topo, socket->local_node);
#endif
  return ret;
}

int
mctop_run_on_socket(mctop_t* topo, const uint socket_n)
{
  if (socket_n >= topo->n_sockets)
    {
      return -EINVAL;
    }
  socket_t* socket = &topo->sockets[socket_n];
  return mctop_run_on_socket_ref(socket, 0);
}

int
mctop_run_on_socket_nm(mctop_t* topo, const uint socket_n)
{
  if (socket_n >= topo->n_sockets)
    {
      return -EINVAL;
    }
  socket_t* socket = &topo->sockets[socket_n];
  return mctop_run_on_socket_ref(socket, 0);
}

int
mctop_run_on_node(mctop_t* topo, const uint node_n)
{
#if __x86_64__
  if (node_n >= topo->n_sockets)
    {
      return -EINVAL;
    }

  const uint socket_n = topo->node_to_socket[node_n];
  socket_t* socket = &topo->sockets[socket_n];
  return mctop_run_on_socket_ref(socket, 0);
#elif __sparc
  return numa_run_on_node(node_n);
#endif
}



/* if topo == NULL: don't try to fix the NUMA node */
int
mctop_set_cpu(mctop_t* topo, int cpu) 
{
  int ret = 1;
#if defined(__sparc__)
  if (processor_bind(P_LWPID, P_MYID, cpu, NULL) == -1)
    {
      /* printf("Problem with setting processor affinity: %s\n", */
      /* 	     strerror(errno)); */
      ret = 0;
    }
#elif defined(__tile__)
  if (tmc_cpus_set_my_cpu(tmc_cpus_find_nth_cpu(&cpus, cpu)) < 0)
    {
      tmc_task_die("Failure in 'tmc_cpus_set_my_cpu()'.");
    }

  if (cpu != tmc_cpus_get_my_cpu())
    {
      PRINT("******* i am not CPU %d", tmc_cpus_get_my_cpu());
    }

#else
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(cpu, &mask);
  if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0)
    {
      /* printf("Problem with setting processor affinity: %s\n", */
      /* 	     strerror(errno)); */
      ret = 0;
    }

  if (likely(topo != NULL))
    {
      mctop_hwcid_fix_numa_node(topo, cpu);
    }
#endif

  return ret;
}
