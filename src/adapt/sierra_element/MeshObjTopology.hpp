// Copyright 2014 Sandia Corporation. Under the terms of
// Contract DE-AC04-94AL85000 with Sandia Corporation, the
// U.S. Government retains certain rights in this software.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.


#ifndef adapt_sierra_element_MeshObjTopology_hpp
#define adapt_sierra_element_MeshObjTopology_hpp

#include <adapt/sierra_element/CellTopology.hpp>

  namespace percept {
    namespace Elem {

      class MeshObjTopology
      {
      public:
        MeshObjTopology(
                        const CellTopologyData *      cell_topology_data)
          : m_cellTopology(cell_topology_data)
        {}
  
      public:
        ~MeshObjTopology()
        {}
      
        Elem::CellTopology getCellTopology() const {
          return m_cellTopology;
        }
  
      protected:
        Elem::CellTopology                    m_cellTopology;
  
      private:
        MeshObjTopology();
        MeshObjTopology(const MeshObjTopology &);
        MeshObjTopology & operator = (const MeshObjTopology &);
      };

    } // namespace Elem
  } // namespace percept

#endif // adapt_sierra_element_MeshObjTopology_hpp
