#include <mctop.h>
#include <stdarg.h>
#include <math.h>

const char* dot_prefix = "dot";

static void
print2(FILE* ofp, const char* format, ...)
{
  va_list args0, args1;
  va_start(args0, format);
  va_copy(args1, args0);
  vfprintf(stdout, format, args0);
  va_end(args1);

  if (ofp != NULL)
    {
      va_start(args1, format);
      vfprintf(ofp, format, args1);
      va_end(args1);
    }
}

static void
dot_tab(FILE* ofp, const uint n_tabs)
{
  for (int t = 0; t < n_tabs; t++)
    {
      print2(ofp, "\t");
    }
}

static void
dot_tab_print(FILE* ofp, const uint n_tabs, const char* print)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "%s", print);
}

static void
dot_graph(FILE* ofp, const char* title, const uint id)
{
  print2(ofp, "graph mctop_%s_%u\n{\n", title,id);
  print2(ofp, "\tlabelloc=\"t\";\n");
  print2(ofp, "\tcompound=true;\n");
  print2(ofp, "\tnode [shape=record];\n");
  print2(ofp, "\tnode [color=gray];\n");
  print2(ofp, "\tedge [fontcolor=blue];\n");
}

static void
dot_subgraph(FILE* ofp, const uint n_tabs, const uint id)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "subgraph cluster_%u\n", id);
  dot_tab_print(ofp, n_tabs, "{\n");
}

static void
dot_socket(FILE* ofp, const uint n_tabs, const uint id, const uint lat)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "label=\"Socket #%u (Latency: %d)\";\n", mctop_id_no_lvl(id), lat);
}

static void
dot_label(FILE* ofp, const uint n_tabs, const uint lat)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "label=\"%u cycles\";\n", lat);
}

static void
dot_gs_label(FILE* ofp, const uint n_tabs, const uint id)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "gs_%u [label=\"", id);
}

static void
dot_gs_label_id(FILE* ofp, const uint n_tabs, const uint id)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "gs_%u [label=\"%u\"];\n", id, mctop_id_no_lvl(id));
}


static uint dot_min_lat = 0;
const uint dot_weigh_multi = 10000;

static uint
dot_weight_calc(const uint lat)
{
  return dot_weigh_multi / (lat - dot_min_lat);
}

static void
dot_gs_link(FILE* ofp, const uint n_tabs, const uint id, const uint lat)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "gs_%u -- gs_%u [label=\"%u\", weight=\"%u\"];\n", id, id, lat, dot_weight_calc(lat));
}

static void
dot_gss_link(FILE* ofp, const uint n_tabs, const uint id0, const uint id1, const uint lat)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "gs_%u -- gs_%u [label=\"%u\", weight=\"%u\"];\n", id0, id1, lat, dot_weight_calc(lat));
}

static void
dot_gss_link_invis(FILE* ofp, const uint n_tabs, const uint id0, const uint id1)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "gs_%u -- gs_%u [style=invis];\n", id0, id1);
}

static void
dot_gss_link_bw(FILE* ofp, const uint n_tabs, const uint id0, const uint id1, const uint lat, const double bw)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "gs_%u -- gs_%u [label=\"%u (%.2fGB/s)\", weight=\"%u\"];\n", id0, id1, lat, bw, dot_weight_calc(lat));
}

__attribute__((unused)) static void
dot_gs_link_only_bw(FILE* ofp, const uint n_tabs, const uint id, const double bw)
{
  dot_tab(ofp, n_tabs);
  print2(ofp, "gs_%u -- gs_%u [label=\"%.2fGB/s\"];\n", id, id, bw);
}

