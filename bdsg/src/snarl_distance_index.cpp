//#define debug_distance_indexing
//#define debug_snarl_traversal
//#define debug_distances

#include "snarl_distance_index.hpp"

using namespace std;
using namespace handlegraph;
namespace vg {


///////////////////////////////////////////////////////////////////////////////////////////////////
//Constructor
SnarlDistanceIndex::SnarlDistanceIndex() {
    snarl_tree_records.construct(tag);
    snarl_tree_records->width(64);
}
SnarlDistanceIndex::~SnarlDistanceIndex() {
}


/*Temporary distance index for constructing the index from the graph
 */
SnarlDistanceIndex::TemporaryDistanceIndex::TemporaryDistanceIndex(){}

SnarlDistanceIndex::TemporaryDistanceIndex::~TemporaryDistanceIndex(){}

string SnarlDistanceIndex::TemporaryDistanceIndex::structure_start_end_as_string(pair<temp_record_t, size_t> index) const {
    if (index.first == TEMP_NODE) {
        assert(index.second == temp_node_records[index.second-min_node_id].node_id);
        return "node " + std::to_string(temp_node_records[index.second-min_node_id].node_id);
    } else if (index.first == TEMP_SNARL) {
        const TemporarySnarlRecord& temp_snarl_record =  temp_snarl_records[index.second];
        return "snarl " + std::to_string( temp_snarl_record.start_node_id) 
                + (temp_snarl_record.start_node_rev ? " rev" : " fd") 
                + " -> " + std::to_string( temp_snarl_record.end_node_id) 
                + (temp_snarl_record.end_node_rev ? " rev" : " fd");
    } else if (index.first == TEMP_CHAIN) {
        const TemporaryChainRecord& temp_chain_record = temp_chain_records[index.second];
        return "chain " + std::to_string( temp_chain_record.start_node_id) 
                + (temp_chain_record.start_node_rev ? " rev" : " fd") 
                + " -> "  + std::to_string( temp_chain_record.end_node_id) 
                + (temp_chain_record.end_node_rev ? " rev" : " fd");
    } else if (index.first == TEMP_ROOT) {
        return (string) "root";
    } else {
        return (string)"???" + std::to_string(index.first) + "???";
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////
//Implement the SnarlDecomposition's functions for moving around the snarl tree
//


net_handle_t SnarlDistanceIndex::get_root() const {
    // The root is the first thing in the index, the traversal is tip to tip
    return get_net_handle(0, START_END, ROOT_HANDLE);
}

bool SnarlDistanceIndex::is_root(const net_handle_t& net) const {
    return get_handle_type(net) == ROOT_HANDLE;
}

bool SnarlDistanceIndex::is_snarl(const net_handle_t& net) const {
    return get_handle_type(net) == SNARL_HANDLE 
            && (SnarlTreeRecord(net, &snarl_tree_records).get_record_handle_type() == SNARL_HANDLE ||
                SnarlTreeRecord(net, &snarl_tree_records).get_record_type() == ROOT_SNARL ||  
                SnarlTreeRecord(net, &snarl_tree_records).get_record_type() == DISTANCED_ROOT_SNARL);
}

bool SnarlDistanceIndex::is_chain(const net_handle_t& net) const {
    return get_handle_type(net) == CHAIN_HANDLE
            && (SnarlTreeRecord(net, &snarl_tree_records).get_record_handle_type() == CHAIN_HANDLE
            || SnarlTreeRecord(net, &snarl_tree_records).get_record_handle_type() == NODE_HANDLE);
}

bool SnarlDistanceIndex::is_trivial_chain(const net_handle_t& net) const {
    return get_handle_type(net) == CHAIN_HANDLE
            && SnarlTreeRecord(net, &snarl_tree_records).get_record_handle_type() == NODE_HANDLE;
}

bool SnarlDistanceIndex::is_node(const net_handle_t& net) const {
    return get_handle_type(net) == NODE_HANDLE 
            && SnarlTreeRecord(net, &snarl_tree_records).get_record_handle_type() == NODE_HANDLE;
}
bool SnarlDistanceIndex::is_sentinel(const net_handle_t& net) const {
    return get_handle_type(net) == SENTINEL_HANDLE
            && SnarlTreeRecord(net, &snarl_tree_records).get_record_handle_type() == SNARL_HANDLE;
}

net_handle_t SnarlDistanceIndex::get_net(const handle_t& handle, const handlegraph::HandleGraph* graph) const{
    return get_net_handle(get_offset_from_node_id(graph->get_id(handle)), 
                          graph->get_is_reverse(handle) ? END_START : START_END, 
                          NODE_HANDLE);
}
handle_t SnarlDistanceIndex::get_handle(const net_handle_t& net, const handlegraph::HandleGraph* graph) const{
    //TODO: Maybe also want to be able to get the graph handle of a sentinel
    if (get_handle_type(net) == SENTINEL_HANDLE) {
        SnarlRecord snarl_record(net, &snarl_tree_records);
        if (starts_at(net) == START) {
            return graph->get_handle(snarl_record.get_start_id(), 
                       ends_at(net) == START ? !snarl_record.get_start_orientation()   //Going out
                                             : snarl_record.get_start_orientation());  //Going in
        } else {
            assert (starts_at(net) == END);
            return graph->get_handle(snarl_record.get_end_id(), 
                       ends_at(net) == END ? snarl_record.get_end_orientation()   //Going out
                                           : !snarl_record.get_end_orientation());  //Going in
        }
    } else if (get_handle_type(net) == NODE_HANDLE || get_handle_type(net) == CHAIN_HANDLE ) {
        NodeRecord node_record(net, &snarl_tree_records);
        return graph->get_handle(get_node_id_from_offset(get_record_offset(net)), 
                                 ends_at(net) == END ? false : true);
    } else {
        throw runtime_error("error: trying to get a handle from a snarl, chain, or root");
    }
}

net_handle_t SnarlDistanceIndex::get_parent(const net_handle_t& child) const {

    //If the child is the sentinel of a snarl, just return the snarl
    if (get_handle_type(child) == SENTINEL_HANDLE) {
        return get_net_handle(get_record_offset(child), START_END, SNARL_HANDLE); 
    } else if (get_handle_type(child) == ROOT_HANDLE) {
        throw runtime_error("error: trying to find the parent of the root");
    } 

    //Otherwise, we need to move up one level in the snarl tree

    //Get the pointer to the parent, and keep the connectivity of the current handle
    size_t parent_pointer = SnarlTreeRecord(child, &snarl_tree_records).get_parent_record_offset();
    connectivity_t child_connectivity = get_connectivity(child);

    //TODO: I"m going into the parent record here, which could be avoided if things knew what their parents were, but I think if you're doing this you'd later go into the parent anyway so it's probably fine
    net_handle_record_t parent_type = SnarlTreeRecord(parent_pointer, &snarl_tree_records).get_record_handle_type();
    connectivity_t parent_connectivity = START_END;
    if ((child_connectivity == START_END || child_connectivity == END_START) 
        && (parent_type == CHAIN_HANDLE)) {
        //TODO: This also needs to take into account the orientation of the child, which I might be able to get around?
        parent_connectivity = child_connectivity;
    }
    if (get_handle_type(child) == NODE_HANDLE && parent_type != CHAIN_HANDLE) {
        //If this is a node and it's parent is not a chain, we want to pretend that its 
        //parent is a chain
        return get_net_handle(get_record_offset(child), parent_connectivity, CHAIN_HANDLE);
    } 

    return get_net_handle(parent_pointer, parent_connectivity);
}

net_handle_t SnarlDistanceIndex::get_bound(const net_handle_t& snarl, bool get_end, bool face_in) const {
    if (get_handle_type(snarl) == CHAIN_HANDLE) {
        id_t id = get_end ? SnarlTreeRecord(snarl, &snarl_tree_records).get_end_id() 
                          : SnarlTreeRecord(snarl, &snarl_tree_records).get_start_id();
        bool rev_in_parent = NodeRecord(get_offset_from_node_id(id), &snarl_tree_records).get_is_reversed_in_parent();
        if (get_end) {
            rev_in_parent = !rev_in_parent;
        }
        if (!face_in){
            rev_in_parent = !rev_in_parent;
        }
        connectivity_t connectivity = rev_in_parent ? END_START : START_END;
        if (SnarlTreeRecord(snarl, &snarl_tree_records).get_start_id() 
                == SnarlTreeRecord(snarl, &snarl_tree_records).get_end_id() &&  get_end) {
            //If this is a looping chain and we're getting the end, then the traversal of the node will be the 
            //end endpoint repeated, instead of the actual traversal
            //TODO: This might cause problems when checking traversals but I think it's fine
            connectivity = endpoints_to_connectivity(get_end_endpoint(connectivity), get_end_endpoint(connectivity));
        }
        return get_net_handle(get_offset_from_node_id(id), connectivity,  NODE_HANDLE);
    } else {
        assert(get_handle_type(snarl) == SNARL_HANDLE);
        endpoint_t start = get_end ? END : START;
        endpoint_t end = face_in ? (start == END ? START : END) : start;
        return get_net_handle(get_record_offset(snarl), endpoints_to_connectivity(start, end), SENTINEL_HANDLE);
    }
}

net_handle_t SnarlDistanceIndex::flip(const net_handle_t& net) const {
    connectivity_t old_connectivity = get_connectivity(net);
    connectivity_t new_connectivity =  endpoints_to_connectivity(get_end_endpoint(old_connectivity), 
                                                                get_start_endpoint(old_connectivity));
    return get_net_handle(get_record_offset(net), new_connectivity, get_handle_type(net));
}

net_handle_t SnarlDistanceIndex::canonical(const net_handle_t& net) const {
    SnarlTreeRecord record(net, &snarl_tree_records);
    connectivity_t connectivity;
    if (record.is_start_end_connected()) {
        connectivity = START_END;
    } else if (record.is_start_tip_connected()) {
        connectivity = START_TIP;
    } else if (record.is_end_tip_connected()) {
        connectivity = END_TIP;
    } else if (record.is_start_start_connected()) {
        connectivity = START_START;
    } else if (record.is_end_end_connected()) {
        connectivity = END_END;
    } else if (record.is_tip_tip_connected()) {
        connectivity = TIP_TIP;
    } else {
        connectivity = START_END; //TODO: put this back throw runtime_error("error: This node has no connectivity");
    }
    return get_net_handle(get_record_offset(net), connectivity, get_handle_type(net));
}

SnarlDecomposition::endpoint_t SnarlDistanceIndex::starts_at(const net_handle_t& traversal) const {
    return get_start_endpoint(get_connectivity(traversal));

}
SnarlDecomposition::endpoint_t SnarlDistanceIndex::ends_at(const net_handle_t& traversal) const {
    return get_end_endpoint( get_connectivity(traversal));
}

//TODO: I'm also allowing this for the root
bool SnarlDistanceIndex::for_each_child_impl(const net_handle_t& traversal, const std::function<bool(const net_handle_t&)>& iteratee) const {
#ifdef debug_snarl_traversal
    cerr << "Go through children of " << net_handle_as_string(traversal) << endl;
#endif
    //What is this according to the snarl tree
    net_handle_record_t record_type = SnarlTreeRecord(traversal, &snarl_tree_records).get_record_handle_type();
    //What is this according to the handle 
    //(could be a trivial chain but actually a node according to the snarl tree)
    net_handle_record_t handle_type = get_handle_type(traversal);
    if (record_type == SNARL_HANDLE) {
        SnarlRecord snarl_record(traversal, &snarl_tree_records);
        return snarl_record.for_each_child(iteratee);
    } else if (record_type == CHAIN_HANDLE) {
        ChainRecord chain_record(traversal, &snarl_tree_records);
        return chain_record.for_each_child(iteratee);
    } else if (record_type == ROOT_HANDLE) {
        RootRecord root_record(traversal, &snarl_tree_records);
        return root_record.for_each_child(iteratee);
    } else if (record_type == NODE_HANDLE && handle_type == CHAIN_HANDLE) {
        //This is actually a node but we're pretending it's a chain
#ifdef debug_snarl_traversal
        cerr << "     which is actually a node pretending to be a chain" << endl;
#endif
        return iteratee(get_net_handle(get_record_offset(traversal), get_connectivity(traversal), NODE_HANDLE));
    } else {
        throw runtime_error("error: Looking for children of a node or sentinel");
    }
   
}

bool SnarlDistanceIndex::for_each_traversal_impl(const net_handle_t& item, const std::function<bool(const net_handle_t&)>& iteratee) const {
    if (get_handle_type(item) == SENTINEL_HANDLE) {
        //TODO: I'm not sure what to do here?
        if (!iteratee(get_net_handle(get_record_offset(item), START_END, get_handle_type(item)))) {
            return false;
        }
        if (!iteratee(get_net_handle(get_record_offset(item), END_START, get_handle_type(item)))) {
            return false;
        }
    }
    SnarlTreeRecord record(item, &snarl_tree_records);
    for ( size_t type = 1 ; type <= 9 ; type ++ ){
        connectivity_t connectivity = static_cast<connectivity_t>(type);
        if (record.has_connectivity(connectivity)) {
            if (!iteratee(get_net_handle(get_record_offset(item), connectivity, get_handle_type(item)))) {
                return false;
            }
        }
    }
    return true;
}

bool SnarlDistanceIndex::follow_net_edges_impl(const net_handle_t& here, const handlegraph::HandleGraph* graph, bool go_left, const std::function<bool(const net_handle_t&)>& iteratee) const {
#ifdef debug_snarl_traversal
    cerr << "following edges from " << net_handle_as_string(here) << " going " << (go_left ? "rev" : "fd") << endl;
    cerr << "        that is a child of " << net_handle_as_string(get_parent(here)) << endl;
#endif

    SnarlTreeRecord this_record(here, &snarl_tree_records);
    SnarlTreeRecord parent_record (get_parent(here), &snarl_tree_records);

    if (parent_record.get_record_handle_type() == ROOT_HANDLE) {
        //TODO: Double check that this is the right way to handle this
        //If this is a root-level chain or node
        if ((ends_at(here) == END && !go_left) || (ends_at(here) == START && go_left)) {
            //Follow edges leaving the root structure at the end
            if (this_record.is_externally_start_end_connected()) {
                //Follow edge from end to start
                if (!iteratee(get_net_handle(get_record_offset(here), START_END, get_handle_type(here)))) {
                    return false;
                }
            }
            if (this_record.is_externally_end_end_connected()) {
                //Follow edge from end back to the end
                if (!iteratee(get_net_handle(get_record_offset(here), END_START, get_handle_type(here)))) {
                    return false;
                }
            }
        } else {
            //Follow edges leaving the root structure at the end
            if (this_record.is_externally_start_end_connected()) {
                //Follow edge from start to end
                if (!iteratee(get_net_handle(get_record_offset(here), END_START, get_handle_type(here)))) {
                    return false;
                }
            }
            if (this_record.is_externally_end_end_connected()) {
                //Follow edge from the start back to the start
                if (!iteratee(get_net_handle(get_record_offset(here), START_END, get_handle_type(here)))) {
                    return false;
                }
            }

        }
        return true;

    } else if (get_handle_type(here) == CHAIN_HANDLE || get_handle_type(here) == SENTINEL_HANDLE) {
        assert(parent_record.get_record_handle_type() == SNARL_HANDLE ||
               parent_record.get_record_handle_type() == ROOT_HANDLE);//It could also be the root
        //If this is a chain (or a node pretending to be a chain) and it is the child of a snarl
        //Or if it is the sentinel of a snarl, then we walk through edges in the snarl
        //It can either run into another chain (or node) or the boundary node
        //TODO: What about if it is the root?


        //Get the graph handle for the end node of whatever this is, pointing in the right direction
        handle_t graph_handle;
        if (get_handle_type(here) == SENTINEL_HANDLE) {
            if ((get_connectivity(here) == START_END && !go_left) ||
                (get_connectivity(here) == START_START && go_left)) {
                graph_handle = graph->get_handle(parent_record.get_start_id(), parent_record.get_start_orientation());
            } else if ((get_connectivity(here) == END_START && !go_left) ||
                       (get_connectivity(here) == END_END && go_left)) {
                graph_handle = graph->get_handle(parent_record.get_end_id(), !parent_record.get_end_orientation());
            } else {
                //This is facing out, so don't do anything 
                return true;
            }
        } else if (get_handle_type(here) == NODE_HANDLE) {
            graph_handle = graph->get_handle(get_node_id_from_offset(get_record_offset(here)), 
                                ends_at(here) == END ? go_left : !go_left);
        } else {
            //TODO: This might not be the best way to handle orientation because it's a bit inconsistent with tips
            //Might be better to just use go_left and pretend the traversal is forward, but that might be 
            //unintuitive if you have a sentinel of a snarl that you think should be going in or out of the snarl
            //
            //If the traversal explicitly goes out the start, then we assume that it is oriented backwards
            //and go the opposite direction of go_left. Otherwise, assume that it is oriented forwards
            if (ends_at(here) == START) {
                go_left = !go_left;
            } 
            graph_handle = get_handle(get_bound(here, !go_left, false), graph);
        }
#ifdef debug_snarl_traversal
        cerr << "        traversing graph from actual node " << graph->get_id(graph_handle) << (graph->get_is_reverse(graph_handle) ? "rev" : "fd") << endl;
#endif
        graph->follow_edges(graph_handle, false, [&](const handle_t& h) {
#ifdef debug_snarl_traversal
            cerr << "  reached actual node " << graph->get_id(h) << (graph->get_is_reverse(h) ? "rev" : "fd") << endl;
#endif

            if (graph->get_id(h) == parent_record.get_start_id()) {
                //If this is the start boundary node of the parent snarl, then do this on the sentinel
                assert(graph->get_is_reverse(h) == !parent_record.get_start_orientation());
#ifdef debug_snarl_traversal
                cerr << "    -> start of parent " << endl;
#endif
                return iteratee(get_bound(get_parent(here), false, false));
            } else if (graph->get_id(h) == parent_record.get_end_id()) {
                assert(graph->get_is_reverse(h) == parent_record.get_end_orientation());
#ifdef debug_snarl_traversal
                cerr << "    -> end of parent " << endl;
#endif
                return iteratee(get_bound(get_parent(here), true, false));
            } else {
                //It is either another chain or a node, but the node needs to pretend to be a chain

                //Get the node record of the next node
                NodeRecord next_node_record(get_offset_from_node_id(graph->get_id(h)), &snarl_tree_records);

                if (next_node_record.get_parent_record_offset() == parent_record.record_offset) {
                    //If the next node's parent is also the current node's parent, then it is a node
                    //make a net_handle_t of a node pretending to be a chain
                    net_handle_t next_net = get_net_handle(next_node_record.record_offset, 
                                                           graph->get_is_reverse(h) ? END_START : START_END, 
                                                           CHAIN_HANDLE);
#ifdef debug_snarl_traversal
                cerr << "    -> actual child node " << net_handle_as_string(next_net) << endl;
#endif
                   return iteratee(next_net);
                } else {
                    //next_node_record is also the start of a chain

                    bool rev = graph->get_id(h) == next_node_record.get_is_reversed_in_parent() ? false : true;
                    net_handle_t next_net = get_net_handle(next_node_record.get_parent_record_offset(), 
                                                           rev ? END_START : START_END, 
                                                           CHAIN_HANDLE);
#ifdef debug_snarl_traversal
                    assert(SnarlTreeRecord(next_node_record.get_parent_record_offset(), &snarl_tree_records).get_record_handle_type() == CHAIN_HANDLE);
                    assert(get_node_id_from_offset(next_node_record.record_offset) 
                        == SnarlTreeRecord(next_node_record.get_parent_record_offset(), &snarl_tree_records).get_start_id() || 
                        get_node_id_from_offset(next_node_record.record_offset) 
                        == SnarlTreeRecord(next_node_record.get_parent_record_offset(), &snarl_tree_records).get_end_id());
                cerr << "    -> child chain " << net_handle_as_string(next_net) << endl;
#endif
                   return iteratee(next_net);
                }
            }
            return false;
        });

        
    } else if (get_handle_type(here) == SNARL_HANDLE || get_handle_type(here) == NODE_HANDLE) {
        assert(parent_record.get_record_handle_type() == CHAIN_HANDLE);
        //If this is a snarl or node, then it is the component of a (possibly pretend) chain
        ChainRecord parent_chain(this_record.get_parent_record_offset(), &snarl_tree_records);
        if (ends_at(here) == START) {
            go_left = !go_left;
        }
        if (SnarlTreeRecord(get_record_offset(here), &snarl_tree_records).get_is_reversed_in_parent()) {
            go_left = !go_left;
        }
        net_handle_t next_net = parent_chain.get_next_child(here, go_left);
        if (next_net == here) {
            //If this is the end of the chain
            return true;
        }
        return iteratee(next_net);
        
    }
    return true;
}


net_handle_t SnarlDistanceIndex::get_parent_traversal(const net_handle_t& traversal_start, const net_handle_t& traversal_end) const {
    
    net_handle_record_t start_handle_type = get_handle_type(traversal_start);
    net_handle_record_t end_handle_type = get_handle_type(traversal_end);

    if (start_handle_type == SENTINEL_HANDLE) {
        //these are the sentinels of a snarl
        //TODO: Make sure this is handling possible orientations properly
        assert(end_handle_type == SENTINEL_HANDLE);
        endpoint_t start_endpoint = get_start_endpoint(get_connectivity(traversal_start));
        endpoint_t end_endpoint = get_start_endpoint(get_connectivity(traversal_end));
        return get_net_handle(get_record_offset(get_parent(traversal_start)), 
                              endpoints_to_connectivity(start_endpoint, end_endpoint),
                              SNARL_HANDLE);
    } else {
        //These are the endpoints or tips in a chain
        SnarlTreeRecord start_record = get_snarl_tree_record(traversal_start);
        SnarlTreeRecord end_record = get_snarl_tree_record(traversal_end);
        if (start_record.get_parent_record_offset() != end_record.get_parent_record_offset()) {
            throw runtime_error("error: Looking for parent traversal of two non-siblings");
        }
        SnarlTreeRecord parent_record (start_record.get_parent_record_offset(), &snarl_tree_records);
        assert(parent_record.get_record_handle_type() == CHAIN_HANDLE);

        //Figure out what the start and end of the traversal are
        endpoint_t start_endpoint;
        if (start_handle_type == NODE_HANDLE && 
            get_node_id_from_offset(get_record_offset(traversal_start)) == parent_record.get_start_id() &&
            (get_start_endpoint(traversal_start) == START && !parent_record.get_start_orientation() ||
             get_start_endpoint(traversal_start) == END && parent_record.get_start_orientation()) ){
            //If traversal_start is a node and is also the start node oriented into the parent
            start_endpoint = START;
    
        } else if (start_handle_type == NODE_HANDLE && 
            get_node_id_from_offset(get_record_offset(traversal_start)) == parent_record.get_end_id() &&
            (get_start_endpoint(traversal_start) == START && parent_record.get_end_orientation() ||
             get_start_endpoint(traversal_start) == END && !parent_record.get_end_orientation()) ){
            //If traversal_start is a node and also the end node and oriented going into the parent
            start_endpoint = END;
    
        } else if (start_handle_type == SNARL_HANDLE) {
            start_endpoint = TIP;
        } else {
            throw runtime_error("error: trying to get an invalid traversal of a chain");
        }
    
        endpoint_t end_endpoint;
        if (end_handle_type == NODE_HANDLE && 
            get_node_id_from_offset(get_record_offset(traversal_end)) == parent_record.get_start_id() &&
            (get_start_endpoint(traversal_end) == START && parent_record.get_start_orientation() ||
             get_start_endpoint(traversal_end) == END && !parent_record.get_start_orientation())){
            //If traversal_end is a node and also the start node oriented out of the parent
            end_endpoint = START;
        } else if (end_handle_type == NODE_HANDLE && 
            get_node_id_from_offset(get_record_offset(traversal_end)) == parent_record.get_end_id() &&
            (get_start_endpoint(traversal_end) == START && !parent_record.get_end_orientation() ||
             get_start_endpoint(traversal_end) == END && parent_record.get_end_orientation()) ){
            //If traversal_end is a node and also the end node oriented out of the parent
            end_endpoint = END;
        } else if (end_handle_type == SNARL_HANDLE) {
            end_endpoint = TIP;
        } else {
            throw runtime_error("error: trying to get an invalid traversal of a chain");
        }

        if (!parent_record.has_connectivity(start_endpoint, end_endpoint)) {
            throw runtime_error("error: Trying to get parent traversal that is not connected");
        }

        return get_net_handle(parent_record.record_offset, 
                              endpoints_to_connectivity(start_endpoint, end_endpoint), 
                              CHAIN_HANDLE);
    }


}

void SnarlDistanceIndex::load(string& filename) {
    snarl_tree_records.dissociate();
    snarl_tree_records.reset();
    std::ifstream in (filename);
    snarl_tree_records.load(in);
}
void SnarlDistanceIndex::load(int fd) {
    snarl_tree_records.dissociate();
    snarl_tree_records.reset();
    snarl_tree_records.load(fd, tag);
}
void SnarlDistanceIndex::load(std::istream& in) {
    snarl_tree_records.dissociate();
    snarl_tree_records.reset();
    snarl_tree_records.load(in, tag);
}
void SnarlDistanceIndex::save(string& filename) const {
    std::ofstream out(filename);
    snarl_tree_records.save(out);
}
void SnarlDistanceIndex::save(int fd) {
    snarl_tree_records.save(fd);
}
void SnarlDistanceIndex::save(std::ostream& out) const {
    snarl_tree_records.save(out);
}


int64_t SnarlDistanceIndex::distance_in_parent(const net_handle_t& parent, 
        const net_handle_t& child1, const net_handle_t& child2, const HandleGraph* graph) const {

    assert(canonical(parent) == canonical(get_parent(child1)));
    assert(canonical(parent) == canonical(get_parent(child2)));

    if (is_root(parent)) {
        //If the parent is the root, then the children must be in the same root snarl for them to be
        //connected
        size_t parent_record_offset1 = SnarlTreeRecord(child1, &snarl_tree_records).get_parent_record_offset();
        size_t parent_record_offset2 = SnarlTreeRecord(child2, &snarl_tree_records).get_parent_record_offset();
        if (parent_record_offset1 != parent_record_offset2) {
            //If the children are in different connected components
//#ifdef debug_distances
//            cerr << "            => " << std::numeric_limits<int64_t>::max() << endl;
//#endif
            return std::numeric_limits<int64_t>::max();
        } else if (SnarlTreeRecord(parent_record_offset1, &snarl_tree_records).get_record_type() 
                    != DISTANCED_ROOT_SNARL){
            //If they are in the same connected component, but it is not a root snarl
//#ifdef debug_distances
//            cerr << "            => " << std::numeric_limits<int64_t>::max() << endl;
//#endif
            return std::numeric_limits<int64_t>::max();
        } else {
            //They are in the same root snarl, so find the distance between them
            //
            SnarlRecord snarl_record(parent_record_offset1, &snarl_tree_records);
            SnarlTreeRecord child_record1 (child1, &snarl_tree_records);
            SnarlTreeRecord child_record2 (child2, &snarl_tree_records);
            size_t rank1 = child_record1.get_rank_in_parent();
            size_t rank2 = child_record2.get_rank_in_parent();

//#ifdef debug_distances
//            cerr << "            => " << snarl_record.get_distance(rank1, ends_at(child1) == END, rank2, ends_at(child2) == END);
//#endif
            //TODO: Double check orientations
            return snarl_record.get_distance(rank1, ends_at(child1) == END, rank2, ends_at(child2) == END);
        }


    } else if (is_chain(parent)) {
        ChainRecord chain_record(get_parent(child1), &snarl_tree_records);
        assert(is_node(child1) || is_snarl(child1));
        assert(is_node(child2) || is_snarl(child2));

        if (is_trivial_chain(parent)) {
            return std::numeric_limits<int64_t>::max();
        }

        if (is_node(child1) && is_node(child2)) {
            //The distance between two nodes
            NodeRecord child_record1 (child1, &snarl_tree_records);
            NodeRecord child_record2 (child2, &snarl_tree_records);

            bool go_left1 = child_record1.get_is_reversed_in_parent();
            if (ends_at(child1) == START){
                go_left1 = !go_left1;
            }
            bool go_left2 = child_record2.get_is_reversed_in_parent();
            if (ends_at(child2) == START) {
                go_left2 = !go_left2;
            }
//#ifdef debug_distances
//            cerr << "Finding distances between two nodes with ranks " << child_record1.get_rank_in_parent() << " " << go_left1 << " " << child_record1.get_node_length() << " and " << endl << child_record2.get_rank_in_parent() << " " << go_left2 << " " << child_record2.get_node_length() << endl;
//            cerr << "            => " << chain_record.get_distance(
//                make_tuple(child_record1.get_rank_in_parent(), go_left1, child_record1.get_node_length()), 
//                make_tuple(child_record2.get_rank_in_parent(), go_left2, child_record2.get_node_length())) << endl;
//#endif

            return chain_record.get_distance(
                make_tuple(child_record1.get_rank_in_parent(), go_left1, child_record1.get_node_length()), 
                make_tuple(child_record2.get_rank_in_parent(), go_left2, child_record2.get_node_length()));
        } else if (is_node(child1) != is_node(child2)) {
            //If one of them is a node and one is a snarl
            //It doesn't matter which is which, since we're looking at the distance pointing towards each other
            NodeRecord node_record (is_node(child1) ? child1 : child2, &snarl_tree_records);
            bool go_left_node = node_record.get_is_reversed_in_parent();
            if (ends_at(is_node(child1) ? child1 : child2) == START){
                go_left_node = !go_left_node;
            }

            SnarlRecord snarl_record (is_snarl(child1) ? child1 : child2, &snarl_tree_records);
            id_t snarl_node = ends_at(is_snarl(child1) ? child1 : child2) == START 
                             ? snarl_record.get_start_id() : snarl_record.get_end_id();
            bool go_left_snarl = ends_at(is_snarl(child1) ? child1 : child2) == START ;
                                
            if (node_record.get_node_id() == snarl_node && go_left_node != go_left_snarl) {
                //If the node is the boundary of the snarl facing in
//#ifdef debug_distances
//                cerr << "        => 0" << endl;
//#endif
                return 0;
            } else {
                //Otherwise, find the actual distance from the node to the correct boundary of the snarl,
                //and add the length of the boundary of the snarl, since it is not included in the chain distance
                NodeRecord snarl_node_record (get_offset_from_node_id(snarl_node), &snarl_tree_records);
//#ifdef debug_distances
//            cerr << "            => " << sum({chain_record.get_distance(
//                               make_tuple(node_record.get_rank_in_parent(), go_left_node, node_record.get_node_length()), 
//                               make_tuple(snarl_node_record.get_rank_in_parent(), go_left_snarl, snarl_node_record.get_node_length())) 
//                           , snarl_node_record.get_node_length()}) << endl;
//#endif
                return sum({chain_record.get_distance(
                               make_tuple(node_record.get_rank_in_parent(), go_left_node, node_record.get_node_length()), 
                               make_tuple(snarl_node_record.get_rank_in_parent(), go_left_snarl, snarl_node_record.get_node_length())) 
                           , snarl_node_record.get_node_length()});
            }
        } else {
            assert(is_snarl(child1));
            assert(is_snarl(child2));
            SnarlRecord child_record1 (child1, &snarl_tree_records);
            SnarlRecord child_record2 (child2, &snarl_tree_records);

            id_t node_id1 = ends_at(child1) == START ? child_record1.get_start_id() : child_record1.get_end_id();
            id_t node_id2 = ends_at(child2) == START ? child_record2.get_start_id() : child_record2.get_end_id();
            NodeRecord node_record1 (get_offset_from_node_id(node_id1), &snarl_tree_records);
            NodeRecord node_record2 (get_offset_from_node_id(node_id2), &snarl_tree_records); 

            bool go_left1 = ends_at(child1) == START;
            bool go_left2 = ends_at(child2) == START;

            if (node_id1 == node_id2 && go_left1 != go_left2) {
                //If the snarls are adjacent (and not the same snarl)
//#ifdef debug_distances
//            cerr << "            => " << node_record1.get_node_length() << endl;
//#endif

                return node_record1.get_node_length();
            } else {
//#ifdef debug_distances
//            cerr << "            => " << sum({chain_record.get_distance(
//                            make_tuple(node_record1.get_rank_in_parent(), go_left1, node_record1.get_node_length()), 
//                            make_tuple(node_record2.get_rank_in_parent(), go_left2, node_record2.get_node_length())) 
//                    , node_record1.get_node_length() , node_record2.get_node_length()}) << endl;
//#endif
                return sum({chain_record.get_distance(
                            make_tuple(node_record1.get_rank_in_parent(), go_left1, node_record1.get_node_length()), 
                            make_tuple(node_record2.get_rank_in_parent(), go_left2, node_record2.get_node_length())) 
                    , node_record1.get_node_length() , node_record2.get_node_length()});
            }
        }

    } else if (is_snarl(parent)) {
        SnarlRecord snarl_record(get_parent(child1), &snarl_tree_records);
        SnarlTreeRecord child_record1 (child1, &snarl_tree_records);
        SnarlTreeRecord child_record2 (child2, &snarl_tree_records);
        size_t rank1, rank2; bool rev1, rev2;
        if (is_sentinel(child1)) {
            rank1 = starts_at(child1) == START ? 0 : 1;
            rev1 = false;
        } else {
            rank1 = child_record1.get_rank_in_parent();
            rev1 = ends_at(child1) == END;
        }
        if (is_sentinel(child2)) {
            rank2 = starts_at(child2) == START ? 0 : 1;
            rev2 = false;
        } else {
            rank2 = child_record2.get_rank_in_parent();
            rev2 = ends_at(child2) == END;
        }
        if ((is_sentinel(child1) && starts_at(child1) == ends_at(child1)) ||
            (is_sentinel(child2) && starts_at(child2) == ends_at(child2)) ) {
            //If this is a sentinel pointing out of the snarl
//#ifdef debug_distances
//            cerr << "            => " << std::numeric_limits<int64_t>::max() << endl;
//#endif
            return std::numeric_limits<int64_t>::max();
        }

//#ifdef debug_distances
//            cerr << "             between ranks " << rank1 << " " << rev1 << " " << rank2 << " " << rev2 << endl;
//            cerr << "            => " << snarl_record.get_distance(rank1, rev1, rank2, rev2) << endl;
//#endif
//
        if (snarl_record.get_record_type() == OVERSIZED_SNARL 
            && !(rank1 == 0 || rank1 == 1 || rank2 == 0 || rank2 == 1)) {
            //If this is an oversized snarl and we're looking for internal distances, then we didn't store the
            //distance and we have to find it using dijkstra's algorithm
            if (graph == nullptr) {
                cerr << "warning: trying to find the distance in an oversized snarl without a graph. Returning inf" << endl;
                return std::numeric_limits<int64_t>::max();
            }
            handle_t handle1 = is_node(child1) ? get_handle(child1, graph) : get_handle(get_bound(child1, ends_at(child1) == END, false), graph); 
            handle_t handle2 = is_node(child2) ? get_handle(child2, graph) : get_handle(get_bound(child2, ends_at(child2) == END, false), graph);
            handle2 = graph->flip(handle2);

            int64_t distance = std::numeric_limits<int64_t>::max();
            handlegraph::algorithms::dijkstra(graph, handle1, [&](const handle_t& reached, size_t dist) {
                if (reached == handle2) {
                    //TODO: Also give up if the distance is too great
                    distance = dist;
                    return false;
                }
                return true;
            }, false);
            return distance;

            
        } else {
           return snarl_record.get_distance(rank1, rev1, rank2, rev2);
        }
    } else {
        throw runtime_error("error: Trying to find distance in the wrong type of handle");
    }
}

pair<net_handle_t, bool> SnarlDistanceIndex::lowest_common_ancestor(const net_handle_t& net1, const net_handle_t& net2) const {
    net_handle_t parent1 = net1;
    net_handle_t parent2 = net2;

    std::unordered_set<net_handle_t> net1_ancestors;
    while (!is_root(parent1)){
        net1_ancestors.insert(canonical(parent1));
        parent1 = canonical(get_parent(parent1));
    }

    while (net1_ancestors.count(canonical(parent2)) == 0 && !is_root(parent2)){
        //Go up until the parent2 matches something in the ancestors of net1
        //This loop will end because everything is in the same root eventually
        parent2 = canonical(get_parent(parent2));
    }

    bool is_connected = true;
    if (is_root(parent2) && is_root(parent1)){
        size_t parent_record_offset1 = SnarlTreeRecord(parent1, &snarl_tree_records).get_parent_record_offset();
        size_t parent_record_offset2 = SnarlTreeRecord(parent2, &snarl_tree_records).get_parent_record_offset();
        if (parent_record_offset1 != parent_record_offset2) {
            is_connected = false;
        }
    }
    return make_pair(parent2, is_connected);
}

int64_t SnarlDistanceIndex::minimum_distance(handle_t handle1, size_t offset1, handle_t handle2, size_t offset2, bool unoriented_distance, const HandleGraph* graph) const {


#ifdef debug_distances
        cerr << endl;
        cerr << "Find the minimum distance between " << handle1 << " and " << handle2 << endl;
#endif


    /*Helper function to walk up the snarl tree
     * Given a net handle, its parent,  and the distances to the start and end of the handle, 
     * update the distances to reach the ends of the parent and update the handle and its parent
     * If the parent is a chain, then the new distances include the boundary nodes of the chain.
     * If it is a snarl, it does not*/
    //TODO: This should really be an actual function and it doesn't consider the lengths of the 
    //boundary nodes. I think chains might need to know the lengths of their boundary nodes,
    //or it could find it from the snarl. Really, since the snarl is storing it anyway, I should
    //probaby rearrange things so that either the snarl or the chain can access boundary node lengths
    auto update_distances = [&](net_handle_t& net, net_handle_t& parent, int64_t& dist_start, int64_t& dist_end) {
#ifdef debug_distances
        cerr << "     Updating distance from node " << net_handle_as_string(net) << " at parent " << net_handle_as_string(parent) << endl;
#endif

        if (is_trivial_chain(parent)) {
            //Don't update distances for the trivial chain
            return;
        }


        net_handle_t start_bound = get_bound(parent, false, true);
        net_handle_t end_bound = get_bound(parent, true, true);

        //The lengths of the start and end nodes of net
        //This is only needed if net is a snarl, since the boundary nodes are not technically part of the snarl
        int64_t start_length = is_chain(parent) ? node_length(start_bound) : 0;
        int64_t end_length = is_chain(parent) ? node_length(end_bound) : 0;

        //Get the distances from the bounds of the parent to the node we're looking at
        int64_t distance_start_start = start_bound == net ? -start_length : distance_in_parent(parent, start_bound, flip(net), graph);
        int64_t distance_start_end = start_bound == flip(net) ? -start_length : distance_in_parent(parent, start_bound, net, graph);
        int64_t distance_end_start = end_bound == net ? -end_length : distance_in_parent(parent, end_bound, flip(net), graph);
        int64_t distance_end_end = end_bound == flip(net) ? -end_length : distance_in_parent(parent, end_bound, net, graph);

        int64_t distance_start = dist_start;
        int64_t distance_end = dist_end; 


        dist_start = sum({std::min( sum({distance_start_start, distance_start}), 
                                    sum({distance_start_end , distance_end})) , 
                           start_length});
        dist_end = sum({std::min(sum({distance_end_start , distance_start}), 
                            sum({distance_end_end , distance_end})) ,
                       end_length});
#ifdef debug_distances
        cerr << "        ...new distances to start and end: " << dist_start << " " << dist_end << endl;
#endif
        return;
    };

    /*
     * Get net handles for the two nodes and the distances from each position to the ends of the handles
     * TODO: net2 is pointing in the opposite direction of the position. The final 
     * distance will be between the two nets pointing towards each other
     */
    net_handle_t net1 = get_net_handle(get_offset_from_node_id(graph->get_id(handle1)), START_END);
    net_handle_t net2 = get_net_handle(get_offset_from_node_id(graph->get_id(handle2)), START_END);
    pair<net_handle_t, bool> lowest_ancestor = lowest_common_ancestor(net1, net2);
    if (!lowest_ancestor.second) {
        //If these are not in the same connected component
#ifdef debug_distances
        cerr << "These are in different connected components" << endl;
#endif
        return std::numeric_limits<int64_t>::max();
    }

    //The lowest common ancestor of the two positions
    net_handle_t common_ancestor = std::move(lowest_ancestor.first);

#ifdef debug_distances
        cerr << "Found the lowest common ancestor " << net_handle_as_string(common_ancestor) << endl;
#endif
    //These are the distances to the ends of the node, including the position
    int64_t distance_to_start1 = graph->get_is_reverse(handle1) ? node_length(net1) - offset1 : offset1 + 1;
    int64_t distance_to_end1 = graph->get_is_reverse(handle1) ? offset1 + 1 : node_length(net1) - offset1;
    int64_t distance_to_start2 = graph->get_is_reverse(handle2) ? node_length(net2) - offset2 : offset2 + 1;
    int64_t distance_to_end2 = graph->get_is_reverse(handle2) ? offset2 + 1 : node_length(net2) - offset2;

    if (!unoriented_distance) {
        //If we care about the oriented distance, one of the distances will be infinite
        if (graph->get_is_reverse(handle1)) {
            distance_to_end1 = std::numeric_limits<int64_t>::max();
        } else {
            distance_to_start1 = std::numeric_limits<int64_t>::max();
        }
        if (graph->get_is_reverse(handle2)) {
            distance_to_start2 = std::numeric_limits<int64_t>::max();
        } else {
            distance_to_end2 = std::numeric_limits<int64_t>::max();
        }
    }

#ifdef debug_distances
        cerr << "Starting with distances " << distance_to_start1 << " " << distance_to_end1 << " and " << distance_to_start2 << " " << distance_to_end2 << endl;
#endif

    int64_t minimum_distance = std::numeric_limits<int64_t>::max();


    /*
     * Walk up the snarl tree until net1 and net2 are children of the lowest common ancestor
     * Keep track of the distances to the ends of the net handles as we go
     */
 
    if (canonical(net1) == canonical(net2)){
        if (sum({distance_to_end1 , distance_to_start2}) > node_length(net1) && 
            sum({distance_to_end1 , distance_to_start2}) != std::numeric_limits<int64_t>::max()) {
            //If the positions are on the same node and are pointing towards each other, then
            //check the distance between them in the node
            minimum_distance = minus(sum({distance_to_end1 , distance_to_start2}), node_length(net1));
        }
        if (sum({distance_to_start1 , distance_to_end2}) > node_length(net1) && 
            sum({distance_to_start1 , distance_to_end2}) != std::numeric_limits<int64_t>::max()) {
            minimum_distance = std::min(minus(sum({distance_to_start1 , distance_to_end2}), node_length(net1)), minimum_distance);
        }
        common_ancestor = get_parent(net1);
    } else {

        //Get the distance from position 1 up to the ends of a child of the common ancestor
#ifdef debug_distances
        cerr << "Reaching the children of the lowest common ancestor for first position..." << endl;
#endif   
        while (canonical(get_parent(net1)) != canonical(common_ancestor)) {
            net_handle_t parent = get_parent(net1);
            update_distances(net1, parent, distance_to_start1, distance_to_end1);
            net1 = parent;
        }
#ifdef debug_distances
        cerr << "Reached node " << net_handle_as_string(net1) << " for position 1" << endl;
        cerr << "   with distances to ends " << distance_to_start1 << " and " << distance_to_end1 << endl;
        cerr << "Reaching the children of the lowest common ancestor for position 2..." << endl;
#endif   
        //And the same for position 2
        while (canonical(get_parent(net2)) != canonical(common_ancestor)) {
            net_handle_t parent = get_parent(net2);
            update_distances(net2, parent, distance_to_start2, distance_to_end2);
            net2 = parent;
        }
#ifdef debug_distances
        cerr << "Reached node " << net_handle_as_string(net2) << " for position 2" << endl;
        cerr << "   with distances to ends " << distance_to_start2 << " and " << distance_to_end2 << endl;
#endif
    }
    //TODO: I'm taking this out because it should really be start-end connected, but this
    //won't do this if that traversal isn't possible. Really it should just be setting the 
    //connectivity instead
    //net1 = canonical(net1);
    //net2 = canonical(net2);


    /* 
     * common_ancestor is now the lowest common ancestor of both net handles, and 
     * net1 and net2 are both children of common_ancestor
     * Walk up to the root and check for distances between the positions within each
     * ancestor
     */
    while (!is_root(net1)){
        //TODO: Actually checking distance in a chain is between the nodes, not the snarl
        //and neither include the lengths of the nodes
#ifdef debug_distances
            cerr << "At common ancestor " << net_handle_as_string(common_ancestor) <<  endl;
            cerr << "  with distances for child 1 (" << net_handle_as_string(net1) << "): " << distance_to_start1 << " "  << distance_to_end1 << endl;
            cerr << "                     child 2 (" << net_handle_as_string(net2) << "): " << distance_to_start2 << " " <<  distance_to_end2 << endl;
#endif

        //Find the minimum distance between the two children (net1 and net2)
        int64_t distance_start_start = distance_in_parent(common_ancestor, flip(net1), flip(net2), graph);
        int64_t distance_start_end = distance_in_parent(common_ancestor, flip(net1), net2, graph);
        int64_t distance_end_start = distance_in_parent(common_ancestor, net1, flip(net2), graph);
        int64_t distance_end_end = distance_in_parent(common_ancestor, net1, net2, graph);

        //And add those to the distances we've found to get the minimum distance between the positions
        minimum_distance = std::min(minimum_distance, 
                           std::min(sum({distance_start_start , distance_to_start1 , distance_to_start2}),
                           std::min(sum({distance_start_end , distance_to_start1 , distance_to_end2}),
                           std::min(sum({distance_end_start , distance_to_end1 , distance_to_start2}),
                                    sum({distance_end_end , distance_to_end1 , distance_to_end2})))));

#ifdef debug_distances
            cerr << "    Found distances between nodes: " << distance_start_start << " " << distance_start_end << " " << distance_end_start << " " << distance_end_end << endl;
            cerr << "  best distance is " << minimum_distance << endl;
#endif
        if (!is_root(common_ancestor)) {
            //Update the distances to reach the ends of the common ancestor
            update_distances(net1, common_ancestor, distance_to_start1, distance_to_end1);
            update_distances(net2, common_ancestor, distance_to_start2, distance_to_end2);

            //Update which net handles we're looking at
            net1 = common_ancestor;
            net2 = common_ancestor;
            common_ancestor = get_parent(common_ancestor);
        } else {
            //If net1 and net2 are the same, check if they have external connectivity in the root
            if ( canonical(net1) == canonical(net2) ) {
#ifdef debug_distances
                cerr << "    Checking external connectivity" << endl;
#endif
                if (has_external_connectivity(net1, START, START)) {
                    minimum_distance = std::min(minimum_distance, sum({distance_to_start1, distance_to_start2}));
                }
                if (has_external_connectivity(net1, END, END)) {
                    minimum_distance = std::min(minimum_distance, sum({distance_to_end1, distance_to_end2}));
                }
                if (has_external_connectivity(net1, START, END)) {
                    minimum_distance = std::min(minimum_distance, 
                                       std::min(sum({distance_to_start1, distance_to_end2}),
                                                sum({distance_to_end1, distance_to_start2})));
                }

                if (has_external_connectivity(net1, START, START) && has_external_connectivity(net1, END, END)) {
                    minimum_distance = std::min(minimum_distance, 
                                       sum({std::min(sum({distance_to_start1, distance_to_end2}),
                                                sum({distance_to_end1, distance_to_start2})), 
                                           minimum_length(net1)}));
                }
            }
            //Just update this one to break out of the loop
            net1 = common_ancestor;

        }

#ifdef debug_distances
            cerr << "  new common ancestor " << net_handle_as_string(common_ancestor) <<  endl;
            cerr << "  new distances are " << distance_to_start1 << " "  << distance_to_end1 << 
                    " " << distance_to_start2 << " " <<  distance_to_end2 << endl;
#endif
    }

    //minimum distance currently includes both positions
    return minimum_distance == std::numeric_limits<int64_t>::max() ? std::numeric_limits<int64_t>::max() : minimum_distance-1;



}

int64_t SnarlDistanceIndex::node_length(const net_handle_t& net) const {
    assert(is_node(net) || is_sentinel(net));
    if (is_node(net)) {
        return NodeRecord(net, &snarl_tree_records).get_node_length();
    } else if (is_sentinel(net)) {
        //If this is the sentinel of a snarl, return the node length
        //TODO: It might be better to store the node lengths in the chains
        //This would use more memory, but it would mean a faster lookup since you wouldn't have
        //to go back down to the level of nodes. But at some point you would probably have 
        //encountered those nodes anyway, so it might not be that expensive to do it this way
        SnarlRecord snarl_record(net, &snarl_tree_records);
        NodeRecord node_record;
        if (get_start_endpoint(net) == START) {
            node_record = NodeRecord(get_offset_from_node_id(snarl_record.get_start_id()), &snarl_tree_records);
        } else {
            node_record = NodeRecord(get_offset_from_node_id(snarl_record.get_end_id()), &snarl_tree_records);
        }
        return node_record.get_node_length();
    } else {
        throw runtime_error("error: Looking for the node length of a non-node net_handle_t");
    }

}


//TODO: This is kind of redundant with node_length 
int64_t SnarlDistanceIndex::minimum_length(const net_handle_t& net) const {
        return SnarlTreeRecord(net, &snarl_tree_records).get_min_length();
}
int64_t SnarlDistanceIndex::node_id(const net_handle_t& net) const {
    assert(is_node(net) || is_sentinel(net));
    if (is_node(net)) {
        return NodeRecord(net, &snarl_tree_records).get_node_id();
    } else if (is_sentinel(net)) {
        SnarlRecord snarl_record(net, &snarl_tree_records);
        NodeRecord node_record;
        if (get_start_endpoint(net) == START) {
            return snarl_record.get_start_id();
        } else {
            return snarl_record.get_end_id();
        }
    } else {
        throw runtime_error("error: Looking for the node length of a non-node net_handle_t");
    }

}

bool SnarlDistanceIndex::has_connectivity(const net_handle_t& net, endpoint_t start, endpoint_t end) const {
    SnarlTreeRecord record(net, &snarl_tree_records);
    return record.has_connectivity(start, end);
}
bool SnarlDistanceIndex::has_external_connectivity(const net_handle_t& net, endpoint_t start, endpoint_t end) const {
    SnarlTreeRecord record(net, &snarl_tree_records);
    return record.has_external_connectivity(start, end);
}
bool SnarlDistanceIndex::SnarlTreeRecord::has_connectivity(connectivity_t connectivity) const {
    if (connectivity == START_START) {
        return is_start_start_connected();
    } else if (connectivity == START_END || connectivity == END_START) {
        return is_start_end_connected();
    } else if (connectivity == START_TIP || connectivity == TIP_START) {
        return is_start_tip_connected();
    } else if (connectivity == END_END) {
        return is_end_end_connected();
    } else if (connectivity == END_TIP || connectivity == TIP_END) {
        return is_end_tip_connected();
    } else if (connectivity == TIP_TIP) {
        return is_tip_tip_connected();
    } else {
        throw runtime_error("error: Invalid connectivity");
    }
}
size_t SnarlDistanceIndex::SnarlTreeRecord::get_min_length() const {
    record_t type = get_record_type();
    size_t val;
    if (type == DISTANCED_NODE) {
        val =  (*records)->at(record_offset + NODE_LENGTH_OFFSET);
    } else if (type == DISTANCED_SNARL || type == OVERSIZED_SNARL)  {
        val =  (*records)->at(record_offset + SNARL_MIN_LENGTH_OFFSET);
    } else if (type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        val = (*records)->at(record_offset + CHAIN_MIN_LENGTH_OFFSET);
    } else if (type == NODE || type == SNARL || type == CHAIN) {
        throw runtime_error("error: trying to access get distance in a distanceless index");
    } else if (type == ROOT || type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL) {
        throw runtime_error("error: trying to find the length of the root");
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
    return val == 0 ? std::numeric_limits<size_t>::max() : val - 1;
};

size_t SnarlDistanceIndex::SnarlTreeRecord::get_max_length() const {
    record_t type = get_record_type();
    size_t val;
    if (type == DISTANCED_NODE) {
        val = (*records)->at(record_offset + NODE_LENGTH_OFFSET);
    } else if (type == DISTANCED_SNARL || type == OVERSIZED_SNARL)  {
        val = (*records)->at(record_offset + SNARL_MAX_LENGTH_OFFSET);
    } else if (type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        val = (*records)->at(record_offset + CHAIN_MAX_LENGTH_OFFSET);
    } else if (type == NODE || type == SNARL || type == CHAIN) {
        throw runtime_error("error: trying to access get distance in a distanceless index");
    }  else if (type == ROOT || type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL) {
        throw runtime_error("error: trying to find the length of the root");
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }

    return val == 0 ? std::numeric_limits<size_t>::max() : val - 1;
};

size_t SnarlDistanceIndex::SnarlTreeRecord::get_rank_in_parent() const {
    record_t type = get_record_type();
    if (type == NODE || type == DISTANCED_NODE) {
        return (*records)->at(record_offset + NODE_RANK_OFFSET) >> 1;
    } else if (type == SNARL || type == DISTANCED_SNARL || type == OVERSIZED_SNARL
            || type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL)  {
        return (*records)->at(record_offset + SNARL_RANK_OFFSET) >> 1;
    } else if (type == CHAIN || type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        return (*records)->at(record_offset + CHAIN_RANK_OFFSET) >> 1;
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
};
bool SnarlDistanceIndex::SnarlTreeRecord::get_is_reversed_in_parent() const {
    record_t type = get_record_type();
    if (type == NODE || type == DISTANCED_NODE) {
        return (*records)->at(record_offset + NODE_RANK_OFFSET) & 1;
    } else if (type == SNARL || type == DISTANCED_SNARL || type == OVERSIZED_SNARL
            || type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL)  {
        return (*records)->at(record_offset + SNARL_RANK_OFFSET) & 1;
    } else if (type == CHAIN || type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        return (*records)->at(record_offset + CHAIN_RANK_OFFSET) & 1;
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
};

id_t SnarlDistanceIndex::SnarlTreeRecord::get_start_id() const {
    record_t type = get_record_type();
    if (type == ROOT) {
        //TODO: Also not totally sure what this should do
        throw runtime_error("error: trying to get the start node of the root");
    } else if (type == NODE || type == DISTANCED_NODE) {
        //TODO: I'm not sure if I want to allow this
        //cerr << "warning: Looking for the start of a node" << endl;
        return get_id_from_offset(record_offset);
    } else if (type == SNARL || type == DISTANCED_SNARL || type == OVERSIZED_SNARL)  {
        return ((*records)->at(record_offset + SNARL_START_NODE_OFFSET)) >> 1;
    } else if (type == CHAIN || type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        return ((*records)->at(record_offset + CHAIN_START_NODE_OFFSET)) >> 1;
    } else if (type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL) {
        throw runtime_error("error: trying to find the start node of a root snarl");
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
}
bool SnarlDistanceIndex::SnarlTreeRecord::get_start_orientation() const {
    record_t type = get_record_type();
    if (type == ROOT) {
        //TODO: Also not totally sure what this should do
        throw runtime_error("error: trying to get the start node of the root");
    } else if (type == NODE || type == DISTANCED_NODE) {
        //TODO: I'm not sure if I want to allow this
        //cerr << "warning: Looking for the start of a node" << endl;
        return false;
    } else if (type == SNARL || type == DISTANCED_SNARL || type == OVERSIZED_SNARL)  {
        return ((*records)->at(record_offset + SNARL_START_NODE_OFFSET) & 1);
    } else if (type == CHAIN || type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        return ((*records)->at(record_offset + CHAIN_START_NODE_OFFSET)) & 1;
    } else if (type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL) {
        throw runtime_error("error: trying to find the start node of a root snarl");
    }else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
}
id_t SnarlDistanceIndex::SnarlTreeRecord::get_end_id() const {
    record_t type = get_record_type();
    if (type == ROOT) {
        //TODO: Also not totally sure what this should do
        throw runtime_error("error: trying to get the end node of the root");
    } else if (type == NODE || type == DISTANCED_NODE) {
        //TODO: I'm not sure if I want to allow this
        //cerr << "warning: Looking for the end of a node" << endl;
        //TODO: Put this in its own function? Also double check for off by ones
        //Offset of the start of the node vector
        return get_id_from_offset(record_offset);
    } else if (type == SNARL || type == DISTANCED_SNARL || type == OVERSIZED_SNARL)  {
        return ((*records)->at(record_offset + SNARL_END_NODE_OFFSET)) >> 1;
    } else if (type == CHAIN || type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        return ((*records)->at(record_offset + CHAIN_END_NODE_OFFSET)) >> 1;
    } else if (type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL) {
        throw runtime_error("error: trying to find the end node of a root snarl");
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
}

id_t SnarlDistanceIndex::SnarlTreeRecord::get_end_orientation() const {
    record_t type = get_record_type();
    if (type == ROOT) {
        //TODO: Also not totally sure what this should do
        throw runtime_error("error: trying to get the end node of the root");
    } else if (type == NODE || type == DISTANCED_NODE) {
        //TODO: I'm not sure if I want to allow this
        //cerr << "warning: Looking for the end of a node" << endl;
        //TODO: Put this in its own function? Also double check for off by ones
        //Offset of the start of the node vector
        return get_id_from_offset(record_offset);
    } else if (type == SNARL || type == DISTANCED_SNARL || type == OVERSIZED_SNARL)  {
        return ((*records)->at(record_offset + SNARL_END_NODE_OFFSET)) & 1;
    } else if (type == CHAIN || type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        return ((*records)->at(record_offset + CHAIN_END_NODE_OFFSET)) & 1;
    } else if (type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL) {
        throw runtime_error("error: trying to find the end node of a root snarl");
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
}

size_t SnarlDistanceIndex::SnarlTreeRecord::get_offset_from_id (const id_t id) const {
    size_t node_records_offset = (*records)->at(COMPONENT_COUNT_OFFSET) + ROOT_RECORD_SIZE;
    size_t offset = (id-(*records)->at(MIN_NODE_ID_OFFSET)) * NODE_RECORD_SIZE;
    return node_records_offset + offset;

}
id_t SnarlDistanceIndex::SnarlTreeRecord::get_id_from_offset(size_t offset) const {
    size_t min_node_id = (*records)->at(MIN_NODE_ID_OFFSET);
    size_t node_records_offset = (*records)->at(COMPONENT_COUNT_OFFSET) + ROOT_RECORD_SIZE;
    return ((offset-node_records_offset) / NODE_RECORD_SIZE) + min_node_id;
}


bool SnarlDistanceIndex::SnarlTreeRecord::has_connectivity(endpoint_t start, endpoint_t end){
    return has_connectivity(endpoints_to_connectivity(start, end));
}

bool SnarlDistanceIndex::SnarlTreeRecord::has_external_connectivity(endpoint_t start, endpoint_t end) {
    if (start == START && end == START) {
        return is_externally_start_start_connected();
    } else if (start == END && end == END) {
        return is_externally_end_end_connected();
    } else if ( (start == START && end == END ) || (start == END && end == START)) {
        return is_externally_start_end_connected();
    } else {
        return false;
    }
}

size_t SnarlDistanceIndex::SnarlTreeRecord::get_parent_record_offset() const {
    record_t type = get_record_type();
    if (type == ROOT) {
        return 0;
    } else if (type == NODE || type == DISTANCED_NODE) {
        return ((*records)->at(record_offset + NODE_PARENT_OFFSET));
    } else if (type == SNARL || type == DISTANCED_SNARL || type == OVERSIZED_SNARL
            || type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL)  {
        return ((*records)->at(record_offset + SNARL_PARENT_OFFSET));
    } else if (type == CHAIN || type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        return ((*records)->at(record_offset + CHAIN_PARENT_OFFSET));
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
};

SnarlDistanceIndex::record_t SnarlDistanceIndex::SnarlTreeRecordConstructor::get_record_type() const {
    return static_cast<record_t>((*records)->at(record_offset) >> 9);
}
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_start_start_connected() {
#ifdef debug_indexing
    cerr << record_offset << " set start_start_connected" << endl;
#endif    

    (*records)->at(record_offset) = (*records)->at(record_offset) | 32;
}
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_start_end_connected() {
#ifdef debug_indexing
    cerr << record_offset << " set start_end_connected" << endl;
#endif

    (*records)->at(record_offset) = (*records)->at(record_offset) | 16;
}
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_start_tip_connected() {
#ifdef debug_indexing
    cerr << record_offset << " set start_tip_connected" << endl;
#endif

    (*records)->at(record_offset) = (*records)->at(record_offset) | 8;
}
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_end_end_connected() {
#ifdef debug_indexing
    cerr << record_offset << " set end_end_connected" << endl;
#endif

    (*records)->at(record_offset) = (*records)->at(record_offset) | 4;
}
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_end_tip_connected() {
#ifdef debug_indexing
    cerr << record_offset << " set end_tip_connected" << endl;
#endif

    (*records)->at(record_offset) = (*records)->at(record_offset) | 2;
}
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_tip_tip_connected() {
#ifdef debug_indexing
    cerr << record_offset << " set tpi_tip_connected" << endl;
#endif

    (*records)->at(record_offset) = (*records)->at(record_offset) | 1;
}
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_externally_start_end_connected() {
#ifdef debug_indexing
    cerr << record_offset << " set externally start_end connected" << endl;
#endif

    (*records)->at(record_offset) = (*records)->at(record_offset) | 64;
}
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_externally_start_start_connected() const {
#ifdef debug_indexing
    cerr << record_offset << " set externally start_start connected" << endl;
#endif
    (*records)->at(record_offset) = (*records)->at(record_offset) | 128;
}
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_externally_end_end_connected() const {
#ifdef debug_indexing
    cerr << record_offset << " set externally end_end connected" << endl;
#endif
    (*records)->at(record_offset) = (*records)->at(record_offset) | 256;
}
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_record_type(record_t type) {
#ifdef debug_indexing
    cerr << record_offset << " set record type to be " << type << endl;
    assert((*records)->at(record_offset) == 0);
#endif
    (*records)->at(record_offset) = ((static_cast<size_t>(type) << 9) | ((*records)->at(record_offset) & 511));
}


void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_min_length(size_t length) {
    record_t type = get_record_type();
    size_t offset;
    if (type == DISTANCED_NODE) {
        offset = record_offset + NODE_LENGTH_OFFSET;
    } else if (type == DISTANCED_SNARL || type == OVERSIZED_SNARL)  {
        offset = record_offset + SNARL_MIN_LENGTH_OFFSET;
    } else if (type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        offset = record_offset + CHAIN_MIN_LENGTH_OFFSET;
    } else if (type == NODE || type == SNARL || type == CHAIN ) {
        throw runtime_error("error: trying to access get distance in a distanceless index");
    } else if (type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL) {
        throw runtime_error("error: set the length of a root snarl");
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
#ifdef debug_indexing
    cerr << offset << " set min length to be " << length << endl;
    assert((*records)->at(offset) == 0);
#endif

    (*records)->at(offset) = (length == std::numeric_limits<size_t>::max() ? 0 : length + 1);
};
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_max_length(size_t length) {
    record_t type = get_record_type();
    size_t offset;
    if (type == DISTANCED_NODE) {
        offset = record_offset + NODE_LENGTH_OFFSET;
    } else if (type == DISTANCED_SNARL || type == OVERSIZED_SNARL)  {
        offset = record_offset + SNARL_MAX_LENGTH_OFFSET;
    } else if (type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        offset = record_offset + CHAIN_MAX_LENGTH_OFFSET;
    } else if (type == DISTANCED_NODE || type == SNARL || type == CHAIN) {
        throw runtime_error("error: trying to access get distance in a distanceless index");
    }  else if (type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL) {
        throw runtime_error("error: set the length of a root snarl");
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
#ifdef debug_indexing
    cerr << offset << " set max length to be " << length << endl;
    assert((*records)->at(offset) == 0);
#endif

    (*records)->at(offset) = (length == std::numeric_limits<size_t>::max() ? 0 : length + 1);
};

void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_rank_in_parent(size_t rank) {
    record_t type = get_record_type();
    size_t offset;
    if (type == NODE || type == DISTANCED_NODE) {
        offset = record_offset + NODE_RANK_OFFSET;
    } else if (type == SNARL || type == DISTANCED_SNARL || type == OVERSIZED_SNARL
            || type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL)  {
        offset = record_offset + SNARL_RANK_OFFSET;
    } else if (type == CHAIN || type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        offset = record_offset + CHAIN_RANK_OFFSET;
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
#ifdef debug_indexing
    cerr << offset << " set rank in parent to be " << rank << endl;
    assert((*records)->at(offset) >> 1 == 0);
#endif

    bool rev = (*records)->at(offset) & 1;
    (*records)->at(offset) = (rank << 1) | rev;
};
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_is_reversed_in_parent(bool rev) {
    record_t type = get_record_type();
    size_t offset;
    if (type == NODE || type == DISTANCED_NODE) {
        offset = record_offset + NODE_RANK_OFFSET;
    } else if (type == SNARL || type == DISTANCED_SNARL || type == OVERSIZED_SNARL
            || type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL)  {
        offset = record_offset + SNARL_RANK_OFFSET;
    } else if (type == CHAIN || type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        offset = record_offset + CHAIN_RANK_OFFSET;
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
#ifdef debug_indexing
    cerr << offset << " set rev in parent to be " << rev << endl;
#endif

    (*records)->at(offset) =  (((*records)->at(offset)>>1)<<1) | rev;
};
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_parent_record_offset(size_t pointer){
    record_t type = get_record_type();
    size_t offset;
    if (type == NODE || type == DISTANCED_NODE) {
        offset = record_offset + NODE_PARENT_OFFSET;
    } else if (type == SNARL || type == DISTANCED_SNARL || type == OVERSIZED_SNARL
            || type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL)  {
#ifdef debug_indexing
        if (type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL) {
            assert(pointer == 0);
        }
#endif

        offset = record_offset + SNARL_PARENT_OFFSET;
    } else if (type == CHAIN || type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        offset = record_offset + CHAIN_PARENT_OFFSET;
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
#ifdef debug_indexing
    cerr << offset << " set parent offset to be " << pointer << endl;
    assert((*records)->at(offset) == 0);
#endif

    (*records)->at(offset) = pointer;
};
//Rev is true if the node is traversed backwards to enter the snarl
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_start_node(id_t id, bool rev) {
    record_t type = get_record_type();
    size_t offset;
    if (type == ROOT || type == NODE || type == DISTANCED_NODE) {
        throw runtime_error("error: trying to set the node id of a node or root");
    } else if (type == SNARL || type == DISTANCED_SNARL || type == OVERSIZED_SNARL)  {
        offset = record_offset + SNARL_START_NODE_OFFSET;
    } else if (type == CHAIN || type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        offset = record_offset + CHAIN_START_NODE_OFFSET;
    }  else if (type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL) {
        throw runtime_error("error: set the start node of a root snarl");
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
#ifdef debug_indexing
    cerr << offset << " set start node to be " << id << " facing " << (rev ? "rev" : "fd") << endl;
    assert((*records)->at(offset) == 0);
#endif

    (*records)->at(offset) = (id << 1) | rev;
}
void SnarlDistanceIndex::SnarlTreeRecordConstructor::set_end_node(id_t id, bool rev) const {
    record_t type = get_record_type();
    size_t offset;
    if (type == ROOT || type == NODE || type == DISTANCED_NODE) {
        throw runtime_error("error: trying to set the node id of a node or root");
    } else if (type == SNARL || type == DISTANCED_SNARL || type == OVERSIZED_SNARL)  {
        offset = record_offset + SNARL_END_NODE_OFFSET;
    } else if (type == CHAIN || type == DISTANCED_CHAIN || type == MULTICOMPONENT_CHAIN)  {
        offset = record_offset + CHAIN_END_NODE_OFFSET;
    }  else if (type == ROOT_SNARL || type == DISTANCED_ROOT_SNARL) {
        throw runtime_error("error: set the end node of a root snarl");
    } else {
        throw runtime_error("error: trying to access a snarl tree node of the wrong type");
    }
#ifdef debug_indexing
    cerr << offset << " set end node to be " << id << " facing " << (rev ? "rev" : "fd") << endl;
    assert((*records)->at(offset) == 0);
#endif

    (*records)->at(offset) = (id << 1) | rev;
}



bool SnarlDistanceIndex::RootRecord::for_each_child(const std::function<bool(const handlegraph::net_handle_t&)>& iteratee) const {
    size_t connected_component_count = get_connected_component_count();
    for (size_t i = 0 ; i < connected_component_count ; i++) {

        size_t child_offset = (*records)->at(record_offset + ROOT_RECORD_SIZE + i);
        net_handle_record_t type = SnarlTreeRecord(child_offset, records).get_record_handle_type();
        record_t record_type = SnarlTreeRecord(child_offset, records).get_record_type();


        if (record_type == ROOT_SNARL || record_type == DISTANCED_ROOT_SNARL) {
            //This is a bunch of root components that are connected, so go through each
            SnarlRecord snarl_record(child_offset, records);
            if (! snarl_record.for_each_child(iteratee)) {
                return false;
            }
        } else {
            //Otherwise, it is a separate connected component
            net_handle_t child_handle =  get_net_handle(child_offset, START_END, type);
            if (!iteratee(child_handle)) {
                return false;
            }
        }
    }
    return true;
}

void SnarlDistanceIndex::RootRecordConstructor::set_connected_component_count(size_t connected_component_count) {
#ifdef debug_indexing
    cerr << RootRecord::record_offset+COMPONENT_COUNT_OFFSET << " set connected component to be " << connected_component_count << endl;
    assert((*SnarlTreeRecordConstructor::records)->at(RootRecord::record_offset+COMPONENT_COUNT_OFFSET) == 0);
#endif

    (*SnarlTreeRecordConstructor::records)->at(RootRecord::record_offset+COMPONENT_COUNT_OFFSET)=connected_component_count;
}
void SnarlDistanceIndex::RootRecordConstructor::set_node_count(size_t node_count) {
#ifdef debug_indexing
    cerr << RootRecord::record_offset+NODE_COUNT_OFFSET << " set node count to be " << node_count << endl;
    assert((*RootRecord::records)->at(RootRecord::record_offset+NODE_COUNT_OFFSET) == 0);
#endif

    (*SnarlTreeRecordConstructor::records)->at(RootRecord::record_offset+NODE_COUNT_OFFSET)=node_count;
}
void SnarlDistanceIndex::RootRecordConstructor::set_min_node_id(id_t node_id) {
#ifdef debug_indexing
    cerr << RootRecord::record_offset+MIN_NODE_ID_OFFSET << " set min node id to be " << node_id << endl;
    assert((*RootRecord::records)->at(RootRecord::record_offset+MIN_NODE_ID_OFFSET) == 0);
#endif

    (*SnarlTreeRecordConstructor::records)->at(RootRecord::record_offset+MIN_NODE_ID_OFFSET)=node_id;
}
void SnarlDistanceIndex::RootRecordConstructor::add_component(size_t index, size_t offset) {
#ifdef debug_indexing
    cerr << RootRecord::record_offset+ROOT_RECORD_SIZE+index << " set new component " << offset << endl;
    assert((*SnarlTreeRecordConstructor::records)->at(RootRecord::record_offset+ROOT_RECORD_SIZE+index) == 0);
    assert(index < get_connected_component_count());
#endif

    (*SnarlTreeRecordConstructor::records)->at(RootRecord::record_offset+ROOT_RECORD_SIZE+index)
        = offset;

}


size_t SnarlDistanceIndex::SnarlRecord::distance_vector_size(record_t type, size_t node_count) {
    if (type == SNARL || type == ROOT_SNARL){
        //For a normal snarl, its just the record size and the pointers to children
        return 0;
    } else if (type == DISTANCED_SNARL) {
        //For a normal min distance snarl, record size and the pointers to children, and
        //matrix of distances
        size_t node_side_count = node_count * 2 + 2;
        return  (((node_side_count+1)*node_side_count) / 2);
    } else if (type ==  OVERSIZED_SNARL){
        //For a large min_distance snarl, record the side, pointers to children, and just
        //the min distances from each node side to the two boundary nodes
        size_t node_side_count = node_count * 2 + 2;
        return  (node_side_count * 2);
    } else if (type == DISTANCED_ROOT_SNARL) {
        size_t node_side_count = node_count * 2;
        return  (((node_side_count+1)*node_side_count) / 2);
    } else {
        throw runtime_error ("error: this is not a snarl");
    }
}

size_t SnarlDistanceIndex::SnarlRecord::record_size (record_t type, size_t node_count) {
    return SNARL_RECORD_SIZE + distance_vector_size(type, node_count) + node_count;
}
size_t SnarlDistanceIndex::SnarlRecord::record_size() {
    record_t type = get_record_type();
    return record_size(type, get_node_count());
}


int64_t SnarlDistanceIndex::SnarlRecord::get_distance_vector_offset(size_t rank1, bool right_side1, size_t rank2,
        bool right_side2, size_t node_count, record_t type) {

    //how many node sides in this snarl
    size_t node_side_count = type == DISTANCED_ROOT_SNARL ? node_count * 2 : node_count * 2 + 2;

    //make sure we're looking at the correct node side
    //If this is the start or end node, then we don't adjust based on orientation
    //because we only care about the inner sides. If this is a root snarl, then
    //there is no start or end node and the ranks 0 and 1 are not special
     if (type == DISTANCED_ROOT_SNARL) {
        rank1 = rank1 * 2;
        if (right_side1) {
            rank1 += 1;
        }
     } else if (rank1 != 0 && rank1 != 1) {
        rank1 = (rank1-1) * 2;
        if (right_side1) {
            rank1 += 1;
        }
    }
    if (type == DISTANCED_ROOT_SNARL) {
        rank2 = rank2 * 2;
        if (right_side2) {
            rank2 += 1;
        }
    } else if (rank2 != 0 && rank2 != 1) {
        rank2 = (rank2-1) * 2;
        if (right_side2) {
            rank2 += 1;
        }
    }

    //reverse order of ranks if necessary
    if (rank1 > rank2) {
        size_t tmp = rank1;
        rank1 = rank2;
        rank2 = tmp;
    }

    if (type == SNARL || type == ROOT_SNARL) {
        throw runtime_error("error: trying to access distance in a distanceless snarl tree");
    } else if (type == DISTANCED_SNARL || type == DISTANCED_ROOT_SNARL) {
        //normal distance index
        size_t k = node_side_count-rank1;
        return (((node_side_count+1) * node_side_count)/2) - (((k+1)*k) / 2) + rank2 - rank1;
    } else if (type ==  OVERSIZED_SNARL) {
        //abbreviated distance index storing only the distance from each node side to the
        //start and end
        if (rank1 == 0) {
            return rank2;
        } else if (rank1 == 1) {
            return node_count + 2 + rank2;
        } else {
            throw runtime_error("error: trying to access distance in an oversized snarl");
        }
    } else {
        throw runtime_error("error: trying to distance from something that isn't a snarl");
    }
}

int64_t SnarlDistanceIndex::SnarlRecord::get_distance_vector_offset(size_t rank1, bool right_side1,
        size_t rank2, bool right_side2) const {
    return get_distance_vector_offset(rank1, right_side1, rank2, right_side2,
        get_node_count(), get_record_type());
}

int64_t SnarlDistanceIndex::SnarlRecord::get_distance(size_t rank1, bool right_side1, size_t rank2, bool right_side2) const {

    //offset of the start of the distance vectors in snarl_tree_records
    size_t distance_vector_start = record_offset + SNARL_RECORD_SIZE + get_node_count();
    //Offset of this particular distance in the distance vector
    size_t distance_vector_offset = get_distance_vector_offset(rank1, right_side1, rank2, right_side2);

    size_t val = (*records)->at(distance_vector_start + distance_vector_offset);
    return  val == 0 ? std::numeric_limits<int64_t>::max() : val-1;

}

bool SnarlDistanceIndex::SnarlRecord::for_each_child(const std::function<bool(const net_handle_t&)>& iteratee) const {
    size_t child_count = get_node_count();
    size_t child_record_offset = get_child_record_pointer();
    for (size_t i = 0 ; i < child_count ; i++) {
        size_t child_offset =  (*records)->at(child_record_offset + i);
        net_handle_record_t type = SnarlTreeRecord(child_offset, records).get_record_handle_type();
        assert(type == NODE_HANDLE || type == CHAIN_HANDLE);
        net_handle_t child_handle =  get_net_handle (child_offset, START_END, CHAIN_HANDLE);
        bool result = iteratee(child_handle);
        if (result == false) {
            return false;
        }
    }
    return true;
}


void SnarlDistanceIndex::SnarlRecordConstructor::set_distance(size_t rank1, bool right_side1, size_t rank2, bool right_side2, int64_t distance) {
    //offset of the start of the distance vectors in snarl_tree_records
    size_t distance_vector_start = SnarlRecord::record_offset + SNARL_RECORD_SIZE + get_node_count();
    //Offset of this particular distance in the distance vector
    size_t distance_vector_offset = get_distance_vector_offset(rank1, right_side1, rank2, right_side2);

    size_t val = distance == std::numeric_limits<int64_t>::max() ? 0 : distance+1;
#ifdef debug_indexing
    cerr <<  distance_vector_start + distance_vector_offset << " set distance_value " << val << endl;
    //assert(SnarlTreeRecordConstructor::records->at(distance_vector_start + distance_vector_offset) == 0 ||
    //        SnarlTreeRecordConstructor::records->at(distance_vector_start + distance_vector_offset) == val);
#endif


    (*SnarlTreeRecordConstructor::records)->at(distance_vector_start + distance_vector_offset) = val;
}

//Node count is the number of nodes in the snarl, not including boundary nodes
void SnarlDistanceIndex::SnarlRecordConstructor::set_node_count(size_t node_count) {
#ifdef debug_indexing
    cerr << SnarlRecord::record_offset + SNARL_NODE_COUNT_OFFSET << " set snarl node count " << node_count << endl;
    assert(node_count > 0);//TODO: Don't bother making a record for trivial snarls
    assert((*SnarlTreeRecordConstructor::records)->at(SnarlRecord::record_offset + SNARL_NODE_COUNT_OFFSET) == 0);
#endif

    (*SnarlTreeRecordConstructor::records)->at(SnarlRecord::record_offset + SNARL_NODE_COUNT_OFFSET) = node_count;
}


void SnarlDistanceIndex::SnarlRecordConstructor::set_child_record_pointer(size_t pointer) {
#ifdef debug_indexing
    cerr << SnarlRecord::record_offset+SNARL_CHILD_RECORD_OFFSET << " set snarl child record pointer " << pointer << endl;
    assert((*SnarlTreeRecordConstructor::records)->at(SnarlRecord::record_offset+SNARL_CHILD_RECORD_OFFSET) == 0);
#endif

    (*SnarlTreeRecordConstructor::records)->at(SnarlRecord::record_offset+SNARL_CHILD_RECORD_OFFSET) = pointer;
}
//Add a reference to a child of this snarl. Assumes that the index is completed up
//to here
void SnarlDistanceIndex::SnarlRecordConstructor::add_child(size_t pointer){
#ifdef debug_indexing
    cerr << (*SnarlTreeRecordConstructor::records)->size() << " Adding child pointer to the end of the array " << endl;
#endif
    size_t start_i = (*SnarlTreeRecordConstructor::records)->size();
    (*SnarlTreeRecordConstructor::records)->resize(start_i+1);
    (*SnarlTreeRecordConstructor::records)->at(start_i) = pointer;
}

size_t SnarlDistanceIndex::ChainRecord::get_node_count() const {
    if (get_record_handle_type() == NODE_HANDLE) {
        return 1;
    } else {
        return (*records)->at(record_offset + CHAIN_NODE_COUNT_OFFSET);
    }
}

pair<size_t, bool> SnarlDistanceIndex::ChainRecord::get_last_child_offset() const {
    if (get_record_handle_type() == NODE_HANDLE) {
        throw runtime_error("error: Trying to get children of a node");
    } else {
        size_t val = (*records)->at(record_offset + CHAIN_LAST_CHILD_OFFSET) < 1;
        return make_pair(val<1, val & 1);
    }
}
bool SnarlDistanceIndex::ChainRecord::get_is_looping_chain_connected_backwards() const {
    if (get_record_handle_type() == NODE_HANDLE) {
        throw runtime_error("error: Trying to get children of a node");
    } else {
        size_t val = (*records)->at(record_offset + CHAIN_LAST_CHILD_OFFSET);
        return val & 1;
    }
}

int64_t SnarlDistanceIndex::ChainRecord::get_chain_node_id(size_t pointer) const {
    if (get_record_handle_type() == NODE_HANDLE) {
        throw runtime_error("error: Trying to get chain distances from a node");
    }
    return (*records)->at(pointer+CHAIN_NODE_ID_OFFSET);
}
int64_t SnarlDistanceIndex::ChainRecord::get_prefix_sum_value(size_t pointer) const {
    if (get_record_handle_type() == NODE_HANDLE) {
        throw runtime_error("error: Trying to get chain distances from a node");
    }
    size_t val = (*records)->at(pointer+CHAIN_NODE_PREFIX_SUM_OFFSET);
    return val == 0 ? std::numeric_limits<int64_t>::max() : val-1;
}
int64_t SnarlDistanceIndex::ChainRecord::get_forward_loop_value(size_t pointer) const {
    if (get_record_handle_type() == NODE_HANDLE) {
        throw runtime_error("error: Trying to get chain distances from a node");
    }
    size_t val = (*records)->at(pointer+CHAIN_NODE_FORWARD_LOOP_OFFSET);
    return val == 0 ? std::numeric_limits<int64_t>::max() : val-1;
}
int64_t SnarlDistanceIndex::ChainRecord::get_reverse_loop_value(size_t pointer) const {
    if (get_record_handle_type() == NODE_HANDLE) {
        throw runtime_error("error: Trying to get chain distances from a node");
    }
    size_t val =  (*records)->at(pointer+CHAIN_NODE_REVERSE_LOOP_OFFSET);
    return val == 0 ? std::numeric_limits<int64_t>::max() : val-1;
}
size_t SnarlDistanceIndex::ChainRecord::get_chain_component(size_t pointer, bool get_end) const {
    if (get_record_handle_type() == NODE_HANDLE) {
        throw runtime_error("error: Trying to get chain distances from a node");
    }
    if (get_record_type() != MULTICOMPONENT_CHAIN){
        throw runtime_error("error: Trying to get the component of a single component chain");
    }
    if (get_chain_node_id(pointer) == get_start_id() && !get_end) {
        //The first component is always 0
        return 0;
    } else {
        return (*records)->at(pointer+CHAIN_NODE_COMPONENT_OFFSET);
    }
}

int64_t SnarlDistanceIndex::ChainRecord::get_distance(tuple<size_t, bool, int64_t> node1,
                     tuple<size_t, bool, int64_t> node2) const {

    if (std::get<0>(node1) > std::get<0>(node2)) {
        //If the first node comes after the second in the chain, reverse them
        tuple<size_t, bool, size_t> tmp = node1;
        node1 = node2;
        node2 = tmp;

    }

    bool is_looping_chain = get_start_id() == get_end_id();
    if (get_record_handle_type() == NODE_HANDLE) {
        throw runtime_error("error: Trying to get chain distances from a node");
    } else if (get_record_type() == MULTICOMPONENT_CHAIN) {
        if (get_chain_component(std::get<0>(node1)) != get_chain_component(std::get<0>(node2))) {
            if (is_looping_chain) {
                //If this is a looping chain, then the first/last node could be in two
                //components
                return get_distance_taking_chain_loop(node1, node2);
            } else {
                return std::numeric_limits<int64_t>::max();
            }
        }
    }


    int64_t distance;

    if (!std::get<1>(node1) && std::get<1>(node2)) {
        //Right of 1 and left of 2, so a simple forward traversal of the chain
        if (std::get<0>(node1) == std::get<0>(node2)) {
            //If these are the same node, then the path would need to go around the node
            distance = sum({get_forward_loop_value(std::get<0>(node1)),
                        get_reverse_loop_value(std::get<0>(node1)),
                        std::get<2>(node1)});
        } else {
            distance = minus(get_prefix_sum_value(std::get<0>(node2)) - get_prefix_sum_value(std::get<0>(node1)),
                 std::get<2>(node1));
        }
    } else if (!std::get<1>(node1) && !std::get<1>(node2)) {
        //Right side of 1 and right side of 2
        if (std::get<0>(node1) == std::get<0>(node2)) {
            distance = get_forward_loop_value(std::get<0>(node2));

        } else {
            distance = minus( sum({get_prefix_sum_value(std::get<0>(node2)) - get_prefix_sum_value(std::get<0>(node1)) ,
                               std::get<2>(node2),
                               get_forward_loop_value(std::get<0>(node2))}),
                         std::get<2>(node1));
        }
    } else if (std::get<1>(node1) && std::get<1>(node2)) {
        //Left side of 1 and left side of 2
        if (std::get<0>(node1) == std::get<0>(node2)) {
            distance = get_reverse_loop_value(std::get<0>(node1));

        } else {
            distance = sum({get_prefix_sum_value(std::get<0>(node2)) - get_prefix_sum_value(std::get<0>(node1)),
                            get_reverse_loop_value(std::get<0>(node1))});
        }
    } else {
        assert(std::get<1>(node1) && !std::get<1>(node2));
        //Left side of 1 and right side of 2
        distance = sum({get_prefix_sum_value(std::get<0>(node2)) - get_prefix_sum_value(std::get<0>(node1)),
                        get_reverse_loop_value(std::get<0>(node1)),
                        get_forward_loop_value(std::get<0>(node2)),
                        std::get<2>(node2)});

    }
    if (is_looping_chain) {
        distance = std::min(distance, get_distance_taking_chain_loop(node1, node2));
    }
    return distance;
}


int64_t SnarlDistanceIndex::ChainRecord::get_distance_taking_chain_loop(tuple<size_t, bool, int64_t> node1,
                     tuple<size_t, bool, int64_t> node2) const {
    //This is only called by get_distance, so the nodes should be ordered
    assert (std::get<0>(node1) <= std::get<0>(node2));

    if (get_record_handle_type() == NODE_HANDLE) {
        throw runtime_error("error: Trying to get chain distances from a node");
    } else if (get_record_type() == MULTICOMPONENT_CHAIN) {
        size_t last_component = get_chain_component(get_first_node_offset(), true);
        bool first_in_first_component = get_chain_component(std::get<0>(node1)) == 0 || get_chain_component(std::get<0>(node1)) == last_component;
        bool second_in_first_component = get_chain_component(std::get<0>(node2)) == 0 || get_chain_component(std::get<0>(node2)) == last_component;
        bool can_loop = get_is_looping_chain_connected_backwards();

        if (!can_loop || !first_in_first_component || ! second_in_first_component) {
            //If this is a multicomponent chain then it can only reach around backwards if both nodes
            //are in the first (last) component
            return std::numeric_limits<int64_t>::max();
        }
    }


    int64_t distance;
    assert(get_start_id() == get_end_id());

    if (!std::get<1>(node1) && std::get<1>(node2)) {
        //Right of 1 and left of 2, so a simple forward traversal of the chain
        //loop forward from the first node, from the start of the chain to the first
        //node, from the end of the node to the second node, and the reverse loop of the second
        distance = sum({get_forward_loop_value(std::get<0>(node1)),
                             std::get<2>(node1),
                             get_prefix_sum_value(std::get<0>(node1)),
                             minus(get_min_length(), get_prefix_sum_value(std::get<0>(node2))),
                             get_reverse_loop_value(std::get<0>(node2))});
    } else if (!std::get<1>(node1) && !std::get<1>(node2)) {
        //Right side of 1 and right side of 2

        //Check distance for taking loop in chain: loop forward from the first node, from the start of the
        //chain to the first node, from the end of the node to the second node
        distance = sum({get_forward_loop_value(std::get<0>(node1)),
                             std::get<2>(node1),
                             get_prefix_sum_value(std::get<0>(node1)),
                             minus(minus(get_min_length(), get_prefix_sum_value(std::get<0>(node2))),
                                std::get<2>(node2))});
    } else if (std::get<1>(node1) && std::get<1>(node2)) {
        //Left side of 1 and left side of 2

        //from the first node left to the start, around the
        //chain loop, then the reverse loop of the second node
        //This assumes that the length of the chain only includes the start/end node's length once,
        //which it does but might change
        distance = sum({get_prefix_sum_value(std::get<0>(node1)),
                             minus(get_min_length(), get_prefix_sum_value(std::get<0>(node2))),
                             get_reverse_loop_value(std::get<0>(node2))});
    } else {
        assert(std::get<1>(node1) && !std::get<1>(node2));
        //Left side of 1 and right side of 2

        //Check the distance going backwards around the chain
        distance = sum({get_prefix_sum_value(std::get<0>(node1)),
                             minus(minus(get_min_length(), get_prefix_sum_value(std::get<0>(node2))),
                              std::get<2>(node2))});
    }
    return distance;
}

size_t SnarlDistanceIndex::ChainRecord::get_first_node_offset() const {
    if (get_record_handle_type() == NODE_HANDLE) {
        throw runtime_error("error: Trying to get chain traversal from a node");
    }
    return record_offset + CHAIN_RECORD_SIZE;
}

pair<size_t, bool> SnarlDistanceIndex::ChainRecord::get_next_child(const pair<size_t, bool> pointer, bool go_left) const {
    //If this is a multicomponent chain, then the size of each node record in the chain is bigger
    if (get_record_handle_type() == NODE_HANDLE) {
        throw runtime_error("error: Trying to get chain traversal from a node");
    }
    size_t node_record_size = get_record_type() == MULTICOMPONENT_CHAIN ? CHAIN_NODE_MULTICOMPONENT_RECORD_SIZE : CHAIN_NODE_RECORD_SIZE;
    if (pointer.second) {
        //This is a snarl
        if (go_left) {
            return make_pair(pointer.first - node_record_size, false);
        } else {
            if (get_start_id() == get_end_id() && pointer.first == get_last_child_offset().first) {
                //If this is the last child in a looping chain
                return make_pair(get_first_node_offset(), false);
            } else {
                size_t snarl_record_length = SnarlRecord(pointer.first, records).record_size();
                return make_pair(pointer.first + snarl_record_length + 1, false);
            }
        }
    } else {
        //This is a node
        if (go_left) {
            //Looking left in the chain
            if (pointer.first == get_first_node_offset()) {
                //If this is the first node in the chain
                if (get_start_id() == get_end_id()) {
                    pair<size_t, bool> last_child = get_last_child_offset();
                    return last_child; //TODO: I'm not sure if this is always true (snarl)
                } else {
                    return make_pair(0, false);
                }
            }
            size_t snarl_record_size = (*records)->at(pointer.first-1);
            if (snarl_record_size == 0) {
                //Just another node to the left
                return make_pair(pointer.first-node_record_size, false);
            } else {
                //There is a snarl to the left of this node
                return make_pair(pointer.first - snarl_record_size - 1, true);
            }
        } else {
            //Looking right in the chain
            if ((*records)->at(pointer.first+node_record_size-1) == 0 &&
                (*records)->at(pointer.first+node_record_size) == 0) {
                //If this is the last node in the chain
                if (get_start_id() == get_end_id()) {
                    //If this loops, go back to the beginning
                    return make_pair(get_first_node_offset(), false);
                } else {
                    return make_pair(0, false);
                }
            }
            size_t snarl_record_size = (*records)->at(pointer.first+node_record_size-1);
            return make_pair(pointer.first+node_record_size, snarl_record_size != 0);
        }
    }
}
net_handle_t SnarlDistanceIndex::ChainRecord::get_next_child(const net_handle_t& net_handle, bool go_left) const {
    //get the next child in the chain. net_handle must point to a snarl or node in the chain
    net_handle_record_t handle_type = get_handle_type(net_handle);
    net_handle_record_t record_type = get_record_handle_type();
    if (record_type == NODE_HANDLE) {
        //If this record is a node pretending to be a chain, then there is no next child
        assert(handle_type == NODE_HANDLE);
        assert(get_record_offset(net_handle) == record_offset);
        return net_handle;
    }

    //Get the current pointer, pointing at the net_handle in the chain
    if (handle_type == NODE_HANDLE) {
        //If this net handle is a node, then it's rank in parent points to it in the chain
        NodeRecord node_record(get_record_offset(net_handle), records);
        assert(node_record.get_parent_record_offset() == record_offset);
        pair<size_t, bool> next_pointer = get_next_child(
            make_pair(node_record.get_rank_in_parent(), false), go_left);
        if (next_pointer.first == 0 ){
            return net_handle;
        }
        bool next_is_reversed_in_parent = NodeRecord(
                get_offset_from_id((*records)->at(next_pointer.first)), records
            ).get_is_reversed_in_parent();

        connectivity_t connectivity = go_left == next_is_reversed_in_parent ? START_END : END_START;
        if (!next_pointer.second && next_pointer.first == get_first_node_offset()) {
            connectivity = endpoints_to_connectivity(get_end_endpoint(connectivity), get_end_endpoint(connectivity));
        }
        return get_net_handle(get_offset_from_id((*records)->at(next_pointer.first)),
                          connectivity,
                          next_pointer.second ? SNARL_HANDLE : NODE_HANDLE);
    } else {
        //Otherwise, it is a snarl and we can use the snarl's offset, since it exists in
        //the chain
        assert(handle_type == SNARL_HANDLE) ;
        pair<size_t, bool> next_pointer = get_next_child(
            make_pair(get_record_offset(net_handle), true), go_left);
        if (next_pointer.first == 0 ){
            return net_handle;
        }
        connectivity_t connectivity = next_pointer.second ? END_START : START_END;
        if (!next_pointer.second && next_pointer.first == get_first_node_offset()) {
            connectivity = endpoints_to_connectivity(get_end_endpoint(connectivity), get_end_endpoint(connectivity));
        }
        return get_net_handle(next_pointer.first, connectivity,
                          next_pointer.first ? SNARL_HANDLE : NODE_HANDLE);
    }
}

bool SnarlDistanceIndex::ChainRecord::for_each_child(const std::function<bool(const net_handle_t&)>& iteratee) const {

    if (get_record_handle_type() == NODE_HANDLE) {
        //If this is a node pretending to be a chain, just do it for the node
        return iteratee(get_net_handle(record_offset, START_END, NODE_HANDLE));
    }


    //If this is a node, then the offset of the node in the chain, false
    //If it is a snarl, then the offset of the snarl record, true
    pair<size_t, bool> current_child (get_first_node_offset(), false);
    bool is_first = true;

    while (current_child.first != 0) {
        net_handle_t child_handle = current_child.second 
            ? get_net_handle (current_child.first, START_END, SNARL_HANDLE)
            : get_net_handle (get_offset_from_id((*records)->at(current_child.first)), START_END, NODE_HANDLE);
        if (!is_first && current_child == make_pair(get_first_node_offset(), false)){
            //Don't look at the first node a second time
            return true;
        }

        bool result = iteratee(child_handle);
        if (result == false) {
            return false;
        }
        current_child = get_next_child(current_child, false);
        is_first = false;
    }
    return true;
}


void SnarlDistanceIndex::ChainRecordConstructor::add_node(id_t id, int64_t prefix_sum, int64_t forward_loop, int64_t reverse_loop) {
    assert(ChainRecord::get_record_type() != MULTICOMPONENT_CHAIN);
#ifdef debug_indexing
    cerr << (*SnarlTreeRecordConstructor::records)->size() << " - " << (*SnarlTreeRecordConstructor::records)->size() + 3 << " Adding chain's child node " << id << " to the end of the a     rray (values: " << prefix_sum << "," << forward_loop << "," << reverse_loop << ")" << endl;
#endif

    size_t start_i = (*SnarlTreeRecordConstructor::records)->size();
    (*SnarlTreeRecordConstructor::records)->resize(start_i+4);

    (*SnarlTreeRecordConstructor::records)->at(start_i) = id;
    (*SnarlTreeRecordConstructor::records)->at(start_i+1) = 
            prefix_sum==std::numeric_limits<int64_t>::max() ? 0 : prefix_sum+1;
    (*SnarlTreeRecordConstructor::records)->at(start_i+2) = 
            forward_loop==std::numeric_limits<int64_t>::max() ? 0 : forward_loop+1;
    (*SnarlTreeRecordConstructor::records)->at(start_i+3) = 
            reverse_loop==std::numeric_limits<int64_t>::max() ? 0 : reverse_loop+1;
}
void SnarlDistanceIndex::ChainRecordConstructor::add_node(id_t id, int64_t prefix_sum, int64_t forward_loop, int64_t reverse_loop, size_t component) {
    assert(ChainRecord::get_record_type() == MULTICOMPONENT_CHAIN);
#ifdef debug_indexing
    cerr << (*SnarlTreeRecordConstructor::records)->size() << " - " << (*SnarlTreeRecordConstructor::records)->size() + 4 << " Adding chain's child node the end of the array " << endl;
#endif

    size_t start_i = (*SnarlTreeRecordConstructor::records)->size();
    (*SnarlTreeRecordConstructor::records)->resize(start_i+5);

    (*SnarlTreeRecordConstructor::records)->at(start_i) = id;
    (*SnarlTreeRecordConstructor::records)->at(start_i+1) = prefix_sum==std::numeric_limits<int64_t>::max() ? 0 : prefix_sum+1;
    (*SnarlTreeRecordConstructor::records)->at(start_i+2) = forward_loop==std::numeric_limits<int64_t>::max() ? 0 : forward_loop+1;
    (*SnarlTreeRecordConstructor::records)->at(start_i+3) = reverse_loop==std::numeric_limits<int64_t>::max() ? 0 : reverse_loop+1;
    (*SnarlTreeRecordConstructor::records)->at(start_i+4) = component;
}
void SnarlDistanceIndex::ChainRecordConstructor::set_node_count(size_t node_count) {
#ifdef debug_indexing
    cerr << ChainRecord::record_offset + CHAIN_NODE_COUNT_OFFSET << " set chain node count " << node_count << endl;
    assert((*SnarlTreeRecordConstructor::records)->at(ChainRecord::record_offset + CHAIN_NODE_COUNT_OFFSET) == 0);
#endif

    (*SnarlTreeRecordConstructor::records)->at(ChainRecord::record_offset + CHAIN_NODE_COUNT_OFFSET) = node_count;
}

//The offset of the last child, if it is a snarl, and if it can loop
void SnarlDistanceIndex::ChainRecordConstructor::set_last_child_offset(size_t offset, bool is_snarl, bool loopable) {
#ifdef debug_indexing
    cerr << ChainRecord::record_offset + CHAIN_LAST_CHILD_OFFSET << " set chain last child offset " << offset << endl;
    assert((*SnarlTreeRecordConstructor::records)->at(ChainRecord::record_offset + CHAIN_LAST_CHILD_OFFSET) == 0);
#endif

    (*SnarlTreeRecordConstructor::records)->at(ChainRecord::record_offset + CHAIN_LAST_CHILD_OFFSET) = ((offset < 2) | (is_snarl<1)) | loopable;
}

void SnarlDistanceIndex::ChainRecordConstructor::add_trivial_snarl() {
#ifdef debug_indexing
    cerr << (*SnarlTreeRecordConstructor::records)->size() << "  Adding chain's trivial snarl to the end of the array " << endl;
#endif
    size_t start_i = (*SnarlTreeRecordConstructor::records)->size();
    (*SnarlTreeRecordConstructor::records)->resize(start_i+1);

    (*SnarlTreeRecordConstructor::records)->at(start_i) = 0;
}
//Add a snarl to the end of the chain and return a SnarlRecordConstructor pointing to it
SnarlDistanceIndex::SnarlRecordConstructor SnarlDistanceIndex::ChainRecordConstructor::add_snarl(size_t snarl_size, record_t type) {
    size_t snarl_record_size = SnarlRecord::record_size(type, snarl_size);
#ifdef debug_indexing
    cerr << (*SnarlTreeRecordConstructor::records)->size() << " Adding child snarl length to the end of the array " << endl;
#endif

    
    
    size_t start_i = (*SnarlTreeRecordConstructor::records)->size();
    (*SnarlTreeRecordConstructor::records)->resize(start_i+1);
    (*SnarlTreeRecordConstructor::records)->at(start_i) = snarl_record_size;
    (*SnarlTreeRecordConstructor::records)->reserve(start_i + snarl_record_size);
    SnarlRecordConstructor snarl_record(snarl_size, SnarlTreeRecordConstructor::records, type);
#ifdef debug_indexing
    cerr << (*SnarlTreeRecordConstructor::records)->size() << " Adding child snarl length to the end of the array " << endl;
#endif
    start_i = (*SnarlTreeRecordConstructor::records)->size();
    (*SnarlTreeRecordConstructor::records)->resize(start_i+1);
    (*SnarlTreeRecordConstructor::records)->at(start_i) = snarl_record_size;
    return snarl_record;
}
void SnarlDistanceIndex::ChainRecordConstructor::finish_chain(){
#ifdef debug_indexing
    cerr << (*SnarlTreeRecordConstructor::records)->size()  << " - " <<  (*SnarlTreeRecordConstructor::records)->size()+1 << " Adding the last two chain 0's to the end of the array " <<      endl;  
#endif

    size_t start_i = (*SnarlTreeRecordConstructor::records)->size();
    (*SnarlTreeRecordConstructor::records)->resize(start_i+2);
    (*SnarlTreeRecordConstructor::records)->at(start_i) = 0;
    (*SnarlTreeRecordConstructor::records)->at(start_i+1) = 0;
}
string SnarlDistanceIndex::net_handle_as_string(const net_handle_t& net) const {
    net_handle_record_t type = get_handle_type(net);
    SnarlTreeRecord record (net, &snarl_tree_records);
    net_handle_record_t record_type = record.get_record_handle_type();
    string result;
    if (type == ROOT_HANDLE) {
        return "root"; 
    } else if (type == NODE_HANDLE) {
        if (ends_at(net) == starts_at(net)) {
            return "node" + std::to_string( get_node_id_from_offset(get_record_offset(net))) + (ends_at(net) == START ? "rev" : "fd") + " that is the end node of a looping chain";
        }
        return  "node " + std::to_string( get_node_id_from_offset(get_record_offset(net))) + (ends_at(net) == START ? "rev" : "fd");
    } else if (type == SNARL_HANDLE) {
        if (record.get_record_type() == ROOT) {
            return "root snarl";
        }
        result += "snarl ";         
    } else if (type == CHAIN_HANDLE && record_type == NODE_HANDLE) {
        return  "node " + std::to_string( get_node_id_from_offset(get_record_offset(net)))
               + (ends_at(net) == START ? "rev" : "fd") + " pretending to be a chain";
    } else if (type == CHAIN_HANDLE) {
        result += "chain ";
    } else if (type == SENTINEL_HANDLE) {
        result += "sentinel of snarl ";
    } else {
        throw runtime_error("error: Unknown net_handle_t type");
    }
    result += ( std::to_string(record.get_start_id())
            + (record.get_start_orientation() ? "rev" : "fd")
            + "->"
            + std::to_string(record.get_end_id())
            + (record.get_end_orientation() ? "rev" : "fd"));
    result += "traversing ";
    result += (starts_at(net) == START ? "start" : (starts_at(net) == END ? "end" : "tip"));
    result += "->";
    result += (ends_at(net) == START ? "start" : (ends_at(net) == END ? "end" : "tip"));
    return result;
}


}