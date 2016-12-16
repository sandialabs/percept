#if !defined(STK_BUILT_IN_SIERRA)
class RegressionTestRefineSideSet_tmp {};
#else

#include <stdexcept>
#include <sstream>
#include <vector>
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <typeinfo>

#include <math.h>
#if defined( STK_HAS_MPI )
#include <mpi.h>
#endif

#include <gtest/gtest.h>

#include <stk_util/environment/WallTime.hpp>
#include <stk_util/diag/PrintTable.hpp>

#include <Teuchos_ScalarTraits.hpp>

#include <gtest/gtest.h>
#include <boost/lexical_cast.hpp>
#include <stk_io/IossBridge.hpp>

#include <percept/Percept.hpp>
#include <percept/Util.hpp>
#include <percept/ExceptionWatch.hpp>

#include <percept/function/StringFunction.hpp>
#include <percept/function/FieldFunction.hpp>
#include <percept/function/ConstantFunction.hpp>
#include <percept/PerceptMesh.hpp>

#include <adapt/UniformRefinerPattern.hpp>
#include <adapt/UniformRefiner.hpp>
#include <adapt/RefinerUtil.hpp>
#include <adapt/UniformRefinerPattern_Tri3_Quad4_3.hpp>
#include <adapt/UniformRefinerPattern_Tet4_Hex8_4.hpp>
#include <adapt/UniformRefinerPattern_Wedge6_Hex8_6.hpp>
#include <unit_tests/TestLocalRefiner.hpp>
#include <adapt/sierra_element/StdMeshObjTopologies.hpp>
#include <adapt/sierra_element/RefinementTopology.hpp>
#include <percept/RunEnvironment.hpp>

#include <percept/fixtures/Fixture.hpp>
#include <percept/fixtures/BeamFixture.hpp>
#include <percept/fixtures/SingleTetFixture.hpp>
#include <percept/fixtures/TetWedgeFixture.hpp>
#include <percept/fixtures/HeterogeneousFixture.hpp>
#include <percept/fixtures/PyramidFixture.hpp>
#include <percept/fixtures/QuadFixture.hpp>
#include <percept/fixtures/WedgeFixture.hpp>

// smoothing tests
#include <percept/mesh/mod/smoother/MeshSmoother.hpp>
#include <percept/mesh/mod/smoother/ReferenceMeshSmootherConjugateGradient.hpp>

#include <adapt/UniformRefinerPattern_def.hpp>
#include <stk_mesh/base/MeshUtils.hpp>