void
dot_gs_recurse(FILE* ofp, hwc_gs_t* gs, const uint n_tabs)
{
  if (gs->level == 1)
    {
      dot_gs_label(ofp, n_tabs, gs->id);
      for (int i = 0; i < gs->n_children; i++)
	{
	  if (i > 0)
	    {
	      print2(ofp, "|");
	    }
	  print2(ofp, "%03u", gs->children[i]->id);
	}
      print2(ofp, "\"];\n");
      if (gs->type != SOCKET)
	{
	  dot_gs_link(ofp, n_tabs, gs->id, gs->latency);
	}
    }
  else 
    {
      const uint new_subgraph = gs->type != SOCKET;
      if (new_subgraph)
	{
	  dot_subgraph(ofp, n_tabs, gs->id);
	  dot_label(ofp, n_tabs + 1, gs->latency);
	}
      for (int i = gs->n_children - 1; i >= 0; i--)
	{
	  dot_gs_recurse(ofp, gs->children[i], n_tabs + 1);
	}
      if (new_subgraph)
	{
	  dot_tab_print(ofp, n_tabs, "}\n");
	}
    }
}

static uint
do_dont_link_cores_every(const uint n_cores)
{
  uint root = sqrt(n_cores);
  uint every = root;
  uint tries = 0;
  do
    {
      uint tot = every * root;
      if (tot == n_cores) { break; }
      else if (tot < n_cores) { every++; }
      else { every--; }

      tries++;
      if (tries == 16) { root++; }
      else if (tries == 32) { root -= 2; }
      else if (tries == 48) { root++;  break; }
    }
  while (1);
  return root;
}

static void
dot_gs_add_invisible_links(FILE* ofp, socket_t* socket)
{
  if (socket->level > 1)
    {
      uint n_cores = mctop_socket_get_num_cores(socket);
      uint every = do_dont_link_cores_every(n_cores);
      hwc_gs_t* pgs = NULL;
      hwc_gs_t* gs = mctop_socket_get_first_gs_core(socket);
      for (int i = 1; i <  n_cores; i++)
	{
	  pgs = gs;
	  gs = gs->next;
	  if (i % every != 0)
	    {
	      dot_gss_link_invis(ofp, 1, pgs->id, gs->id);
	    }
	}
    }
}

void
mctop_dot_graph_intra_socket_plot(mctop_t* topo)
{
  char out_file[100];
  sprintf(out_file, "dot/%s_intra_socket.dot", dot_prefix);
  FILE* ofp = fopen(out_file, "w+");
  if (ofp == NULL)
    {
      fprintf(stderr, "MCTOP Warning: Cannot open output file %s! Will only plot at stdout.\n", out_file);
    }

  for (int s = 0; s < topo->n_sockets; s++)
    {
      socket_t* socket = &topo->sockets[s];
      dot_graph(ofp, "intra_socket", s);
      dot_subgraph(ofp, 1, socket->id);
      dot_socket(ofp, 2, socket->id, socket->latency);
      dot_gs_recurse(ofp, socket, 1);
      dot_tab_print(ofp, 1, "}\n");

      dot_gs_add_invisible_links(ofp, socket);

      dot_tab_print(ofp, 1, "node [color=red4];\n");

      if (topo->has_mem >= LATENCY)
	{
	  dot_tab(ofp, 1); print2(ofp, "//Memory latencies node %u\n", socket->id);
	  for (int i = 0; i < socket->n_nodes; i++)
	    {
	      dot_tab(ofp, 1);
	      if (i == socket->local_node)
		{
		  print2(ofp, "mem_lat_%u_%u [label=\"Nod#%u\\n%u cy\", color=\"red\"];\n", 
			 i, socket->id, i, socket->mem_latencies[i]);
		}
	      else
		{
		  print2(ofp, "mem_lat_%u_%u [label=\"Nod#%u\\n%u cy\"];\n", 
			 i, socket->id, i, socket->mem_latencies[i]);
		}

	      dot_tab(ofp, 1);
	      if (socket->level == 1)
		{
		  if (topo->has_mem == BANDWIDTH)
		    {
		      print2(ofp, "mem_lat_%u_%u -- gs_%u [lhead=cluster_%u, label=\"%.1fGB/s\"];\n", 
			     i, socket->id, socket->id, socket->id, socket->mem_bandwidths[i]);
		    }
		  else
		    {
		      print2(ofp, "mem_lat_%u_%u -- gs_%u [lhead=cluster_%u];\n", 
			     i, socket->id, socket->id, socket->id);
		    }
		}
	      else
		{
		  if (topo->has_mem == BANDWIDTH)
		    {
		      print2(ofp, "mem_lat_%u_%u -- gs_%u [lhead=cluster_%u, label=\"%.1fGB/s\"];\n", 
			     i, socket->id, socket->children[0]->id, socket->id, socket->mem_bandwidths[i]);
		    }
		  else
		    {
		      print2(ofp, "mem_lat_%u_%u -- gs_%u [lhead=cluster_%u];\n", 
			     i, socket->id, socket->children[0]->id, socket->id);
		    }
		}
	    }
	}

      print2(ofp, "}\n");
    }


  if (ofp != NULL)
    {
      fclose(ofp);
    }
}

