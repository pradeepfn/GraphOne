#include <omp.h>
#include <iostream>
#include <getopt.h>
#include <stdlib.h>
#include "graph.h"
#include "sgraph.h"
#include "type.h"
#include "typekv.h"
#include "cf_info.h"
#include "static_view.h"
#include "graph_view.h"

#define no_argument 0
#define required_argument 1
#define optional_argument 2

static const long kNumVertices = 1000;

index_t residue = 1;
int THD_COUNT = 0;
vid_t _global_vcount = 0;
index_t _edge_count = 0;
int _dir = 1;//directed
int _persist = 0;//no
int _source = 0;//text

graph* g;
pgraph_t<dst_id_t>* global_p;

int add_edge(unsigned int src, unsigned int dst)
{
    edgeT_t<dst_id_t> edge;
    edge.src_id = src;
    set_dst(&edge, dst);
    status_t s = global_p->batch_edge(edge);

    return s;
}

unsigned int query_outdegree(unsigned int node)
{
    degree_t out = global_p->get_degree_out(node);
    return out;
}

pgraph_t<dst_id_t> *pgraph;

void schema(graph *g){
    g->cf_info = new cfinfo_t *[2];
    g->p_info = new pinfo_t[2];

    pinfo_t *p_info = g->p_info;
    cfinfo_t *info = 0;
    const char *longname = 0;
    const char *shortname = 0;

    longname = "gtype";
    shortname = "gtype";
    info = new typekv_t;
    g->add_columnfamily(info);
    info->add_column(p_info, longname, shortname);
    ++p_info;

    longname = "friend";
    shortname = "friend";
    info = new dgraph<dst_id_t>;
    g->add_columnfamily(info);
    info->add_column(p_info, longname, shortname);
    info->flag1 = 1;
    info->flag2 = 1;
    ++p_info;
    pgraph = static_cast<pgraph_t<dst_id_t>*>(info);
}

int main(int argc, char* argv[]) {
        int o;
        int index = 0;
        string typefile, idir, odir;
        string queryfile;
        int category = 0;
        int job = 0;
        THD_COUNT = omp_get_max_threads() - 1;// - 3;
        cout << "Threads Count = " << THD_COUNT << endl;

        // random number for graph init.
        srand (time(NULL));

        // construct graph similar to main.cpp
        g = new graph;
        g->set_odir("/dev/shm/");
        schema(g);

        // create archiving/snapshot thread and persist/dis-write threads.
        g->create_threads(true, false);
        g->prep_graph_baseline();
        g->file_open(true);


        typekv_t* typekv = g->get_typekv();
        typekv->manual_setup(kNumVertices, true);
        egraph_t egraph_type = eADJ;
        pgraph->prep_graph_baseline(egraph_type);

        snap_t<dst_id_t>* snaph = create_static_view(pgraph, SIMPLE_MASK|V_CENTRIC);
        edgeT_t<dst_id_t>* edgePtr;
        std::cout << snaph->get_degree_out(1) << std::endl;

        global_p = pgraph;
        status_t s = pgraph->batch_update("1", "2");

		// add few edges
        s = pgraph->batch_update("1", "3");
        s = pgraph->batch_update("1", "4");
        add_edge(1,10);
        add_edge(10,1);
        add_edge(2,4);
        add_edge(3,4);

		// update view does nothing??
        snaph->update_view();

        std::cout << snaph->get_degree_out(1) << std::endl; // returns 0
        int nEdges = snaph->get_nonarchived_edges(edgePtr); // returns inserted no of edges
        std::cout << "Non-archived edges : " << nEdges << std::endl;


        for(long i = 0; i < (1 << 20); i++){
            auto src_v = rand() % kNumVertices;
            auto dest_v = rand() % kNumVertices;
            add_edge(src_v, dest_v);

			// archiving happens after every batch of (1 << 16) edge inserts.
			// Hoever, archiving  segfaults at edge_sharding.h:459
            if( (i % (1 << 16)) == 0 ){
                delete_static_view(snaph);
                snaph = create_static_view(pgraph, STALE_MASK|V_CENTRIC);
                std::cout << "snapshot graph : " << snaph->get_degree_out(1) << std::endl;
                std::cout << "Non-archived edges : " << snaph->get_nonarchived_edges(edgePtr) << std::endl;
            }
        }

		std::cout << s << std::endl;
        return 0;
    }
