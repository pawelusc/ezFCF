#ifndef _normalmode_h_
#define _normalmode_h_

/*! \file normalmode.h
  \brief Normal mode: frequency and 3N displacements (N-number of atoms)
  \ingroup DATA_CLASSES
  */

//ACHTUNG! different constructor from the previous versions -- now one should supply
//         it with number of atoms, NOT nAtoms*CartDim.

#include "genincludes.h"
#include "constants.h"

//Stored in mass-unweighted form and in Angstrom -- this is ugly ...
class NormalMode
{
  // 1D structure AtomNumber*CARTDIM+CoordinateNumber:
  arma::Col<double> displacement;
  double freq;
  int nAtoms;

  public:
  NormalMode(int n, double fr) : displacement(n*CARTDIM, arma::fill::zeros), freq(fr), nAtoms(n) {};
  NormalMode(const NormalMode& other);
  NormalMode& operator=(const NormalMode& other);

  //! Returns frequency
  double& getFreq() { return freq; }
  // 3N displacements (x,y,z for N atoms)
  arma::Col<double>& getDisplacement() { return displacement; }

  // matrix multiplication coordinates*matrix_3x3; "coordinates" matrix is of 3 columns: x,y,z. matrix_3x3 has eigen vectors of the transformation in rows.
  void transformCoordinates(const arma::Mat<double>& matrix_3x3);
  void applyCoordinateThreshold(const double threshold);
  // three rotations around x, y, z by PI/2:
  void rotateX_90deg();
  void rotateY_90deg();
  void rotateZ_90deg();

};
#endif