#include "stk_unit_test_utils/ReadWriteSidesetTester.hpp"


  namespace percept
  {
    namespace regression_tests
    {

#include "RegressionTestFileLoc.hpp"

    const percept::Elem::RefinementTopology* get_refinement_topology(const CellTopologyData * const cellTopoData)
    {
        shards::CellTopology cellTopo(cellTopoData);
        percept::Elem::CellTopology elem_celltopo = percept::Elem::getBasicCellTopology(cellTopo.getName());
        const percept::Elem::RefinementTopology* ref_topo_p = Elem::getRefinementTopology(elem_celltopo);
        return ref_topo_p;
    }

    const percept::Elem::RefinementTopology* get_refinement_topology(stk::topology topology)
    {
        return get_refinement_topology(stk::mesh::get_cell_topology(topology).getCellTopologyData());
    }

    const percept::Elem::RefinementTopology* get_refinement_topology(percept::PerceptMesh &eMesh, stk::mesh::Entity element)
    {
        return get_refinement_topology(eMesh.get_cell_topology(element));
    }

    class MeshWithSideset {
    public:
        MeshWithSideset(const std::string &inputBaseName, bool delayMeshLoad = false) :
            m_baseName(inputBaseName),
            m_meta(3, get_entity_rank_names()),
            m_bulk(m_meta, MPI_COMM_WORLD),
            m_eMesh(&m_meta, &m_bulk, false),
            m_ioBroker(create_and_setup_io_broker_tester()),
            m_breakPattern(m_eMesh),
            m_isMeshLoaded(false)
        {
            if(false == delayMeshLoad)
                load_mesh_and_fill_sideset_data();
        }

        ~MeshWithSideset()
        {
            delete m_ioBroker;
        }

    private:
        std::vector<std::string> get_entity_rank_names()
        {
            std::vector<std::string> entityRankNames = stk::mesh::entity_rank_names();
            entityRankNames.push_back("FAMILY_TREE");
            return entityRankNames;
        }

        stk::unit_test_util::sideset::StkMeshIoBrokerTester *
        create_and_setup_io_broker_tester()
        {
            stk::unit_test_util::sideset::StkMeshIoBrokerTester *inputIoBroker = new stk::unit_test_util::sideset::StkMeshIoBrokerTester();
            inputIoBroker->property_add(Ioss::Property("DECOMPOSITION_METHOD", "RCB"));
            inputIoBroker->set_rank_name_vector(get_entity_rank_names());
            inputIoBroker->set_bulk_data(m_bulk);

            inputIoBroker->add_mesh_database(get_input_filename(), stk::io::READ_MESH);
            inputIoBroker->create_input_mesh();
            inputIoBroker->add_all_mesh_fields_as_input_fields();
            return inputIoBroker;
        }

        MeshWithSideset();
        MeshWithSideset(const MeshWithSideset&);

    private:
        std::string m_baseName;
        stk::mesh::MetaData m_meta;
        stk::unit_test_util::sideset::BulkDataTester m_bulk;
        percept::PerceptMesh m_eMesh;
        stk::unit_test_util::sideset::StkMeshIoBrokerTester *m_ioBroker = nullptr;
        stk::unit_test_util::sideset::SideSetData m_sidesetData;
        URP_Heterogeneous_3D m_breakPattern;
        bool m_isMeshLoaded;

    public:
        stk::unit_test_util::sideset::BulkDataTester &get_bulk() { return m_bulk; }
        stk::mesh::MetaData &get_meta() { return m_meta; }
        percept::PerceptMesh &get_mesh() { return m_eMesh; }
        stk::unit_test_util::sideset::SideSetData &get_input_sideset() { return m_sidesetData; }
        UniformRefinerPatternBase &get_breaker() { return m_breakPattern; }

        std::string get_input_filename()
        {
            std::string inputFilename(input_files_loc + m_baseName + ".e");
            return inputFilename;
        }

        std::string get_output_filename()
        {
            std::string outputFilename(output_files_loc + m_baseName + "_1.e");
            return outputFilename;
        }

        void load_mesh_and_fill_sideset_data()
        {
            if(m_isMeshLoaded) return;

            m_eMesh.add_field("proc_rank", stk::topology::ELEMENT_RANK, 0);
            m_ioBroker->populate_bulk_data();
            m_eMesh.setCoordinatesField();
            m_ioBroker->fill_sideset_data(m_sidesetData);

            m_isMeshLoaded = true;
        }

        void write_exodus_file()
        {
            ThrowRequire(m_isMeshLoaded);
            stk::unit_test_util::sideset::StkMeshIoBrokerTester outputIoBroker;
            outputIoBroker.set_bulk_data(m_bulk);
            size_t resultFileIndex = outputIoBroker.create_output_mesh(get_output_filename(), stk::io::WRITE_RESULTS);
            outputIoBroker.write_output_mesh(resultFileIndex);
        }

        void do_uniform_refinement(bool removeOldElements=true)
        {
            ThrowRequire(m_isMeshLoaded);
            UniformRefiner breaker(m_eMesh, m_breakPattern, m_eMesh.get_field(stk::topology::ELEMENT_RANK, "proc_rank"));
            breaker.setRemoveOldElements(removeOldElements);
            //breaker.setIgnoreSideSets(true);
            breaker.doBreak();
        }
    };

      struct ElemIdSide {
          int elem_id;
          int side_ordinal;
      };
      typedef std::vector<ElemIdSide> ElemIdSideVector;

      struct SideSetIdAndElemIdSides {
          int id;
          ElemIdSideVector sideSet;
      };
      typedef std::vector<SideSetIdAndElemIdSides> SideSetIdAndElemIdSidesVector;

      struct ElemIdSideLess {
        inline bool operator()(const ElemIdSide &lhs, const ElemIdSide &rhs) const
        {
            if(lhs.elem_id < rhs.elem_id)
                return true;
            else if(lhs.elem_id == rhs.elem_id)
                return lhs.side_ordinal < rhs.side_ordinal;
            else
                return false;
        }
        inline ElemIdSideLess& operator=(const ElemIdSideLess& rhs);
      };

      stk::mesh::SideSet get_stk_side_set(stk::mesh::BulkData &bulk, const ElemIdSideVector &ss)
      {
          stk::mesh::SideSet sideSet(ss.size());
          for(size_t i=0; i<ss.size(); i++)
              sideSet[i] = stk::mesh::SideSetEntry(bulk.get_entity(stk::topology::ELEM_RANK, ss[i].elem_id), ss[i].side_ordinal);

          return sideSet;
      }

      stk::unit_test_util::sideset::SideSetData get_stk_side_set_data(stk::mesh::BulkData &bulk, const SideSetIdAndElemIdSidesVector &ssData)
      {
          stk::unit_test_util::sideset::SideSetData sideSetData(ssData.size());

          for(size_t i=0; i<ssData.size(); i++)
          {
              sideSetData[i].id = ssData[i].id;
              sideSetData[i].sideSet = get_stk_side_set(bulk, ssData[i].sideSet);
          }
          return sideSetData;
      }

      void compare_sidesets(const std::string& inputFileName,
                            stk::mesh::BulkData &bulk,
                            const stk::unit_test_util::sideset::SideSetData &sidesetData,
                            const SideSetIdAndElemIdSidesVector &expected)
      {
          ASSERT_EQ(expected.size(), sidesetData.size()) << "for file: " << inputFileName;
          for(size_t ss=0; ss<sidesetData.size(); ++ss)
          {
              const stk::mesh::SideSet& sideSet = sidesetData[ss].sideSet;
              const std::vector<ElemIdSide>& expectedSideSet = expected[ss].sideSet;
              EXPECT_EQ(expected[ss].id, sidesetData[ss].id);
              ASSERT_EQ(expectedSideSet.size(), sideSet.size()) << "for file: " << inputFileName;

              for(size_t i=0;i<sideSet.size();++i)
              {
                  EXPECT_EQ(static_cast<stk::mesh::EntityId>(expectedSideSet[i].elem_id), bulk.identifier(sideSet[i].element)) << "for file: " << inputFileName;
                  EXPECT_EQ(expectedSideSet[i].side_ordinal, sideSet[i].side) << "for file: " << inputFileName;
              }
          }
      }

      void compare_sidesets(const std::string& input_file_name,
                            stk::mesh::BulkData &bulk,
                            const SideSetIdAndElemIdSidesVector &sideset,
                            const SideSetIdAndElemIdSidesVector &expected)
      {
          compare_sidesets(input_file_name, bulk, get_stk_side_set_data(bulk, sideset), expected);
      }

      void refine_mesh(const std::string &baseName,
                       const SideSetIdAndElemIdSidesVector& expectedInputSideset,
                       const SideSetIdAndElemIdSidesVector& expectedRefinedSideset)
      {
        bool delayMeshLoad = false;
        MeshWithSideset inputMesh(baseName, delayMeshLoad);

        stk::unit_test_util::sideset::BulkDataTester &bulk = inputMesh.get_bulk();
        stk::mesh::MetaData &meta = inputMesh.get_meta();

        compare_sidesets(inputMesh.get_input_filename(), bulk, inputMesh.get_input_sideset(), expectedInputSideset);

        inputMesh.do_uniform_refinement();

        for(const SideSetIdAndElemIdSides& sset : expectedRefinedSideset)
	{
	    stk::mesh::SideSet sideSet = get_stk_side_set(bulk, sset.sideSet);
            bulk.tester_save_sideset_data(sset.id, sideSet);
	}

        stk::mesh::EntityVector faces;
        stk::mesh::get_selected_entities(meta.locally_owned_part(), bulk.buckets(meta.side_rank()), faces);

        for(stk::mesh::Entity face : faces)
            ASSERT_EQ(2u, bulk.num_elements(face));

        inputMesh.write_exodus_file();

        stk::mesh::MetaData meta2;
        stk::unit_test_util::sideset::BulkDataTester bulk2(meta2, MPI_COMM_WORLD);
        stk::unit_test_util::sideset::SideSetData outputSidesetData = stk::unit_test_util::sideset::get_sideset_data_from_written_file(bulk2, inputMesh.get_output_filename());
        EXPECT_NO_THROW(compare_sidesets(inputMesh.get_output_filename(), bulk2, outputSidesetData, expectedRefinedSideset));
      }

      struct TestCase
      {
          std::string filename;
          SideSetIdAndElemIdSidesVector originalSideset;
          SideSetIdAndElemIdSidesVector refinedSideset;
      };

      void add_test_case(const std::string &baseName,
                         const SideSetIdAndElemIdSidesVector &original,
                         const SideSetIdAndElemIdSidesVector &refined,
                         std::vector<TestCase> &testCases)
      {
          // This function is necessary to get around intel compiler issues with seagull brace initialization
          testCases.push_back({baseName, original, refined});
      }

      TEST(regr_uniformRefiner, DISABLED_internal_sidesets)
      {
        stk::ParallelMachine pm = MPI_COMM_WORLD;

        const unsigned p_size = stk::parallel_machine_size(pm);

        std::vector<TestCase> testCases;
        add_test_case("ADA",
                      {{1, {{ 1, 5}, { 2, 4}                                                      }}},
                      {{1, {{23, 5}, {24, 5}, {25, 5}, {26, 5}, {27, 4}, {28, 4}, {29, 4}, {30, 4}}}},
                      testCases);
        add_test_case("ARA",
                      {{1, {{ 2, 4}                           }}},
                      {{1, {{27, 4}, {28, 4}, {29, 4}, {30, 4}}}},
                      testCases);
        add_test_case("ALA",
                      {{1, {{ 1, 5}                           }}},
                      {{1, {{23, 5}, {24, 5}, {25, 5}, {26, 5}}}},
                      testCases);
        add_test_case("ADe",
                      {{1, {{ 1, 5}, { 2, 1}                                                      }}},
                      {{1, {{23, 5}, {24, 5}, {25, 5}, {26, 5}, {85, 1}, {86, 1}, {87, 1}, {88, 1}}}},
                      testCases);
        add_test_case("ARe",
                      {{1, {{ 2, 1}                           }}},
                      {{1, {{85, 1}, {86, 1}, {87, 1}, {88, 1}}}},
                      testCases);
        add_test_case("ALe",
                      {{1, {{ 1, 5}                           }}},
                      {{1, {{23, 5}, {24, 5}, {25, 5}, {26, 5}}}},
                      testCases);

        std::vector<TestCase> testCasesP0;
        add_test_case("ADA",
                      {{1, {{ 1, 5}                           }}},
                      {{1, {{23, 5}, {24, 5}, {25, 5}, {26, 5}}}},
                      testCasesP0);
        add_test_case("ARA",
                      {{1, { }}},
                      {{1, { }}},
                      testCasesP0);
        add_test_case("ALA",
                      {{1, {{ 1, 5}                           }}},
                      {{1, {{23, 5}, {24, 5}, {25, 5}, {26, 5}}}},
                      testCasesP0);
        add_test_case("ADe",
                      {{1, {{ 1, 5}                           }}},
                      {{1, {{23, 5}, {24, 5}, {25, 5}, {26, 5}}}},
                      testCasesP0);
        add_test_case("ARe",
                      {{1, { }}},
                      {{1, { }}},
                      testCasesP0);
        add_test_case("ALe",
                      {{1, {{ 1, 5}                           }}},
                      {{1, {{23, 5}, {24, 5}, {25, 5}, {26, 5}}}},
                      testCasesP0);

        std::vector<TestCase> testCasesP1;
        add_test_case("ADA",
                      {{1, {{ 2, 4}                               }}},
                      {{1, {{27,  4}, {28,  4}, {29,  4}, {30,  4}}}},
                      testCasesP1);
        add_test_case("ARA",
                      {{1, {{ 2, 4}                               }}},
                      {{1, {{27,  4}, {28,  4}, {29,  4}, {30,  4}}}},
                      testCasesP1);
        add_test_case("ALA",
                      {{1, { }}},
                      {{1, { }}},
                      testCasesP1);
        add_test_case("ADe",
                      {{1, {{  2, 1}                              }}},
                      {{1, {{139, 1}, {140, 1}, {141, 1}, {142, 1}}}},
                      testCasesP1);
        add_test_case("ARe",
                      {{1, {{  2, 1}                              }}},
                      {{1, {{139, 1}, {140, 1}, {141, 1}, {142, 1}}}},
                      testCasesP1);
        add_test_case("ALe",
                      {{1, { }}},
                      {{1, { }}},
                      testCasesP1);

        std::vector<TestCase> runTheseCases;
        if(p_size == 1)
            runTheseCases = testCases;
        else if(stk::parallel_machine_rank(pm) == 0)
            runTheseCases = testCasesP0;
        else
            runTheseCases = testCasesP1;

        if (p_size <= 2) {
            for(const auto& test : runTheseCases)
                refine_mesh(test.filename, test. originalSideset, test.refinedSideset);
        }
      }

      void print_parent_child_sideset_connectivity(MeshWithSideset &inputMesh,
                                                   const ElemIdSideVector &parentSideSet,
                                                   const ElemIdSideVector &childSideSet)
      {
          std::ostringstream os;
          bool checkForFamilyTree = true;

          os << "On mesh(" << inputMesh.get_bulk().parallel_rank() << "): " << inputMesh.get_input_filename() << std::endl;
          percept::PerceptMesh &eMesh = inputMesh.get_mesh();

          for(const auto &entry : parentSideSet)
          {
              stk::mesh::Entity parent = eMesh.get_bulk_data()->get_entity(stk::topology::ELEMENT_RANK, entry.elem_id);

              if(eMesh.get_bulk_data()->is_valid(parent))
              {
                  os << "  For element: " << eMesh.get_bulk_data()->identifier(parent) << std::endl;

                  for(unsigned i=0; i<childSideSet.size(); ++i)
                  {
                      stk::mesh::Entity child = eMesh.get_bulk_data()->get_entity(stk::topology::ELEMENT_RANK, childSideSet[i].elem_id);
                      stk::mesh::Entity parentOfChild = eMesh.getParent(child, checkForFamilyTree);

                      if(parent == parentOfChild)
                          os << "    (child,ordinal) : (" << childSideSet[i].elem_id << "," << childSideSet[i].side_ordinal << ")" << std::endl;
                  }
              }
          }

          std::cerr << os.str() << std::endl;
      }

      ElemIdSideVector get_refined_child_sideset(MeshWithSideset &inputMesh,
                                                 const ElemIdSideVector &parentSideSet)
      {
          percept::PerceptMesh &eMesh = inputMesh.get_mesh();

          ElemIdSideVector childSideSet;
          const bool checkForFamilyTree = true;
          const bool onlyIfElementIsParentLeaf = false;

          for(const auto &entry : parentSideSet)
          {
              stk::mesh::Entity parent = eMesh.get_bulk_data()->get_entity(stk::topology::ELEMENT_RANK, entry.elem_id);

              EXPECT_TRUE(eMesh.get_bulk_data()->is_valid(parent));

              const percept::Elem::RefinementTopology* refTopo = get_refinement_topology(eMesh, parent);

              std::vector< std::pair<UInt,UInt> > refinedSideset = refTopo->get_children_on_ordinal(static_cast<UInt>(entry.side_ordinal));

              std::vector<stk::mesh::Entity> children;
              bool hasChildren = eMesh.getChildren( parent, children, checkForFamilyTree, onlyIfElementIsParentLeaf);

              EXPECT_EQ(hasChildren, (refinedSideset.size() > 0));

              if(hasChildren)
              {
                  EXPECT_GE(children.size(), refinedSideset.size());

                  for(unsigned i=0; i<refinedSideset.size(); ++i)
                  {
                      UInt childIndex = refinedSideset[i].first;
                      UInt faceIndex  = refinedSideset[i].second;

                      childSideSet.push_back({eMesh.get_bulk_data()->identifier(children[childIndex]), faceIndex});
                  }
              }
          }

          std::sort(childSideSet.begin(), childSideSet.end(), ElemIdSideLess());

          return childSideSet;
      }

      SideSetIdAndElemIdSidesVector
      get_refined_sideset_from_parent(MeshWithSideset &inputMesh,
                                      const SideSetIdAndElemIdSidesVector& parentSidesetData)
      {
          SideSetIdAndElemIdSidesVector refinedSideset;

          refinedSideset.resize(parentSidesetData.size());

          for(unsigned i=0; i<parentSidesetData.size(); ++i)
          {
              refinedSideset[i].id = parentSidesetData[i].id;
              refinedSideset[i].sideSet = get_refined_child_sideset(inputMesh, parentSidesetData[i].sideSet);
              print_parent_child_sideset_connectivity(inputMesh, parentSidesetData[i].sideSet, refinedSideset[i].sideSet);
          }

          return refinedSideset;
      }

      void test_output_sideset(MeshWithSideset &inputMesh,const SideSetIdAndElemIdSidesVector &refinedSideset)
      {
        inputMesh.write_exodus_file();

        stk::mesh::MetaData meta2;
        stk::unit_test_util::sideset::BulkDataTester bulk2(meta2, MPI_COMM_WORLD);
        stk::unit_test_util::sideset::SideSetData outputSidesetData = stk::unit_test_util::sideset::get_sideset_data_from_written_file(bulk2, inputMesh.get_output_filename());
        EXPECT_NO_THROW(compare_sidesets(inputMesh.get_output_filename(), bulk2, outputSidesetData, refinedSideset));
      }

      void test_refine_sideset(MeshWithSideset &inputMesh,
                               stk::mesh::PartVector &parts,
                               const SideSetIdAndElemIdSidesVector& originalSideset,
                               const SideSetIdAndElemIdSidesVector& expectedRefinedSideset)
      {
        stk::unit_test_util::sideset::BulkDataTester &bulk = inputMesh.get_bulk();
        stk::mesh::MetaData &meta = inputMesh.get_meta();

        bool removeOldElements = false;
        inputMesh.do_uniform_refinement(removeOldElements);

        bulk.initialize_face_adjacent_element_graph();

        stk::mesh::EntityVector faces;
        stk::mesh::get_selected_entities(meta.locally_owned_part(), bulk.buckets(meta.side_rank()), faces);

        ASSERT_EQ(0u, faces.size());

        SideSetIdAndElemIdSidesVector refinedSideset = get_refined_sideset_from_parent(inputMesh, originalSideset);
        EXPECT_NO_THROW(compare_sidesets(inputMesh.get_input_filename(), bulk, expectedRefinedSideset, refinedSideset));

        for(const SideSetIdAndElemIdSides& sset : refinedSideset)
        {
            stk::mesh::SideSet sideSet = get_stk_side_set(bulk, sset.sideSet);
            bulk.tester_save_sideset_data(sset.id, sideSet);
            bulk.create_side_entities(sideSet, parts);
        }

        stk::mesh::get_selected_entities(meta.locally_owned_part(), bulk.buckets(meta.side_rank()), faces);

        for(stk::mesh::Entity face : faces)
            ASSERT_EQ(2u, bulk.num_elements(face));

        test_output_sideset(inputMesh, refinedSideset);
      }

      void refine_sideset(const std::string &base_name,
                          const SideSetIdAndElemIdSidesVector& originalSideset,
                          const SideSetIdAndElemIdSidesVector& expectedRefinedSideset)
      {
        bool delayMeshLoad = true;
        MeshWithSideset inputMesh(base_name, delayMeshLoad);

        stk::mesh::MetaData &meta = inputMesh.get_meta();

        stk::mesh::PartVector parts;
        for(const SideSetIdAndElemIdSides &sideSet : originalSideset)
        {
            std::ostringstream os;
            os << "surface_" << sideSet.id;

            stk::mesh::Part &part = meta.declare_part(os.str(), meta.side_rank());
            meta.set_part_id(part, sideSet.id);
            stk::io::put_io_part_attribute(part);
            parts.push_back(&part);
        }

        inputMesh.load_mesh_and_fill_sideset_data();

        EXPECT_TRUE(inputMesh.get_input_sideset().empty());
        test_refine_sideset(inputMesh, parts, originalSideset, expectedRefinedSideset);
      }

      TEST(regr_uniformRefiner, get_refined_sideset)
      {
        stk::ParallelMachine pm = MPI_COMM_WORLD;

        const unsigned p_size = stk::parallel_machine_size(pm);

        std::vector<TestCase> testCases;
        add_test_case("AA",
                      {{1, {{ 1, 5}, { 2, 4}                                                      }}},
                      {{1, {{23, 5}, {24, 5}, {25, 5}, {26, 5}, {27, 4}, {28, 4}, {29, 4}, {30, 4}}}},
                      testCases);
        add_test_case("Ae",
                      {{1, {{ 1, 5}, { 2, 1}                                                      }}},
                      {{1, {{23, 5}, {24, 5}, {25, 5}, {26, 5}, {85, 1}, {86, 1}, {87, 1}, {88, 1}}}},
                      testCases);

        std::vector<TestCase> testCasesP0;
        add_test_case("AA",
                      {{1, {{ 1, 5}                           }}},
                      {{1, {{23, 5}, {24, 5}, {25, 5}, {26, 5}}}},
                      testCasesP0);
        add_test_case("Ae",
                      {{1, {{ 1, 5}                           }}},
                      {{1, {{23, 5}, {24, 5}, {25, 5}, {26, 5}}}},
                      testCasesP0);

        std::vector<TestCase> testCasesP1;
        add_test_case("AA",
                      {{1, {{ 2, 4}                               }}},
                      {{1, {{27,  4}, {28,  4}, {29,  4}, {30,  4}}}},
                      testCasesP1);
        add_test_case("Ae",
                      {{1, {{  2, 1}                              }}},
                      {{1, {{139, 1}, {140, 1}, {141, 1}, {142, 1}}}},
                      testCasesP1);

        std::vector<TestCase> runTheseCases;
        if(p_size == 1)
            runTheseCases = testCases;
        else if(stk::parallel_machine_rank(pm) == 0)
            runTheseCases = testCasesP0;
        else
            runTheseCases = testCasesP1;

        if (p_size <= 2) {
            for(const auto& test : runTheseCases)
                refine_sideset(test.filename, test.originalSideset, test.refinedSideset);
        }
      }

    }//    namespace regression_tests
  }//  namespace percept

#endif