template <Int sdim, Int edim>
INLINE Few<Vector<sdim>, edim> simplex_basis(
    Few<Vector<sdim>, edim + 1> p) {
  Few<Vector<sdim>, edim> b;
  for (Int i = 0; i < edim; ++i)
    b[i] = p[i + 1] - p[0];
  return b;
}

INLINE Real triangle_area(Few<Vector<2>, 2> b) {
  return cross(b[0], b[1]) / 2.0;
}

INLINE Real element_size(Few<Vector<2>, 2> b) {
  return triangle_area(b);
}

INLINE Real triangle_area(Few<Vector<3>, 2> b) {
  return norm(cross(b[0], b[1])) / 2.0;
}

INLINE Real tet_volume(Few<Vector<3>, 3> b) {
  return (cross(b[0], b[1]) * b[2]) / 6.0;
}

INLINE Real element_size(Few<Vector<3>, 3> b) {
  return tet_volume(b);
}

template <Int dim>
INLINE Real iso_edge_length(Few<Vector<dim>, 2> p, Real iso) {
  return norm(p[1] - p[0]) / iso;
}

template <Int dim>
DEVICE Real iso_edge_length(Few<LO, 2> v, Reals coords, Reals isos) {
  auto p = gather_vectors<2,dim>(coords, v);
  auto iso = average(gather_scalars<2>(isos, v));
  return iso_edge_length(p, iso);
}

template <Int dim>
INLINE Real metric_edge_length(Few<Vector<dim>, 2> p,
    Matrix<dim,dim> metric) {
  return metric_length(metric, p[1] - p[0]);
}

template <Int dim>
DEVICE Real metric_edge_length(Few<LO, 2> v, Reals coords, Reals metrics) {
  auto p = gather_vectors<2,dim>(coords, v);
  auto metric = average_metrics(gather_symms<2,dim>(metrics, v));
  return metric_edge_length(p, metric);
}

template <Int dim>
struct RealEdgeLengths {
  Reals coords;
  RealEdgeLengths(Mesh const* mesh):
    coords(mesh->coords())
  {}
  DEVICE Real measure(Few<LO, 2> v) const {
    auto p = gather_vectors<2,dim>(coords, v);
    return norm(p[1] - p[0]);
  }
};

template <Int dim>
struct IsoEdgeLengths {
  Reals coords;
  Reals isos;
  IsoEdgeLengths(Mesh const* mesh):
    coords(mesh->coords()),
    isos(mesh->get_array<Real>(VERT, "size"))
  {}
  DEVICE Real measure(Few<LO, 2> v) const {
    return iso_edge_length<dim>(v, coords, isos);
  }
};

template <Int dim>
struct MetricEdgeLengths {
  Reals coords;
  Reals metrics;
  MetricEdgeLengths(Mesh const* mesh):
    coords(mesh->coords()),
    metrics(mesh->get_array<Real>(VERT, "metric"))
  {}
  DEVICE Real measure(Few<LO, 2> v) const {
    return metric_edge_length<dim>(v, coords, metrics);
  }
};

Reals measure_edges_real(Mesh* mesh, LOs a2e);
Reals measure_edges_metric(Mesh* mesh, LOs a2e);
Reals measure_edges_real(Mesh* mesh);
Reals measure_edges_metric(Mesh* mesh);

Reals find_identity_size(Mesh* mesh);

template <Int dim>
INLINE Real real_element_size(Few<Vector<dim>, dim + 1> p) {
  auto b = simplex_basis<dim,dim>(p);
  return element_size(b);
}

struct RealElementSizes {
  Reals coords;
  RealElementSizes(Mesh const* mesh):coords(mesh->coords()) {}
  template <Int neev>
  DEVICE Real measure(Few<LO, neev> v) const {
    auto p = gather_vectors<neev, neev - 1>(coords, v);
    return real_element_size<neev - 1>(p);
  }
};

Reals measure_elements_real(Mesh* mesh);
