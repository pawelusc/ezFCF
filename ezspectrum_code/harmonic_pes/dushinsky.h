#ifndef _dushinsky_h_
#define _dushinsky_h_

/*! \file dushinsky.h
  \ingroup METHODS
  \brief calculates and stores full dimentional Franck-Condon factors (i.e.
  including Dushinsky rotations)

  the notation and equations are from [Berger et al. JPCA 102:7157(1998)]
  To initialize provide:
  1. vector of molStates; ([0]-initial, [1],[2],... --target states)
  2. vector of int -- list of normal modes in the subspace;
  3. threshold for FCFs to accept (in_fcf_threshold)
  4. target state index =1..Nmax for spectral lines assignment (in_targN)

*/

#include "constants.h"
#include "do_not_excite_subspace.h"
#include "dushinsky_parameters.h"
#include "energy_thresholds.h"
#include "genincludes.h"
#include "job_parameters.h"
#include "mathutil.h"
#include "molstate.h"
#include "single_excitations.h"
#include "spectrum.h"
#include "the_only_initial_state.h"
#include "vibrational_indexing.h"
#include "vibronic_state.h"

/* Class for calculations of FCFs that include the effects of Duschinsky
 * rotation.
 *
 * IMPORTANT!
 * All matrices and zero-zero integral are evaluated for the FULL space of
 * normal modes (3N-5/6 normal modes). Then, the space is shrinked to
 * the space described by ```no_excite_subspace.get_active_subspace()```, so
 * excitations will be added only to those normal modes.
 *
 * Notation and equations are from [Berger et al. JPCA 102:7157(1998)]
 * */
class Dushinsky {
  //! number of normal modes in the molecule
  //! n_mol_nms = 3N-5/6 
  const int n_mol_nms;
  //! number of normal modes in the active space: 
  //! n_active_nms = 3N-5/6 - size(do_not_excite_subspace)
  const int n_active_nms;
  //! target state index
  const int targN;
  //! frequently used matrices in the "excite subspace"
  arma::Mat<double> tpmo, tqmo, tr;
  //! frequently used matrices in the full space; requred for single excitation
  //! (outside the "excite subspace") and hot bands recursive calculations;
  arma::Mat<double> tpmo_full, tqmo_full, tr_full;
  arma::Col<double> ompd, ompd_full, rd, rd_full;
  //! K' -- maximum stored layer (maximum total number of quanta in the target
  //! state):
  int Kp_max_saved;
  //! K' -- maximum evaluated layer (maximum total number of quanta in the
  //! target state):
  int Kp_max;
  //! <zero|zero> integral -- initital one in the recurrent procedure
  double zero_zero;
  //! threshold for FCFs to be included in the spectrum:
  double fcf_threshold;

  //! initial('constant' for the no hot bands case) and target(variable) states:
  VibronicState state0, state;

  //! TODO: no need for memory handling
  //!
  //! "layer" #K' (K'=0...Kp_max)contains all states with the total number of
  //! excitations = K' in the target state only
  //! each layer is a linear vector; to get between "vector's index" and
  //! "vibrational state" functions convVibrState2Index() and convIndex2VibrState
  //! are used
  std::vector<std::vector<double> *> layers;

  //! spectrum which stores all the points
  Spectrum spectrum;

  // array of sqrt: sqrtArray[i]=sqrt(i);
  double *sqrtArray;

  void evaluate_higher_levels(const DushinskyParameters &dush_parameters);

  //! Just like evalNextLayer, but the initial state comes from
  //! the_only_initial_state node. Start with Kp = 0 and increment to Kp_max;
  //! Also updates the spectrum and returns number of points below the
  //! threashold added;
  void add_the_only_intial_state_transitions(
      const DushinskyParameters &dush_config,
      const TheOnlyInitialState &the_only_initial_state, const int iniN);

public:
  Dushinsky(std::vector<MolState> &molStates, const int in_targN,
            const EnergyThresholds &thresholds,
            const DushinskyParameters &dush_parameters,
            const JobParameters &job_parameters,
            const DoNotExcite &no_excite_subspace,
            const TheOnlyInitialState &the_only_initial_state,
            SingleExcitations &single_excitations);
  ~Dushinsky();

  void old_constructor(std::vector<MolState> &molStates,
                       const DushinskyParameters &dush_parameters,
                       const JobParameters &job_parameters,
                       const DoNotExcite &no_excite_subspace);

  //! K'=0 is zero_zero; so it starts with K'=1 and increments Kp_max; Also
  //! updates the spectrum and returns number of points below the threahold
  //! added;
  int evalNextLayer(const bool if_save = true);

  //! It's needed to start over the iterations over layers in
  //! "the_only_initial_state"
  void reset_Kp_max() { Kp_max = 0; }

  //! add hot bands to the spectrum
  void addHotBands(std::vector<MolState> &molStates,
                   const DoNotExcite &no_excite_subspace,
                   const JobParameters &job_config,
                   const DushinskyParameters &dush_config,
                   const EnergyThresholds &thresholds);

  //! if total number excitations in the stete is larger than Kp_max (stored),
  //! evaluates FCF recursively;
  double evalSingleFCF(VibronicState &state_ini, int K,
                       VibronicState &state_targ, int Kp);
  //! a separate copy of evalSingleFCF() for faster execution; if there are
  //! excitations in normal modes outside "excite subspace", than use this
  //! one;
  double evalSingleFCF_full_space(VibronicState &state_ini, int K,
                                  VibronicState &state_targ, int Kp);

  //! returns the up to date spectrum
  Spectrum &getSpectrum() { return spectrum; };

  //! add a single point to the spectrum
  void addSpectralPoint(const double fcf, VibronicState state_ini,
                        VibronicState state_targ);
  //! returns Kp_max
  int getLmax() { return Kp_max; };
  //! Prints estimated sizes of each layer up to Kp_max (works before the
  //! memory is actually allocated)
  void printLayersSizes(const DushinskyParameters &dush_parameters,
                        const DoNotExcite &no_excite_subspace);

  // goes over all single excitations finds FCFs and adds them to the
  // spectrum
  void add_single_excitations(SingleExcitations &storage);

  // Set energies (peak positions) and intensities (by applying Boltzmann
  // distribution) for every point in the spectrum. Applies intensity cutoff
  // to decide if the state is going to be printed in the spectrum.
  //
  // TODO: this should be tied to the Dushinsky::addSpectralPoint or
  // Spectrum::addSpectralPoint functions or even earlier -- when the spectral
  // points are created they should be fully formed before being added to the
  // vector of points.
  void
  updated_intensities_and_positions(const JobParameters &job_config,
                                    const DushinskyParameters &dush_parameters,
                                    const EnergyThresholds &thresholds,
                                    MolState &initial_el_st,
                                    MolState &target_el_st);
};

#endif
