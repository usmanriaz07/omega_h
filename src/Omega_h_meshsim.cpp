#include "Omega_h_file.hpp"

#include "Omega_h_build.hpp"
#include "Omega_h_class.hpp"
#include "Omega_h_element.hpp"
#include "Omega_h_map.hpp"
#include "Omega_h_vector.hpp"
#include "Omega_h_mesh.hpp"
#include "Omega_h_for.hpp"
#include "Omega_h_adj.hpp"

#include "SimPartitionedMesh.h"
#include "SimModel.h"
#include "SimUtil.h"
#include "SimDiscrete.h"
#include "MeshSim.h"
#include "SimMessages.h"
#include "SimError.h"
#include "SimErrorCodes.h"
#include "SimMeshingErrorCodes.h"
#include "SimDiscreteErrorCodes.h"

using namespace std;

namespace Omega_h {

namespace meshsim {

namespace {

/*
int classId(pEntity e) {
  pGEntity g = EN_whatIn(e);
  assert(g);
  return GEN_tag(g);
}
*/

//void read_internal(pMesh m, Mesh* mesh) {//use this for user generated mesh
void read_internal(pParMesh sm, Mesh* mesh) {
  pMesh m = PM_mesh(sm, 0);
  (void)mesh;
  const int numVtx = M_numVertices(m);
  const int numEdges = M_numEdges(m);
  const int numFaces = M_numFaces(m);
  const int numRegions = M_numRegions(m);

/*
  Int max_dim;
  if (numRegions(m)) {
    max_dim = 3;
  } else if (numFaces(m)) {
    max_dim = 2;
  } else if (numEdges(m)) {
    max_dim = 1;
  } else {
    Omega_h_fail("There were no Elements of dimension higher than zero!\n");
  }
*/
  std::vector<int> down_adjs[10];
  //std::vector<int> ent_class_ids[10];
  //allocate space for the requirement based topo type ids
    //this will only be required if EN_id returns global per_mesh ids
    //as opposed to per_dimension EN_ids
  //const int numEntities = numVtx + numEdges + numFaces + numRegions;
  //std::vector<int> Topo_type_ids;
  //Topo_type_ids.reserve(numEntities);

/*
  //write vertex coords into node_coords and vertex ids into down_adjs
  down_adjs[0].reserve(numVtx);
  ent_class_ids[0].reserve(numVtx);
  HostWrite<Real> host_coords(numVtx*max_dim);
  VIter vertices = M_vertexIter(m);
  pVertex vtx;
  i = 0;
  while ((vtx = (pVertex) VIter_next(vertices))) {
    double xyz[3];
    V_coord(vtx,xyz);
    if( max_dim < 3 && xyz[2] != 0 )
      Omega_h_fail("The z coordinate must be zero for a 2d mesh!\n");
    for(int j=0; j<max_dim; j++) {
      host_coords[i * max_dim + j] = xyz[j];
    }
    down_adjs[0].push_back(EN_id(vtx));
    ent_class_ids[0].push_back(classId(vtx));
    ++i;
  }
  VIter_delete(vertices);
*/

  //get the ids of vertices bounding each edge
  down_adjs[1].reserve(numEdges*2);
  //ent_class_ids[1].reserve(numEdges);
  EIter edges = M_edgeIter(m);
  pEdge edge;
  pVertex vtx;
  int count_edge = 0;
  //printf(" ok1.4.2 \n");
  double xyz[3];//stores vtx coords
  while ((edge = (pEdge) EIter_next(edges))) {
    //Topo_type_ids[EN_id(edge)] = count_edge;
    count_edge += 1;
    //printf("edge EN_id is=%d\n", EN_id(edge));
    for(int j=0; j<2; ++j) {
      vtx = E_vertex(edge,j);
      down_adjs[1].push_back(EN_id(vtx));
      V_coord(vtx,xyz);
      //printf("vtx EN_id is=%d, x=%f, y=%f, z=%f\n", EN_id(vtx), xyz[0], xyz[1], xyz[2]);
    }
    //ent_class_ids[1].push_back(classId(edge));
  }
  EIter_delete(edges);
  //printf(" ok1.4.3 \n");

  //pass vectors to set_ents
  auto deg = element_degree(Topo_type::edge, Topo_type::vertex);
  //printf("deg e2v=%d \n", deg);
  
/*
  //below degree values check out
  deg = element_degree(Topo_type::tetrahedron, Topo_type::vertex);
  printf("deg tet2v=%d \n", deg);
  deg = element_degree(Topo_type::hexahedron, Topo_type::quadrilateral);
  printf("deg hex2quad=%d \n", deg);
  deg = element_degree(Topo_type::wedge, Topo_type::triangle);
  printf("deg wed2tri=%d \n", deg);
  deg = element_degree(Topo_type::pyramid, Topo_type::edge);
  printf("deg py2ed=%d \n", deg);
  deg = element_degree(Topo_type::pyramid, Topo_type::quadrilateral);
  printf("deg py2quad=%d \n", deg);
*/
  //set Verts of mesh
  mesh->set_verts_type(numVtx);
  //
  HostWrite<LO> host_e2v(numEdges*deg);
  for (Int i = 0; i < numEdges; ++i) {
    for (Int j = 0; j < deg; ++j) {
      host_e2v[i*deg + j] =
          down_adjs[1][static_cast<std::size_t>(i*deg + j)];
    }
  }
  auto ev2v = Read<LO>(host_e2v.write()); //This is LOs
  mesh->set_ents(Topo_type::edge, Topo_type::vertex, Adj(ev2v));
  // when to_entity is vertex, codes should not exist as per mesh.c L367

  //get the ids of edges bounding each triangle
  //get the ids of edges bounding each quadrilateral
  FIter faces = M_faceIter(m);
  pFace face;
  //printf(" ok1.4.4 \n");
  //count no. of tris and quads
  int count_tri = 0;
  int count_quad = 0;
  //get ids of tris and quads from faceIDs
  std::vector<int> face_type_ids;
  face_type_ids.reserve(numFaces);
  while (face = (pFace) FIter_next(faces)) {
    if (F_numEdges(face) == 3) {
      //get ids of tris
      face_type_ids[EN_id(face)] = count_tri;
      //Topo_type_ids[EN_id(face)] = count_tri;
      //increment for next tri
      count_tri += 1;
    }
    else if (F_numEdges(face) == 4) {
      //get ids of quads
      face_type_ids[EN_id(face)] = count_quad;
      //Topo_type_ids[EN_id(face)] = count_quad;
      //increment for next quad
      count_quad += 1;
    }
    else {
      Omega_h_fail ("Face is neither tri nor quad \n");
    }
  }
  FIter_delete(faces);

  //printf(" ok1.4.5 \n");
  //allocate memory for t2e and q2e
  down_adjs[2].reserve(count_tri*3);
  down_adjs[3].reserve(count_quad*4);
  //
  
  //iterate and populate resp. edge ids
  faces = M_faceIter(m);
  while (face = (pFace) FIter_next(faces)) {
    //printf("face entity id=%d\n", EN_id(face));
    if (F_numEdges(face) == 3) {
      //printf ("is tri\n");
      pEdge tri_edge;
      pPList tri_edges = F_edges(face,1,0);
      assert (PList_size(tri_edges) == 3);
      void *iter = 0; // must initialize to 0
      while (tri_edge = (pEdge) PList_next(tri_edges, &iter)) {
        down_adjs[2].push_back(EN_id(tri_edge));
        //printf("adjacent edge id=%d\n", EN_id(tri_edge));
        //down_adjs[2].push_back(Topo_type_ids[EN_id(tri_edge)]);
      }
      PList_delete(tri_edges);
    }
    else if (F_numEdges(face) == 4) {
      //printf ("is quad\n");
      pEdge quad_edge;
      pPList quad_edges = F_edges(face,1,0);
      assert (PList_size(quad_edges) == 4);
      void *iter = 0; // must initialize to 0
      while (quad_edge = (pEdge) PList_next(quad_edges, &iter)) {
        down_adjs[3].push_back(EN_id(quad_edge));
        //printf("adjacent edge id=%d\n", EN_id(quad_edge));
        //down_adjs[3].push_back(Topo_type_ids[EN_id(quad_edge)]);
      }
      PList_delete(quad_edges);
    }
    else {
      Omega_h_fail ("Face is neither tri nor quad \n");
    }
  }
  FIter_delete(faces);
  //set ents at end
  //printf(" ok1.4.6 \n");

  //pass vectors to set_ents
  HostWrite<LO> host_t2e(count_tri*3);
  for (Int i = 0; i < count_tri; ++i) {
    for (Int j = 0; j < 3; ++j) {
      host_t2e[i*3 + j] =
          down_adjs[2][static_cast<std::size_t>(i*3 + j)];
    }
  }
  auto te2e = Read<LO>(host_t2e.write()); //This is LOs
  deg = element_degree(Topo_type::triangle, Topo_type::edge);
  //auto t2e_codes = get_codes_to_canonical(deg, te2e);
  mesh->set_ents(Topo_type::triangle, Topo_type::edge, Adj(te2e));
  //mesh->set_ents(Topo_type::triangle, Topo_type::edge, Adj(te2e, t2e_codes));

  HostWrite<LO> host_q2e(count_quad*4);
  for (Int i = 0; i < count_quad; ++i) {
    for (Int j = 0; j < 4; ++j) {
      host_q2e[i*4 + j] =
          down_adjs[3][static_cast<std::size_t>(i*4 + j)];
    }
  }
  auto q2e = Read<LO>(host_q2e.write()); //This is LOs
  mesh->set_ents(Topo_type::quadrilateral, Topo_type::edge, Adj(q2e));

  //get the ids of tris bounding each tet
  //get the ids of quads bounding each hex
  //get the ids of tris bounding each wedge
  //get the ids of quads bounding each wedge
  //get the ids of tris bounding each pyramid
  //get the ids of quads bounding each pyramid
  RIter regions = M_regionIter(m);
  //count tets, hexs, wedges and pyramids
  LO count_tet = 0;
  LO count_hex = 0;
  LO count_wedge = 0;
  LO count_pyramid = 0;
  pRegion rgn;
  while (rgn = (pRegion) RIter_next(regions)) {
    if (R_topoType(rgn) == Rtet) {
      count_tet += 1;
    }
    else if (R_topoType(rgn) == Rhex) {
      count_hex += 1;
    }
    else if (R_topoType(rgn) == Rwedge) {
      count_wedge += 1;
    }
    else if (R_topoType(rgn) == Rpyramid) {
      count_pyramid += 1;
    }
    else {
      Omega_h_fail("Region is not tet, hex, wedge, or pyramid \n");
    }
  }
  RIter_delete(regions);
  //

  //allocate memory for t2t. h2q, w2t, w2q, p2t, p2q
  down_adjs[4].reserve(count_tet*4);
  down_adjs[5].reserve(count_hex*6);
  down_adjs[6].reserve(count_wedge*2);//tris
  down_adjs[7].reserve(count_wedge*3);
  down_adjs[8].reserve(count_pyramid*4);//tris
  down_adjs[9].reserve(count_pyramid);
  //
  //printf(" ok1.4.7 \n");
  printf("tet=%d, hex=%d, wedge=%d, pyramid=%d\n",
         count_tet, count_hex, count_wedge, count_pyramid);
  
  //iterate and populate resp. face ids
  regions = M_regionIter(m);
  while ((rgn = (pRegion) RIter_next(regions))) {
    //printf("region entity id=%d\n", EN_id(rgn));
    //Tets
    if (R_topoType(rgn) == Rtet) {
      //printf("is tet\n");
      pFace tri;
      pPList tris = R_faces(rgn,1);
      assert (PList_size(tris) == 4);
      void *iter = 0; // must initialize to 0
      while (tri = (pFace) PList_next(tris, &iter))
        down_adjs[4].push_back(face_type_ids[EN_id(tri)]);
        //down_adjs[4].push_back(Topo_type_ids[EN_id(tri)]);
      PList_delete(tris);
    }
    //Hexs
    else if (R_topoType(rgn) == Rhex) {
      //printf("is hex\n");
      pFace quad;
      pPList quads = R_faces(rgn,1); 
      assert (PList_size(quads) == 6);
      void *iter = 0; // must initialize to 0
      while (quad = (pFace) PList_next(quads, &iter))
        down_adjs[5].push_back(face_type_ids[EN_id(quad)]);
        //down_adjs[5].push_back(Topo_type_ids[EN_id(quad)]);
      PList_delete(quads);
    }
    //Wedges
    else if (R_topoType(rgn) == Rwedge) {
      //printf("is wedge\n");
      pFace w_face;
      pPList w_faces = R_faces(rgn,1);
      assert (PList_size(w_faces) == 5);
      void *iter = 0; // must initialize to 0
      while (w_face = (pFace) PList_next(w_faces, &iter)) {
        if (F_numEdges(w_face) == 3) { //face is tri 
          down_adjs[6].push_back(face_type_ids[EN_id(w_face)]);
          //down_adjs[6].push_back(Topo_type_ids[EN_id(w_face)]);
	}
        else { //face is quad
          down_adjs[7].push_back(face_type_ids[EN_id(w_face)]);
          //down_adjs[7].push_back(Topo_type_ids[EN_id(w_face)]);
	}
      }
      PList_delete(w_faces);
    }
    //Pyramids
    else if (R_topoType(rgn) == Rpyramid) {
      //printf("is pyramid\n");
      pFace p_face;
      pPList p_faces = R_faces(rgn,1);
      assert (PList_size(p_faces) == 5);
      void *iter = 0; // must initialize to 0
      while (p_face = (pFace) PList_next(p_faces, &iter)) {
        if (F_numEdges(p_face) == 3) { //face is tri
          down_adjs[8].push_back(face_type_ids[EN_id(p_face)]);
        }
        else { // face is quad
          down_adjs[9].push_back(face_type_ids[EN_id(p_face)]);
        }
      }
      PList_delete(p_faces);
    }
    else {
      Omega_h_fail ("Region is not tet, hex, wedge, or pyramid \n");
    }
  }
  RIter_delete(regions);
  //printf(" ok1.4.8 \n");

  //
  //pass vectors to set_ents
  HostWrite<LO> host_tet2tr(count_tet*4);
  for (Int i = 0; i < count_tet; ++i) {
    for (Int j = 0; j < 4; ++j) {
      host_tet2tr[i*4 + j] =
          down_adjs[4][static_cast<std::size_t>(i*4 + j)];
    }
  }
  auto tet2tr = Read<LO>(host_tet2tr.write()); //This is LOs
  mesh->set_ents(Topo_type::tetrahedron, Topo_type::triangle, Adj(tet2tr));
  
  //printf(" ok1.4.8.1 \n");
  HostWrite<LO> host_hex2q(count_hex*6);
  for (Int i = 0; i < count_hex; ++i) {
    for (Int j = 0; j < 6; ++j) {
  //printf(" i=%d, j=%d, count_hex=%d\n", i,j,count_hex);
      host_hex2q[i*6 + j] =
          down_adjs[5][static_cast<std::size_t>(i*6 + j)];
    }
  }
  //printf(" ok1.4.8.1.1 \n");
  auto hex2q = Read<LO>(host_hex2q.write()); //This is LOs
  //printf(" ok1.4.8.1.2 \n");
  mesh->set_ents(Topo_type::hexahedron, Topo_type::quadrilateral, Adj(hex2q));
  
  //printf(" ok1.4.8.2 \n");
  HostWrite<LO> host_wedge2tri(count_wedge*2);
  for (Int i = 0; i < count_wedge; ++i) {
    for (Int j = 0; j < 2; ++j) {
      host_wedge2tri[i*2 + j] =
          down_adjs[6][static_cast<std::size_t>(i*2 + j)];
    }
  }
  auto wedge2tri = Read<LO>(host_wedge2tri.write()); //This is LOs
  mesh->set_ents(Topo_type::wedge, Topo_type::triangle, Adj(wedge2tri));
  
  //printf(" ok1.4.8.3 \n");
  HostWrite<LO> host_wedge2quad(count_wedge*3);
  for (Int i = 0; i < count_wedge; ++i) {
    for (Int j = 0; j < 3; ++j) {
      host_wedge2quad[i*3 + j] =
          down_adjs[7][static_cast<std::size_t>(i*3 + j)];
    }
  }
  auto wedge2quad = Read<LO>(host_wedge2quad.write()); //This is LOs
  mesh->set_ents(Topo_type::wedge, Topo_type::quadrilateral, Adj(wedge2quad));
  
  //printf(" ok1.4.8.2 \n");
  HostWrite<LO> host_pyramid2tri(count_pyramid*4);
  for (Int i = 0; i < count_pyramid; ++i) {
    for (Int j = 0; j < 4; ++j) {
      host_pyramid2tri[i*4 + j] =
          down_adjs[8][static_cast<std::size_t>(i*4 + j)];
    }
  }
  auto pyramid2tri = Read<LO>(host_pyramid2tri.write()); //This is LOs
  mesh->set_ents(Topo_type::pyramid, Topo_type::triangle, Adj(pyramid2tri));
  
  HostWrite<LO> host_pyramid2quad(count_pyramid);
  for (Int i = 0; i < count_pyramid; ++i) {
    for (Int j = 0; j < 1; ++j) {
      host_pyramid2quad[i*1 + j] =
          down_adjs[9][static_cast<std::size_t>(i*1 + j)];
    }
  }
  auto pyramid2quad = Read<LO>(host_pyramid2quad.write()); //This is LOs
  mesh->set_ents(Topo_type::pyramid, Topo_type::quadrilateral, Adj(pyramid2quad));
  //printf(" ok1.4.9 \n");

/*
  //print contents of input e2v given to omega-> o/p checks out
  for (std::vector<std::vector<int>>::size_type i = 1; i < 10; i++) {
    printf("from type %lu \n ", i);
    for (std::vector<int>::size_type j = 0; j < down_adjs[i].size(); j++) {
     std::cout << down_adjs[i][j] << ' ';
    }
    std::cout << std::endl;
  }
*/

  //test API call for 1 lvl dwn adj
  auto edge2vert = mesh->get_adj(Topo_type::edge, Topo_type::vertex).ab2b;
  auto edg2v = Write<LO> (edge2vert.size());
  auto print_call0= OMEGA_H_LAMBDA(LO i) {
    edg2v[i] = edge2vert[i];
  };
  parallel_for(edge2vert.size(), print_call0);
  //when array size>32, device to host copy and print
  //printf("edge2vert returned from get_adj is\n");
  auto host_edg2v = HostWrite<LO>(edg2v);
  for (int i=0; i<host_edg2v.size(); ++i) {
    //printf(" %d", host_edg2v[i]);
  };
  printf("\n");

  auto vert2edge = mesh->ask_up(Topo_type::vertex, Topo_type::edge).ab2b;
  auto v2edg = Write<LO> (vert2edge.size());
  auto print_call1 = OMEGA_H_LAMBDA(LO i) {
    v2edg[i] = vert2edge[i];
  };
  parallel_for(vert2edge.size(), print_call1);
  //printf("vert2edge lists returned from ask_up is\n");
  auto host_v2edg = HostWrite<LO>(v2edg);
  for (int i=0; i<host_v2edg.size(); ++i) {
    //printf(" %d", host_v2edg[i]);
  };
  printf("\n");
  OMEGA_H_CHECK(vert2edge.size() == 2*numEdges);

  auto vert2edge_o = mesh->ask_up(Topo_type::vertex, Topo_type::edge).a2ab;
  auto v2edg_o = Write<LO> (vert2edge_o.size());
  auto print_call10 = OMEGA_H_LAMBDA(LO i) {
    v2edg_o[i] = vert2edge_o[i];
  };
  parallel_for(vert2edge_o.size(), print_call10);
  //printf("vert2edge offsets returned from ask_up is\n");
  auto host_v2edg_o = HostWrite<LO>(v2edg_o);
  for (int i=0; i<host_v2edg_o.size(); ++i) {
    //printf(" %d", host_v2edg_o[i]);
  };
  printf("\n");
  OMEGA_H_CHECK(vert2edge_o.size() == numVtx+1);
  //
  //test API
  auto tri2edge = mesh->get_adj(Topo_type::triangle, Topo_type::edge);
  //printf("tri2edge\n");
  auto quad2edge = mesh->get_adj(Topo_type::quadrilateral, Topo_type::edge);
  //printf("quad2edge\n");

  auto edge2tri = mesh->ask_up(Topo_type::edge, Topo_type::triangle);
  //printf("edge2tri lists returned from ask_up is\n");
  auto print_call2 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", edge2tri.ab2b[i]);
  };
  parallel_for(edge2tri.ab2b.size(), print_call2);
  printf("\n");
  OMEGA_H_CHECK(edge2tri.ab2b.size() == 3*count_tri);
  //printf("edge2tri offsets returned from ask_up is\n");
  auto print_call2p0 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", edge2tri.a2ab[i]);
  };
  parallel_for(edge2tri.a2ab.size(), print_call2p0);
  printf("\n");
  OMEGA_H_CHECK(edge2tri.a2ab.size() == numEdges+1);

