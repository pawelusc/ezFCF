#ifndef _parallel_approximation_h_
#define _parallel_approximation_h_

/*! \file parallel_approximation.h 
\ingroup METHODS
\brief calculates and stores Franck-Condon factors in parallel approximation

To initialize provide:
1. vector of molStates; ([0]-initial, [1],[2],... --target states)
2. threshold for FCFs to accept (in_fcf_threshold)
3. target state index =1..Nmax for spectral lines assignment (in_targN)

*/

#include "genincludes.h"
#include "mathutil.h"
#include "constants.h"
#include "vibronic_state.h"
#include "vibrational_indexing.h"

#include <vector>

#include "franck_condon.h"

#include "spectrum.h"
#include "spectralpoint.h"

#include "molstate.h"


class Parallel
{
  //! spectrum which stores all the points above the intensity threshold
  Spectrum spectrum;
  int n_atoms;
  int n_molecule_nm;
  int n_active_nm;

  public:
  Parallel(std::vector <MolState>& molStates, std::vector<int>& active_nm, 
      double fcf_threshold, double temperature, 
      int max_n_initial, int max_n_target, 
      bool if_comb_bands, bool if_use_target_nm, bool if_print_fcfs, bool if_web_version, const char* nmoverlapFName, 
      double energy_threshold_initial,  double energy_threshold_target);

  // This is a constructor where Paweł has prepared the trial version for Marty 
  Parallel(std::vector <MolState>& molStates, std::vector<int> nm_active_space, 
      double fcf_threshold, double temperature, 
      std::vector<int> initial_state, int max_n_target, 
      bool if_comb_bands, bool if_use_target_nm, bool if_print_fcfs, 
      bool if_web_version, const char* nmoverlapFName,  // TODO: is the web version still active: Paweł Apr '22
      double energy_threshold_target);

  //! returns the up to date spectrum
  Spectrum& getSpectrum(){return spectrum;};

};

#endif
