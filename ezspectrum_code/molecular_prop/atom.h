#ifndef _atom_h_
#define _atom_h_

/*! \file atom.h
  \brief Stores x,y,z mass, and atom name
  \ingroup DATA_CLASSES
  */

#include "constants.h"
#include "genincludes.h"

//! atom: stores x,y,z mass, and atom name
class Atom {
  arma::Col<double> coord = arma::Col<double>(3, arma::fill::zeros);
  double mass;
  std::string name;

public:
  // backwards comp:
  double &Mass() { return mass; }
  double Mass() const { return mass; }
  double &getMass() { return mass; }

  std::string &Name() { return name; }
  std::string &getName() { return name; }

  double &Coord(int i) { return coord[i]; }
  double Coord(int i) const { return coord[i]; }
  double &getCoord(int i) { return coord[i]; }
  double getCoord(int i) const { return coord[i]; }

  double getR(); // distance from the origin
  double getCoordMass(const int axis);
  // Shifts coordinate's origin by "vector"
  void shiftCoordinates(arma::Col<double> &vector);
  // matrix multiolication coordinates*matrix_3x3; "coordinates" matrix is of 3
  // columns: x,y,z. matrix_3x3 has eigen vectors of the transformation in rows.
  void transformCoordinates(const arma::Mat<double> &matrix_3x3);
  void applyCoordinateThreshold(const double threshold);
  // three rotations around x, y, z by PI/2:
  void rotateX_90deg();
  void rotateY_90deg();
  void rotateZ_90deg();
};

#endif