  auto edge2quad = mesh->ask_up(Topo_type::edge, Topo_type::quadrilateral);
  //printf("edge2quad lists returned from ask_up is\n");
  auto print_call3 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", edge2quad.ab2b[i]);
  };
  parallel_for(edge2quad.ab2b.size(), print_call3);
  printf("\n");
  //printf("edge2quad offsets returned from ask_up is\n");
  auto print_call3p0 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", edge2quad.a2ab[i]);
  };
  parallel_for(edge2quad.a2ab.size(), print_call3p0);
  printf("\n");
  OMEGA_H_CHECK(edge2quad.ab2b.size() == 4*count_quad);
  OMEGA_H_CHECK(edge2quad.a2ab.size() == numEdges+1);
  //
  //test API
  auto tet2tri = mesh->get_adj(Topo_type::tetrahedron, Topo_type::triangle);
  auto hex2quad = mesh->get_adj(Topo_type::hexahedron, Topo_type::quadrilateral);
  auto wedge2tria = mesh->get_adj(Topo_type::wedge, Topo_type::triangle);
  auto wedge2quadr = mesh->get_adj(Topo_type::wedge, Topo_type::quadrilateral);
  auto pyramid2tria = mesh->get_adj(Topo_type::pyramid, Topo_type::triangle);
  auto pyramid2quadr = mesh->get_adj(Topo_type::pyramid, Topo_type::quadrilateral);

  auto tri2tet = mesh->ask_up(Topo_type::triangle, Topo_type::tetrahedron);
  //printf("tri2tet list returned from ask_up is\n");
  auto print_call4 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", tri2tet.ab2b[i]);
  };
  parallel_for(tri2tet.ab2b.size(), print_call4);
  printf("\n");
  //printf("tri2tet offset returned from ask_up is\n");
  auto print_call4p0 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", tri2tet.a2ab[i]);
  };
  parallel_for(tri2tet.a2ab.size(), print_call4p0);
  printf("\n");
  OMEGA_H_CHECK(tri2tet.ab2b.size() == 4*count_tet);
  OMEGA_H_CHECK(tri2tet.a2ab.size() == count_tri+1);

  auto quad2hex = mesh->ask_up(Topo_type::quadrilateral, Topo_type::hexahedron);
  //printf("quad2hex list returned from ask_up is\n");
  auto print_call5 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", quad2hex.ab2b[i]);
  };
  parallel_for(quad2hex.ab2b.size(), print_call5);
  printf("\n");
  //printf("quad2hex offset returned from ask_up is\n");
  auto print_call5p0 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", quad2hex.a2ab[i]);
  };
  parallel_for(quad2hex.a2ab.size(), print_call5p0);
  printf("\n");
  OMEGA_H_CHECK(quad2hex.ab2b.size() == count_hex*6);
  OMEGA_H_CHECK(quad2hex.a2ab.size() == count_quad+1);

  auto tri2wedge = mesh->ask_up(Topo_type::triangle, Topo_type::wedge);
  //printf("tri2wedge list returned from ask_up is\n");
  auto print_call6 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", tri2wedge.ab2b[i]);
  };
  parallel_for(tri2wedge.ab2b.size(), print_call6);
  printf("\n");
  //printf("tri2wedge offset returned from ask_up is\n");
  auto print_call6p0 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", tri2wedge.a2ab[i]);
  };
  parallel_for(tri2wedge.a2ab.size(), print_call6p0);
  printf("\n");
  OMEGA_H_CHECK(tri2wedge.ab2b.size() == 2*count_wedge);
  OMEGA_H_CHECK(tri2wedge.a2ab.size() == count_tri+1);

  auto quad2wedge = mesh->ask_up(Topo_type::quadrilateral, Topo_type::wedge);
  //printf("quad2wedge list returned from ask_up is\n");
  auto print_call7 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", quad2wedge.ab2b[i]);
  };
  parallel_for(quad2wedge.ab2b.size(), print_call7);
  printf("\n");
  //printf("quad2wedge offset returned from ask_up is\n");
  auto print_call7p0 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", quad2wedge.a2ab[i]);
  };
  parallel_for(quad2wedge.a2ab.size(), print_call7p0);
  printf("\n");
  OMEGA_H_CHECK(quad2wedge.ab2b.size() == count_wedge*3);
  OMEGA_H_CHECK(quad2wedge.a2ab.size() == count_quad+1);

  auto tri2pyramid = mesh->ask_up(Topo_type::triangle, Topo_type::pyramid);
  //printf("tri2pyramid list returned from ask_up is\n");
  auto print_call8 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", tri2pyramid.ab2b[i]);
  };
  parallel_for(tri2pyramid.ab2b.size(), print_call8);
  printf("\n");
  //printf("tri2pyramid offset returned from ask_up is\n");
  auto print_call8p0 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", tri2pyramid.a2ab[i]);
  };
  parallel_for(tri2pyramid.a2ab.size(), print_call8p0);
  printf("\n");
  OMEGA_H_CHECK(tri2pyramid.ab2b.size() == 4*count_pyramid);
  OMEGA_H_CHECK(tri2pyramid.a2ab.size() == count_tri+1);

  auto quad2pyramid = mesh->ask_up(Topo_type::quadrilateral, Topo_type::pyramid);
  //printf("quad2pyramid list returned from ask_up is\n");
  auto print_call9 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", quad2pyramid.ab2b[i]);
  };
  parallel_for(quad2pyramid.ab2b.size(), print_call9);
  printf("\n");
  //printf("quad2pyramid offset returned from ask_up is\n");
  auto print_call9p0 = OMEGA_H_LAMBDA(LO i) {
    //printf(" %d", quad2pyramid.a2ab[i]);
  };
  parallel_for(quad2pyramid.a2ab.size(), print_call9p0);
  printf("\n");
  OMEGA_H_CHECK(quad2pyramid.ab2b.size() == count_pyramid);
  OMEGA_H_CHECK(quad2pyramid.a2ab.size() == count_quad+1);

  //Transit tests
  //auto tri2vert = mesh->ask_down(Topo_type::triangle, Topo_type::vertex);