void
mctop_dot_graph_cross_socket_plot(mctop_t* topo, const uint max_cross_socket_lvl)
{
  char out_file[100];
  sprintf(out_file, "dot/%s_cross_socket.dot", dot_prefix);
  FILE* ofp = fopen(out_file, "w+");
  if (ofp == NULL)
    {
      fprintf(stderr, "MCTOP Warning: Cannot open output file %s! Will only plot at stdout.\n", out_file);
    }

  if (topo->socket_level < topo->n_levels)
    {
      dot_min_lat = topo->latencies[topo->socket_level + 1] - 1;
      printf(" [ set dot_min_lat = %u]\n", dot_min_lat);
    }

  dot_graph(ofp, "cross_socket", 0);
  dot_tab_print(ofp, 1, "node [color=red4];\n");

  dot_tab(ofp, 1); print2(ofp, "// Socket       lvl %u (max lvl %u)\n", topo->socket_level, topo->n_levels);
  for (int i = 0; i < topo->n_sockets; i++)
    {
      dot_gs_label_id(ofp, 1, topo->sockets[i].id);
    }

  for (int lvl = topo->socket_level + 1; lvl < topo->n_levels; lvl++)
    {
      if (max_cross_socket_lvl == 0 || lvl < max_cross_socket_lvl)
	{
	  dot_tab(ofp, 1); print2(ofp, "// Cross-socket lvl %u (max lvl %u)\n", lvl, topo->n_levels);
	  for (int i = 0; i < topo->n_siblings; i++)
	    {
	      sibling_t* sibling = topo->siblings[i];
	      if (sibling->level == lvl)
		{
		  if (topo->has_mem == BANDWIDTH)
		    {
		      double bw = sibling->left->mem_bandwidths[sibling->right->local_node];
		      dot_gss_link_bw(ofp, 1, sibling->left->id, sibling->right->id, sibling->latency, bw);
		    }
		  else
		    {
		      dot_gss_link(ofp, 1, sibling->left->id, sibling->right->id, sibling->latency);
		    }
		}
	    }
	}
      else			/* create a separate group */
	{
	  dot_gs_label(ofp, 1, lvl);
	  print2(ofp, "lvl %u\\n(%u hops)\"];\n", lvl, lvl - topo->socket_level);
	  dot_gss_link(ofp, 1, lvl, lvl, topo->latencies[lvl]);
	}
    }

#if MCTOP_GRAPH_BW_SELF == 1
  if (topo->has_mem == BANDWIDTH)
    {
      for (int i = 0; i < topo->n_sockets; i++)
	{
	  socket_t* socket = &topo->sockets[i];
	  dot_gs_link_only_bw(ofp, 1, socket->id, socket->mem_bandwidths[socket->local_node]);
	}
    }
#endif	/* MCTOP_GRAPH_BW_SELF == 1 */

  print2(ofp, "}\n");

  if (ofp != NULL)
    {
      fclose(ofp);
    }
}

void
mctop_dot_graph_plot(mctop_t* topo, const uint max_cross_socket_lvl)
{
  mctop_dot_graph_intra_socket_plot(topo);
  mctop_dot_graph_cross_socket_plot(topo, max_cross_socket_lvl);
}
