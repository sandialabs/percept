// Copyright 2014 Sandia Corporation. Under the terms of
// Contract DE-AC04-94AL85000 with Sandia Corporation, the
// U.S. Government retains certain rights in this software.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.


#include <adapt/HangingNodeAdapter.hpp>


  namespace percept {

    static bool m_debug_print = false;
    bool s_do_transition_break = false;

    bool HangingNodeAdapterBase::hasChildren(stk::mesh::Entity element)
    {
      unsigned nchild = m_pMesh.numChildren(element);
      return (nchild != 0);
    }

    void HangingNodeAdapterBase::set_sub_dim_refine_ranks(const bool will_refine[3])
    {
      for (unsigned i=0; i < 3; i++)
        m_will_refine[i] = will_refine[i];
    }
    void HangingNodeAdapterBase::get_sub_dim_refine_ranks(bool will_refine[3])
    {
      for (unsigned i=0; i < 3; i++)
        will_refine[i] = m_will_refine[i];
    }

    void HangingNodeAdapterBase::get_node_neighbors(stk::mesh::Entity element, LocalSetType& neighbors)
    {
      unsigned elem_nodes_size = m_pMesh.get_bulk_data()->num_nodes(element);
      stk::mesh::Entity const *elem_nodes = m_pMesh.get_bulk_data()->begin_nodes(element);
      for (unsigned inode=0; inode < elem_nodes_size; inode++)
        {
          stk::mesh::Entity node = elem_nodes[inode];
          unsigned node_elems_size = m_pMesh.get_bulk_data()->num_elements(node);
          stk::mesh::Entity const *node_elems = m_pMesh.get_bulk_data()->begin_elements(node);

          for (unsigned ielem=0; ielem < node_elems_size; ielem++)
            {
              stk::mesh::Entity elem = node_elems[ielem];
              if (elem != element) neighbors.insert(elem);
            }
        }
    }

    bool HangingNodeAdapterBase::is_face_neighbor(stk::mesh::Entity element, int element_level, stk::mesh::Entity neighbor, int neighbor_level,
                                                  bool check_level_early_return,
                                                  const CellTopologyData * const bucket_topo_data_element_0
                                                  )
    {

      stk::mesh::Entity parent_element = element;
      stk::mesh::Entity parent_neighbor = neighbor;
      if (check_level_early_return && std::abs(element_level - neighbor_level) > 1) return false;
      if (element_level > neighbor_level)
        {
          parent_element = m_pMesh.getParent(element, true);
        }
      else if (element_level < neighbor_level)
        {
          parent_neighbor = m_pMesh.getParent(neighbor, true);
        }
      bool fn = m_pMesh.is_face_neighbor(parent_element, parent_neighbor, 0, 0, bucket_topo_data_element_0);
      return fn;
    }

    bool HangingNodeAdapterBase::is_edge_neighbor(stk::mesh::Entity element, int element_level, stk::mesh::Entity neighbor, int neighbor_level,
                                                  bool check_level_early_return,
                                                  const CellTopologyData * const bucket_topo_data_element_0
                                                  )
    {
      stk::mesh::Entity parent_element = element;
      stk::mesh::Entity parent_neighbor = neighbor;
      if (check_level_early_return && std::abs(element_level - neighbor_level) > 1) return false;
      if (element_level > neighbor_level)
        {
          parent_element = m_pMesh.getParent(element, true);
        }
      else if (element_level < neighbor_level)
        {
          parent_neighbor = m_pMesh.getParent(neighbor, true);
        }
      VERIFY_OP_ON(parent_element, !=, stk::mesh::Entity(), "hmm");
      VERIFY_OP_ON(parent_neighbor, !=, stk::mesh::Entity(), "hmm");
      bool fn = m_pMesh.is_edge_neighbor(parent_element, parent_neighbor, 0, 0, bucket_topo_data_element_0);
      return fn;
    }

    bool HangingNodeAdapterBase::is_node_neighbor(stk::mesh::Entity element, int element_level, stk::mesh::Entity neighbor,
                                                  int neighbor_level, bool check_level_early_return)
    {
      stk::mesh::Entity parent_element = element;
      stk::mesh::Entity parent_neighbor = neighbor;
      if (check_level_early_return && std::abs(element_level - neighbor_level) > 1) return false;
      if (element_level > neighbor_level)
        {
          parent_element = m_pMesh.getParent(element, true);
        }
      else if (element_level < neighbor_level)
        {
          parent_neighbor = m_pMesh.getParent(neighbor, true);
        }
      bool fn = m_pMesh.is_node_neighbor(parent_element, parent_neighbor);
      return fn;
    }

    // deprecated
#if 0
    bool HangingNodeAdapterBase::min_max_neighbors_level(stk::mesh::Entity element, int min_max[2], RefineLevelType *refine_level, bool check_what[3])
    {
      min_max[0] = std::numeric_limits<int>::max();
      min_max[1] = 0;
      bool found_a_face_neighbor = false;

      std::set<stk::mesh::Entity> neighbors;
      m_pMesh.get_node_neighbors(element, neighbors);
      int *refine_level_elem = stk::mesh::field_data( *refine_level , element );
      for (std::set<stk::mesh::Entity>::iterator neighbor = neighbors.begin(); neighbor != neighbors.end(); ++neighbor)
        {
          int *refine_level_neigh = stk::mesh::field_data( *refine_level , *neighbor );
          bool fn = false;
          if (check_what[2])
            fn = fn || is_face_neighbor(element, refine_level_elem[0], *neighbor, refine_level_neigh[0]);
          if (check_what[1] && m_pMesh.get_spatial_dim() == 3)
            fn = fn || is_edge_neighbor(element, refine_level_elem[0], *neighbor, refine_level_neigh[0]);
          if (check_what[0])
            fn = fn || is_node_neighbor(element, refine_level_elem[0], *neighbor, refine_level_neigh[0]);

          if (fn)
            {
              min_max[0] = std::min(min_max[0], refine_level_neigh[0]);
              min_max[1] = std::max(min_max[1], refine_level_neigh[0]);
              found_a_face_neighbor = true;
            }
        }
      return found_a_face_neighbor;
    }
#endif

    void HangingNodeAdapterBase::get_neighbors(stk::mesh::Entity element, RefineLevelType *refine_level, bool get_what[3],
                                               std::set<stk::mesh::Entity>& selected_neighbors,
                                               const CellTopologyData * const bucket_topo_data
                                               )
    {
      static std::set<stk::mesh::Entity> neighbors;
      neighbors.clear();
      m_pMesh.get_node_neighbors(element, neighbors);
      //std::cout << "node_neighbors.size= " << neighbors.size() << std::endl;
      const int * const refine_level_elem = stk::mesh::field_data( *refine_level , element );
      const int refine_level_elem_0 = refine_level_elem[0];
      for (std::set<stk::mesh::Entity>::iterator neighbor = neighbors.begin(); neighbor != neighbors.end(); ++neighbor)
        {
          const int *const refine_level_neigh = stk::mesh::field_data( *refine_level , *neighbor );
          bool fn = false;
          if (get_what[2])
            {
              bool isfn = is_face_neighbor(element, refine_level_elem_0, *neighbor, refine_level_neigh[0], bucket_topo_data);
              fn = fn || isfn;
            }
          if (get_what[1] && m_pMesh.get_spatial_dim() == 3)
            fn = fn || is_edge_neighbor(element, refine_level_elem_0, *neighbor, refine_level_neigh[0], false, bucket_topo_data);
          if (get_what[0])
            fn = fn || is_node_neighbor(element, refine_level_elem_0, *neighbor, refine_level_neigh[0], false);

          if (fn)
            {
              selected_neighbors.insert(*neighbor);
            }
        }
    }

    bool HangingNodeAdapterBase::should_check(stk::mesh::Entity element, int refine_level_elem,
                                              stk::mesh::Entity neighbor, int refine_level_neighbor,
                                              bool get_what[3],
                                              const CellTopologyData * const bucket_topo_data
                                              )
    {
      if (get_what[2])
        {
          bool isfn = is_face_neighbor(element, refine_level_elem, neighbor, refine_level_neighbor, bucket_topo_data);
          if (isfn) return true;
        }
      if (get_what[1] && m_pMesh.get_spatial_dim() == 3)
        {
          bool isen = is_edge_neighbor(element, refine_level_elem, neighbor, refine_level_neighbor, false, bucket_topo_data);
          if (isen) return true;
        }
      if (get_what[0])
        {
          bool isnn = is_node_neighbor(element, refine_level_elem, neighbor, refine_level_neighbor, false);
          if (isnn) return true;
        }

      return false;
    }

    bool HangingNodeAdapterBase::check_two_to_one(bool check_what[3])
    {
      bool valid = true;
      RefineLevelType *refine_level = m_pMesh.get_refine_level_field();
      if (!refine_level)
        {
          throw std::logic_error("must have refine_level field for hanging-node refinement");
        }

      stk::mesh::Selector on_locally_owned_part =  ( m_pMesh.get_fem_meta_data()->locally_owned_part() );
      const stk::mesh::BucketVector & buckets = m_pMesh.get_bulk_data()->buckets( m_pMesh.element_rank() );

      for ( stk::mesh::BucketVector::const_iterator k = buckets.begin() ; k != buckets.end() ; ++k )
        {
          if (1 || on_locally_owned_part(**k))
            {
              stk::mesh::Bucket & bucket = **k ;
              const CellTopologyData * const bucket_topo_data = m_pMesh.get_cell_topology(bucket);
              const unsigned num_elements_in_bucket = bucket.size();
              for (unsigned iElement = 0; iElement < num_elements_in_bucket; iElement++)
                {
                  stk::mesh::Entity element = bucket[iElement];

                  if (hasChildren(element))
                    continue;

                  int *refine_level_elem = stk::mesh::field_data( *refine_level , element );
#if USE_BOOST_POOL_ALLOC
                  LocalSetType selected_neighbors;
#else
                  pool.Reset();
                  LocalSetType selected_neighbors(LocalSet::less(), &pool);
#endif
                  get_node_neighbors(element, selected_neighbors);

                  for (LocalSetType::iterator neighbor = selected_neighbors.begin();
                       neighbor != selected_neighbors.end(); ++neighbor)
                    {
                      if (hasChildren(*neighbor))
                        continue;

                      int *refine_level_neigh = stk::mesh::field_data( *refine_level , *neighbor );

                      bool do_check = false;
                      if (check_what[0])
                        {
                          do_check |= is_node_neighbor(element, refine_level_elem[0],
                                                       *neighbor, refine_level_neigh[0],
                                                       false);
                        }
                      if (check_what[1])
                        {
                          do_check |= is_edge_neighbor(element, refine_level_elem[0],
                                                       *neighbor, refine_level_neigh[0],
                                                       false, bucket_topo_data);
                        }
                      if (check_what[2])
                        {
                          do_check |= is_face_neighbor(element, refine_level_elem[0],
                                                       *neighbor, refine_level_neigh[0],
                                                       false, bucket_topo_data);
                        }
                      if (do_check && std::abs(refine_level_neigh[0] - refine_level_elem[0]) > 1)
                        {
                          if (m_debug_print)
                            std::cout << "check_two_to_one: invalid element (id,level)= ("
                                      << m_pMesh.identifier(element) << ", " << refine_level_elem[0]
                                      << ") due to neighbor: (" << m_pMesh.identifier(*neighbor)
                                      << ", " << refine_level_neigh[0] << ")"
                                      << std::endl;
                          valid = false;
                        }
                    }
                }
            }
        }
      return valid;

    }


  }