/*
  //get the ids of vertices bounding each face
  ent_class_ids[2].reserve(numFaces);
  FIter faces = M_faceIter(m);
  pFace face;
  while ((face = (pFace) FIter_next(faces))) {
    ent_class_ids[2].push_back(classId(face));
  }
  FIter_delete(faces);

  //get the ids of vertices bounding each region
  ent_class_ids[3].reserve(numRegions);
  regions = M_regionIter(m);
  while ((rgn = (pRegion) RIter_next(regions))) {
    ent_class_ids[3].push_back(classId(rgn));
  }
  RIter_delete(regions);

  //flatten the down_adjs and ent_class_ids arrays
  for (Int ent_dim = max_dim; ent_dim >= 0; --ent_dim) {
    Int neev = element_degree(family, ent_dim, VERT);
    LO ndim_ents = static_cast<LO>(down_adjs[ent_dim].size()) / neev;
    HostWrite<LO> host_ev2v(ndim_ents * neev);
    HostWrite<LO> host_class_id(ndim_ents);
    for (i = 0; i < ndim_ents; ++i) {
      for (Int j = 0; j < neev; ++j) {
        host_ev2v[i * neev + j] =
            down_adjs[ent_dim][static_cast<std::size_t>(i * neev + j)];
      }
      host_class_id[i] = ent_class_ids[ent_dim][static_cast<std::size_t>(i)];
    }
    auto eqv2v = Read<LO>(host_ev2v.write());
    classify_equal_order(mesh, ent_dim, eqv2v, host_class_id.write());
  }
  finalize_classification(mesh);
*/
}

}  // end anonymous namespace

