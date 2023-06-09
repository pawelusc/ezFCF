#include "parallel_approximation.h"

Parallel::Parallel(std::vector<MolState> &molStates,
                   std::vector<int> &active_nms, double fcf_threshold,
                   double temperature, int max_n_initial, int max_n_target,
                   TheOnlyInitialState initial_vibrational_state,
                   bool if_comb_bands, bool if_use_target_nm,
                   bool if_print_fcfs, bool if_web_version,
                   const std::string nmoverlapFName,
                   double energy_threshold_initial,
                   double energy_threshold_target)
    : iniN(0), n_atoms(molStates[iniN].NAtoms()),
      n_molecule_nm(molStates[iniN].NNormModes()),
      n_active_nm(active_nms.size()) {
  double intens_threshold = fcf_threshold * fcf_threshold;

  bool if_the_only_initial_state = initial_vibrational_state.non_empty();
  std::vector<int> the_only_initial_state = initial_vibrational_state.get_state();

  // An "invalid" occupation number for an excitation in a vibrational state
  // Used in the function `enumerateVibrStates` as an idication that 
  // the state is "empty"
  const int default_value = -1;

  // matrix with FC factors (use the same variable for every target state)
  // max_n_initial stands for maximum allowed excitations in a single mode in the initial state
  // FCFs_tmp(n, m) gives the integral of an overlap of a state with 'n' excitation with 
  // a state of 'm' excitations, i.e, <n|m> 
  arma::Mat<double> FCFs_tmp(max_n_initial+1, max_n_target+1);
  std::vector <arma::Mat<double>> FCFs;

  // matrix with intensities of FC transitions:
  //  FCFs * Boltzmann_factor(E, T) 
  arma::Mat<double> I_tmp (max_n_initial+1, max_n_target+1);
  std::vector <arma::Mat<double>> I;

  // energy position of each transition for a given normal mode; 
  // 1D; ofset by IP; 
  // when add for N dimensions, substract IP from each energy;
  arma::Mat<double> E_position_tmp(max_n_initial + 1, max_n_target + 1);
  std::vector<arma::Mat<double>> E_position;

  // ==========================================================================================
  // Calculate transformation matrix "cartesian->normal mode" coordinates (for each state):
  std::vector <arma::Mat<double>> Cart2Normal_Rs;

  for (int state=0; state<molStates.size(); state++)
  {
    // NOT mass weighted normal modes i.e. (L~)=T^(0.5)*L
    arma::Mat<double> NormModes( CARTDIM * n_atoms, n_molecule_nm, arma::fill::zeros ); 
    // Get L (normal modes in cartesian coordinates mass unweighted (in Angstroms) ):
    for (int j=0; j < n_atoms; j++) 
      for (int i = 0; i < n_molecule_nm; i++) 
        for (int k=0; k < CARTDIM; k++)
          NormModes(j*CARTDIM+k, i) = molStates[state].getNormMode(i).getDisplacement()(j*CARTDIM+k);

    // Make sqrt(T)-matrix (diagonal matrix with sqrt(atomic masses) in
    // cartesian coordinates)
    arma::Mat<double> mass_matrix = molStates[state].getMassMatrix();
    // revert it back to amu as the legacy code didn't use atomic units
    mass_matrix /= AMU_2_ELECTRONMASS;
    arma::Mat<double> SqrtT = arma::sqrt(mass_matrix);

    // Cart->NormalModes transformation matrix R=L^T*sqrt(T)
    // q' = q+d = q+R*(x-x') (for the parallel normal mode approximation)
    // units of d are Angstr*sqrt(amu)
    NormModes = NormModes.t(); 
    NormModes *= SqrtT;

    Cart2Normal_Rs.push_back(NormModes);
  }

  // Initial geometry in cartesian coordinates:
  arma::Col<double> InitialCartCoord(CARTDIM*n_atoms);
  for (int i=0; i<n_atoms; i++)
    for (int k=0; k<CARTDIM; k++)
      InitialCartCoord[i*CARTDIM+k] = molStates[iniN].getAtom(i).Coord(k);
  InitialCartCoord *= ANGSTROM2AU; 

  // initialize SpectralPoint: 
  // create vector of VibrQuantNumbers for the initital and target states 
  // (keep only non-zero qunta, i.e. from the "excite subspace")
  // add subspace mask (the rest of nmodes do not have excitations, 
  // and would not be printed in the spectrum)
  SpectralPoint tmpPoint;
  for (int nm=0; nm<n_active_nm; nm++)
  {
    tmpPoint.getVibrState1().addVibrQuanta(0, active_nms[nm]);
    tmpPoint.getVibrState2().addVibrQuanta(0, active_nms[nm]);
  }

  // reduced mass for FCF calcualtions -- mass weighted coordinates
  double reducedMass=1.0;

  // For each target state, for each normal mode: calculate shift, FCFs, and print spectrum
  for (int targN=1; targN < molStates.size(); targN++)
  {
    std::cout << "===== Target state #" << targN << " =====\n\n"<< std::flush;

    // clear the FC factors and related data
    FCFs.clear();
    I.clear();
    E_position.clear();

    //----------------------------------------------------------------------
    // calculate dQ:

    // Target state geometry in cartesian coordinates:
    arma::Col<double> TargetCartCoord(CARTDIM*n_atoms);
    for (int i=0; i<n_atoms; i++)
      for (int k=0; k <CARTDIM; k++)
        TargetCartCoord[i*CARTDIM+k] = molStates[targN].getAtom(i).Coord(k);
    // input is in angstroms
    TargetCartCoord *= ANGSTROM2AU;

    // Shift of the target state relative to the initial (In cart. coord.)
    arma::Col<double> cartesian_shift = TargetCartCoord - InitialCartCoord;

    // Transform Cart->normal Mode coordinates

    // Geometry differences in normal coordinates
    arma::Col<double> NormModeShift_ini(n_molecule_nm);
    arma::Col<double> NormModeShift_targ(n_molecule_nm);

    NormModeShift_ini = Cart2Normal_Rs[iniN] * cartesian_shift;
    NormModeShift_targ = Cart2Normal_Rs[targN] * cartesian_shift;

    // Rescale it back & print:
    NormModeShift_ini *= AU2ANGSTROM;
    NormModeShift_targ *= AU2ANGSTROM;

    std::cout << "Difference (dQ) between the initial and the target state geometries.\n"
      << "Angstrom*sqrt(amu):\n\n"
      << "normal mode  dQ in initial  dQ in target   frequency   frequency   comments\n"
      << "  number      state coord.  state coord.    initial      target\n\n";
    for (int nm=0; nm<n_molecule_nm; nm++)
    {
      std::cout << "     " << std::fixed << std::setw(3) << nm << "      " 
        << std::setprecision(6) 
        << std::setw(9) <<  NormModeShift_ini[nm] << "       " 
        << std::setw(9) << NormModeShift_targ[nm] << "      " 
        << std::setprecision(2) 
        << std::setw(7) << molStates[iniN].getNormMode(nm).getFreq() << "    " 
        << std::setw(7) << molStates[targN].getNormMode(nm).getFreq() << "\n";
    }
    std::cout<<"\n\n";


    //----------------------------------------------------------------------
    // save the spectrum overlap to a file 
    // (for normal mode reordering tool in the web interface)
    if (if_web_version)
    {
      std::vector <int> nondiagonal_list;
      arma::Mat<double> NMoverlap;
      bool if_overlap_diagonal = molStates[targN].getNormalModeOverlapWithOtherState(molStates[iniN], NMoverlap, nondiagonal_list);

      std::ofstream nmoverlapF;     
      nmoverlapF.open(nmoverlapFName, std::ios::out);
      nmoverlapF << "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n"
        << "<nmoverlap\n  nm_initial=\"" <<n_molecule_nm <<"\"\n  nm_target =\""<<n_molecule_nm << "\">\n\n<nm_order_initial>\n";
      //inital state's nm order -- 0,1,2... -- always!
      for (int nm=0; nm<n_molecule_nm; nm++)
        nmoverlapF << "<oi" << nm << ">"<< molStates[iniN].getNormModeIndex(nm) << "</oi" << nm << ">\n";
      nmoverlapF << "</nm_order_initial>\n\n<frequencies_initial>\n";
      for (int nm=0; nm<n_molecule_nm; nm++)
        nmoverlapF << "<fi" << nm << ">"<< std::fixed << std::setprecision(1) << molStates[iniN].getNormMode(nm).getFreq()<< "</fi" << nm << ">\n";
      nmoverlapF << "</frequencies_initial>\n\n<displacements_initial>\n";
      for (int nm=0; nm<n_molecule_nm; nm++)
        nmoverlapF << "<dqi" << nm << ">"<< std::fixed << std::setprecision(2) << NormModeShift_ini[nm]<< "</dqi" << nm << ">\n";
      nmoverlapF << "</displacements_initial>\n\n<nm_order_target>\n";
      for (int nm=0; nm<n_molecule_nm; nm++)
        nmoverlapF << "<ot" << nm << ">"<< molStates[targN].getNormModeIndex(nm) << "</ot" << nm << ">\n";
      nmoverlapF << "</nm_order_target>\n\n<frequencies_target>\n";
      for (int nm=0; nm<n_molecule_nm; nm++)
        nmoverlapF << "<ft" << nm << ">"<< std::fixed << std::setprecision(1) << molStates[targN].getNormMode(nm).getFreq()<< "</ft" << nm << ">\n";
      nmoverlapF << "</frequencies_target>\n\n<displacements_target>\n";
      for (int nm=0; nm<n_molecule_nm; nm++)
        nmoverlapF << "<dqt" << nm << ">"<< std::fixed << std::setprecision(2) << NormModeShift_targ[nm]<< "</dqt" << nm << ">\n";
      nmoverlapF << "</displacements_target>\n\n<overlap_matrix>\n";
      for (int nmt=0; nmt<n_molecule_nm; nmt++)
      {    
        nmoverlapF << "<row"<<nmt<<">\n";
        for (int nmi=0; nmi<n_molecule_nm; nmi++)
          nmoverlapF << "<c"<<nmi<<">"<< NMoverlap(nmt, nmi)<<"</c"<<nmi<<">";
        nmoverlapF << "\n</row"<<nmt<<">\n";
      }
      nmoverlapF << "</overlap_matrix>\n\n</nmoverlap>";
      nmoverlapF.close();
    }
    //----------------------------------------------------------------------


    if (if_use_target_nm) {
      std::cout<<"TARGET state normal coordinates (displacements dQ) will be used.\n\n"; 
    }
    else {
      std::cout<<"INITIAL state normal coordinates are used.\n\n"; 
    }
    // ----------------------------------------------------------------------

    if (if_print_fcfs)
      std::cout << "Peak positions, Franck-Condon factors, and intensities "
        "along each normal mode:\n\n";

    // for each normal mode of the initial state
    for (int nm = 0; nm < n_molecule_nm; nm++) {
      // TODO: wrap this into functions
      // Calculate the energy positions:
      for (int i = 0; i < max_n_initial + 1; i++) {
        for (int j = 0; j < max_n_target + 1; j++) {
          // energy position of each transition for a given normal mode;
          // 1D; ofset by IP; when add for N dimensions, substract IP from each
          // energy;
          E_position_tmp(i, j)
            =
            - molStates[targN].Energy()
            + (
                molStates[iniN].getNormMode(nm).getFreq() * i
                - molStates[targN].getNormMode(nm).getFreq() * j
              ) * WAVENUMBERS2EV;
        }
      }
      E_position.push_back(E_position_tmp);

      // Calculate the Franck-Condon factors
      if (if_use_target_nm) {
        harmonic_FCf(FCFs_tmp, reducedMass, NormModeShift_targ[nm],
                     molStates[iniN].getNormMode(nm).getFreq(),
                     molStates[targN].getNormMode(nm).getFreq());
      } else {
        harmonic_FCf(FCFs_tmp, reducedMass, NormModeShift_ini[nm],
                     molStates[iniN].getNormMode(nm).getFreq(),
                     molStates[targN].getNormMode(nm).getFreq());
      }

      FCFs.push_back(FCFs_tmp);

      // Calculate the line intensities:
      //   FCF^2 * the temperature distribution in the initial state
      for (int i = 0; i < max_n_initial + 1; i++) {
        // intensity < 10e-44 for non ground states if temperature=0
        double IExponent = 100 * i;
        if (temperature > 0) {
          IExponent = molStates[iniN].getNormMode(nm).getFreq() *
                      WAVENUMBERS2EV * i / (temperature * KELVINS2EV);
          if (IExponent > 100)
            IExponent = 100; // keep the intensity >= 10e-44 == exp(-100)
        }

        for (int j = 0; j < max_n_target + 1; j++)
          I_tmp(i, j) = FCFs_tmp(i, j) * FCFs_tmp(i, j) * exp(-IExponent);
      }
      I.push_back(I_tmp);

      if (if_print_fcfs) {
        std::cout << "NORMAL MODE #" << nm << " (" << std::fixed << std::setw(8)
                  << std::setprecision(2)
                  << molStates[iniN].getNormMode(nm).getFreq() << "cm-1 --> "
                  << molStates[targN].getNormMode(nm).getFreq()
                  << "cm-1, dQ=" << std::setw(6) << std::setprecision(4);
        if (if_use_target_nm)
          std::cout << NormModeShift_targ[nm];
        else
          std::cout << NormModeShift_ini[nm];
        std::cout << ")\n";

        E_position[nm].print("Peak positions, eV");
        FCFs[nm].print("1D Harmonic Franck-Condon factors");
        I[nm].print("Intensities (FCFs^2)*(initial states' termal population)");
      }

    } // end for each normal mode

    //==========================================================================
    // evaluate the overal intensities as all possible products of 1D
    // intensities (including combination bands)
    std::cout << "Maximum number of vibrational excitations:\n"
              << "  " << max_n_initial << " (in the initial state),\n"
              << "  " << max_n_target << " (in each target state).\n\n";

    unsigned long total_combs_ini = 0, total_combs_targ = 0;
    if (if_the_only_initial_state) {
      total_combs_ini = 1;
    } else {
      for (int curr_max_ini = 0; curr_max_ini <= max_n_initial; curr_max_ini++)
        total_combs_ini +=
            nChoosek(curr_max_ini + n_active_nm - 1, n_active_nm - 1);
    }

    for (int curr_max_targ = 0; curr_max_targ <= max_n_target; curr_max_targ++)
      total_combs_targ +=
          nChoosek(curr_max_targ + n_active_nm - 1, n_active_nm - 1);

    std::cout << "Maximum number of combination bands = "
              << total_combs_ini * total_combs_targ
              << "\n   = " << total_combs_ini
              << " (# of vibrational states in the initial electronic state)"
              << "\n   * " << total_combs_targ
              << " (# of vibrational states in the target electronic state).\n"
              << std::endl;

    // find INITIAL states with up to 'max_n_initial' vibrational quanta and
    // with energy below 'energy_threshold_initial':
    std::cout << "A set of initial vibrational states is being created...\n";
    if (energy_threshold_initial < std::numeric_limits<double>::max()) {
      std::cout << "  Energy threshold = " << std::fixed
                << energy_threshold_initial << " eV ("
                << energy_threshold_initial / KELVINS2EV << " K).\n"
                << std::flush;
    } else {
      std::cout << "  Energy threshold is not specified in the input.\n"
                << std::flush;
    }

    // Container for the set of inital vibrational states (for each electronic
    // state)
    // -- after energy threshold applied
    // Every vibrational state is of `n_molecule_nm` length, i.e., full space
    std::vector<std::vector<int>> selected_states_ini;

    // Containers for a single vibrational states in the "excite subspace"
    std::vector<int> state_ini_subspace(n_active_nm, default_value);
    for (int curr_max_ini = 0; curr_max_ini <= max_n_initial; curr_max_ini++) {
      while (enumerateVibrStates(n_active_nm, curr_max_ini, state_ini_subspace,
                                 if_comb_bands)) {
        // state_ini is a full-space version of the 'state_ini_subspace'
        // initlize the state to zero excitation
        std::vector<int> state_ini(n_molecule_nm, 0);
        // copy indexes from the subspace state_ini_subspace into
        // the full space state_ini (the rest stays=0):
        for (int nm = 0; nm < n_active_nm; nm++)
          state_ini[active_nms[nm]] = state_ini_subspace[nm];

        double energy = 0;
        for (int nm = 0; nm < n_molecule_nm; nm++)
          energy +=
              E_position[nm](state_ini[nm], 0) + molStates[targN].Energy();

        if (energy < energy_threshold_initial) {
          selected_states_ini.push_back(state_ini);
        }
      }
      // reset the initial state's vibration "population"
      // see the `enumerateVibrStates` function the the top of the loop
      state_ini_subspace[0] = default_value;
    }
    std::cout << "  " << selected_states_ini.size()
              << " vibrational states below the energy threshold.\n\n"
              << std::flush;

    // TODO: This is a duplicate of the above code. It should be a function.
    // Paweł Apr '22 find TARGET states with up to 'max_n_target' vibrational
    // quanta and with energy below 'energy_threshold_target':
    std::cout << "A set of target vibrational states is being created...\n";
    if (energy_threshold_target < std::numeric_limits<double>::max()) {
      std::cout << "  Energy threshold = " << std::fixed
                << energy_threshold_target << " eV ("
                << energy_threshold_target / WAVENUMBERS2EV << " cm-1).\n"
                << std::flush;
    } else {
      std::cout << "  Energy threshold is not specified in the input.\n"
                << std::flush;
    }

    std::vector<std::vector<int>> selected_states_targ;
    std::vector<int> state_targ_subspace(n_active_nm, default_value);
    selected_states_targ.clear();
    for (int curr_max_targ = 0; curr_max_targ <= max_n_target;
         curr_max_targ++) {
      while (enumerateVibrStates(n_active_nm, curr_max_targ,
                                 state_targ_subspace, if_comb_bands)) {
        std::vector<int> state_targ(n_molecule_nm, 0);
        // copy indexes from the subspace state_targ_subspace into the full
        // space state_targ (the rest stays=0):
        for (int nm = 0; nm < n_active_nm; nm++)
          state_targ[active_nms[nm]] = state_targ_subspace[nm];

        double energy = 0;

        for (int nm = 0; nm < n_molecule_nm; nm++)
          // threshold -- energy above the ground state:
          energy +=
              E_position[nm](0, state_targ[nm]) + molStates[targN].Energy();

        if (-energy < energy_threshold_target) {
          selected_states_targ.push_back(state_targ);
        }
      }
      // reset the target state's vibration "population"
      state_targ_subspace[0] = -1;
    }

    std::cout << "  " << selected_states_targ.size()
              << " vibrational states below the energy threshold.\n\n";

    if (if_the_only_initial_state) {
      selected_states_ini.clear();
      selected_states_ini.push_back(the_only_initial_state);
      std::cout
          << " The only initial state active:\n"
          << "    The initial state contains only ONE vibrational state.\n"
          << std::endl;
    }

    std::cout << "Total number of combination bands with thresholds applied: "
              << selected_states_ini.size() * selected_states_targ.size()
              << ".\n\n"
              << std::flush;

    std::cout << "Intensities of combination bands are being calculated..."
              << std::flush;

    for (const auto &ini_state : selected_states_ini)
      for (const auto &targ_state : selected_states_targ) {
        double intens = 1.0;
        double FCF = 1.0;
        double energy = -molStates[targN].Energy();
        double E_prime_prime = 0.0;

        for (int nm = 0; nm < n_molecule_nm; nm++) {
          int ini_occupation = ini_state[nm];
          int targ_occupation = targ_state[nm];
          double ex_energy = molStates[targN].Energy();

          intens *= I[nm](ini_occupation, targ_occupation);
          FCF *= FCFs[nm](ini_occupation, targ_occupation);

          // [ cancell the IE in each energy, which is stupid but inexpensive;
          // probably should be eliminated ]
          energy += E_position[nm](ini_occupation, targ_occupation) + ex_energy;
          E_prime_prime += E_position[nm](ini_occupation, 0) + ex_energy;
        }

        // add the point to the spectrum if its intensity is above the threshold
        if (intens > intens_threshold) {
          tmpPoint.set_intensity(intens);
          tmpPoint.set_energy(energy);
          tmpPoint.getE_prime_prime() = E_prime_prime;
          tmpPoint.set_FCF(FCF);
          tmpPoint.getVibrState1().reset();
          tmpPoint.getVibrState1().setElStateIndex(0);
          tmpPoint.getVibrState2().reset();
          tmpPoint.getVibrState2().setElStateIndex(targN);
          for (int nm = 0; nm < n_active_nm; nm++) {
            tmpPoint.getVibrState1().setVibrQuanta(nm,
                                                   ini_state[active_nms[nm]]);
            tmpPoint.getVibrState2().setVibrQuanta(nm,
                                                   targ_state[active_nms[nm]]);
          }
          spectrum.AddSpectralPoint(tmpPoint);
        }
      }
    std::cout << "done.\n" << std::flush;
  } // end for each target state
}