/*******************************************************************
 * Below this line, any Simmetrix specific function&API calls is confidential information.
 * Copyright 1997-2019 Simmetrix Inc. All rights reserved. The 
 * Simmetrix specific function&API calls below this line is unpublished work fully protected by the United 
 * States copyright laws and is considered a trade secret belonging 
 * to the copyright holder. Disclosure, use, or reproduction without 
 * the written authorization of Simmetrix Inc. is prohibited. 
 *******************************************************************/ 
void messageHandler(int type, const char *msg)
{
  switch (type) {
  case Sim_InfoMsg:
    cout<<"Info: "<<msg<<endl;
    break;
  case Sim_DebugMsg:
    cout<<"Debug: "<<msg<<endl;
    break;
  case Sim_WarningMsg:
    cout<<"Warning: "<<msg<<endl;
    break;
  case Sim_ErrorMsg:
    cout<<"Error: "<<msg<<endl;
    break;
  }
  return;
}

Mesh read(filesystem::path const& mesh_fname, filesystem::path const& mdl_fname,
    CommPtr comm) {
  SimPartitionedMesh_start(NULL,NULL);
  SimModel_start();
  Sim_readLicenseFile(NULL);
  pNativeModel nm = NULL;
  pProgress p = NULL;
  //printf(" ok1.1 \n");
  pGModel g = GM_load(mdl_fname.c_str(), nm, p);
  //printf(" ok1.2 \n");
  pParMesh sm = PM_load(mesh_fname.c_str(), g, p);
  //printf(" ok1.3 \n");
  auto mesh = Mesh(comm->library());

/*
  pMesh ms;
  ms = M_new(0,0);
  int numVerts = 7;
  double coords[7*3] =  {0.000, 0.000, 0.000,
                         1.000, 0.000, 0.000,
                         0.500, 0.866, 0.000,
			 0.000, 0.000, 1.000, 
                         1.000, 0.000, 1.000,
                         0.500, 0.866, 1.000,
                         0.500, 0.289, 2.000,
			};
  int numElems = 2;
  int elementType[2] = {12, 10};
  int elementData[6+4] = {0,1,2,3,4,5,3,4,5,6};
  M_importFromData(ms, numVerts, coords, numElems,
       elementType, elementData, 0, 0, 0);
  //
  M_write(ms, "/users/joshia5/simmodeler/importDataMesh.sms", 0, 0);
  printf(" ok1.4 \n");
  meshsim::read_internal(ms, &mesh);
  M_release(ms);
*/

  try {  
    Sim_logOn("importData1.log");  // start logging
    MS_init(); // Call before calling Sim_readLicenseFile
    // NOTE: Sim_readLicenseFile() is for internal testing only.  To use,
    // pass in the location of a file containing your keys.  For a release 
    // product, use Sim_registerKey() 
    Sim_readLicenseFile(0);

    // input parameters for the function
    pMesh meshtest; //mesh to load

///*
    // For tet on wedge, hex and pyramid adjacenct to wedge
    int numVerts = 12;
    double coords[12*3] = {0.000, 0.000, 0.000,
                           1.000, 0.000, 0.000,
                           0.500, 0.866, 0.000,
	    	  	   0.000, 0.000, 1.000, 
                           1.000, 0.000, 1.000,
                           0.500, 0.866, 1.000,
                           0.500, 0.289, 1.866,
			   1.000, 1.500, 0.500,
			   0.000, -1.000, 0.000,
			   1.000, -1.000, 0.000,
			   0.000, -1.000, 1.000,
			   1.000, -1.000, 1.000
			  };
    int numElems = 4;
    int elementType[4] = {12, 10, 11, 13};
    int elementData[6+4+5+8] = {0,1,2,3,4,5, 3,4,5,6, 5,4,1,2,7, 8,9,1,0,10,11,4,3};
    pVertex vReturn[12]; // array of created vertices
    pEntity eReturn[4]; // array of created entities
    //
//*/

/*
    //For tet on wedge
    int numVerts = 7;
    double coords[7*3] =  {0.000, 0.000, 0.000,
                           1.000, 0.000, 0.000,
                           0.500, 0.866, 0.000,
	    	  	   0.000, 0.000, 1.000, 
                           1.000, 0.000, 1.000,
                           0.500, 0.866, 1.000,
                           0.500, 0.289, 1.866,
			  };
    int numElems = 2;
    int elementType[2] = {12, 10};
    int elementData[6+4] = {0,1,2,3,4,5,3,4,5,6};
    pVertex vReturn[7]; // array of created vertices
    pEntity eReturn[2]; // array of created entities
*/

/*  For cube as per simmetrix example
    int numVerts = 8; // number of vertices  
    double coords[8*3] = {0.0,0.0,0.0,
                        1.0,0.0,0.0,
                        1.0,0.0,1.0,
                        0.0,0.0,1.0,
                        0.0,1.0,0.0,
                        1.0,1.0,0.0,
                        1.0,1.0,1.0,
                        0.0,1.0,1.0}; 
    int numElems = 12; // number of elements (2 triangles per face - 6 faces)
    // Type of element to be created, 12 triangles
    int elementType[12] = {5,5,5,5,5,5,5,5,5,5,5,5}; 
    // Node list for each element - this array is 3*12 long, for each element's nodes
    int elementData[12*3] = {0,2,3,0,1,2,0,5,4,0,1,5,1,6,5,1,2,6,2,7,6,2,3,7,3,4,7,3,0,4,4,6,7,4,5,6}; 
*/
    SimDiscrete_start(0);  // initialize GeomSim Discrete library
    Sim_setMessageHandler(messageHandler);
    pProgress progress = Progress_new();
    Progress_setDefaultCallback(progress);

    meshtest = M_new(0,0);
    if(M_importFromData(meshtest,numVerts,coords,numElems,
      elementType,elementData,vReturn,eReturn,progress)) { //check for error 
      cerr<<"Error importing mesh data"<<endl;
      M_release(meshtest);
    }

    // create the Discrete model
    pDiscreteModel modeltest = DM_createFromMesh(meshtest, 0, progress);
    if(!modeltest) { //check for error
      cerr<<"Error creating Discrete model from mesh"<<endl;
      M_release(meshtest);
    }

    DM_findEdgesByFaceNormals(modeltest, 0, progress);
    DM_eliminateDanglingEdges(modeltest, progress);
    if(DM_completeTopology(modeltest, progress)) { //check for error
      cerr<<"Error completing Discrete model topology"<<endl;
      M_release(meshtest);
      GM_release(modeltest);
    }

    GM_write(modeltest,"/users/joshia5/simmodeler/Example_4type.smd",0,progress); // save the discrete model
    M_write(meshtest,"/users/joshia5/simmodeler/Example_4type.sms", 0,progress);  // write out the initial mesh data
  
    //meshsim::read_internal(meshtest, &mesh);//use this for user generated mesh
    meshsim::read_internal(sm, &mesh);

    // cleanup
    M_release(meshtest);
    GM_release(modeltest);
    Progress_delete(progress);
    SimDiscrete_stop(0);
    Sim_unregisterAllKeys();
    MS_exit();
    Sim_logOff();

  } catch (pSimError err) {
    cerr<<"SimModSuite error caught:"<<endl;
    cerr<<"  Error code: "<<SimError_code(err)<<endl;
    cerr<<"  Error string: "<<SimError_toString(err)<<endl;
    SimError_delete(err);
  } catch (...) {
    cerr<<"Unhandled exception caught"<<endl;
  }

  M_release(sm);
  GM_release(g);
  SimModel_stop();
  SimPartitionedMesh_stop();
  return mesh;
}

}  // namespace meshsim

}  // end namespace Omega_h
